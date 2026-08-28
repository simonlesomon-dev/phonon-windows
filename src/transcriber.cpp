#include "transcriber.h"
#include "mel_frontend.h"
#include "normalizer.h"

#include <openvino/openvino.hpp>
#include <algorithm>
#include <cstring>
#include <fstream>

namespace phonon {

namespace {
constexpr int kVocabSize = 8193;   // y compris <blk> en dernière position
constexpr int kNumDurations = 5;   // sauts TDT possibles : 0..4
constexpr int kEncDim = 1024;
constexpr int kMaxTokensPerStep = 10;
}

struct Transcriber::Impl {
    ov::Core core;
    ov::CompiledModel encComp, jointComp;
    ov::InferRequest encReq, jointReq;

    // État LSTM du prédicteur [2,1,640].
    ov::Tensor state1In, state2In;
    ov::Tensor targetsT, targetLenT;
};

Transcriber::Transcriber() : impl_(new Impl()), mel_(new MelFrontend()) {}
Transcriber::~Transcriber() { delete impl_; }

std::string Transcriber::pickDevice() {
    try {
        ov::Core core;
        auto devices = core.get_available_devices();
        for (const char* pref : {"NPU", "GPU", "CPU"})
            for (const auto& d : devices)
                if (d.rfind(pref, 0) == 0) return pref;
    } catch (...) {}
    return "CPU";
}

static std::string pathOf(const std::string& dir, const char* name) {
#ifdef _WIN32
    return dir + "\\" + name;
#else
    return dir + "/" + name;
#endif
}

bool Transcriber::init(const Config& cfg, std::string& error) {
    cfg_ = cfg;

    // Give a useful error before OpenVINO receives a missing path. Its
    // "file_exists(path)" exception otherwise hides which model asset is
    // missing behind an internal front-end source location.
    for (const char* name : {"encoder.xml", "encoder.bin",
                             "decoder_joint.xml", "decoder_joint.bin",
                             "frontend.bin", "tokens.txt"}) {
        std::ifstream f(pathOf(cfg.modelDir, name), std::ios::binary);
        if (!f) {
            error = "Fichier modèle manquant: " + pathOf(cfg.modelDir, name);
            return false;
        }
    }

    try {
        auto encModel =
            impl_->core.read_model(pathOf(cfg.modelDir, "encoder.xml"));
        // Entrées statiques pour un graphe compatible NPU.
        encModel->reshape({
            {"audio_signal", {{1, 128, -1}}},
            {"length",       {{1}}}
        });

        // Let the current OpenVINO NPU plugin select its compiler and its
        // default efficiency-oriented mode. Forcing the legacy DRIVER
        // compiler/max-tiles settings can block compilation on recent NPU
        // drivers.
        ov::AnyMap encOpts;
        impl_->encComp =
            impl_->core.compile_model(encModel, cfg.device, encOpts);
        impl_->encReq = impl_->encComp.create_infer_request();

        // Le decoder_joint (petit LSTM + joint) tourne sur CPU.
        auto jointModel =
            impl_->core.read_model(pathOf(cfg.modelDir, "decoder_joint.xml"));
        jointModel->reshape({
            {"encoder_outputs", {{1, kEncDim, 1}}},
            {"targets",         {{1, 1}}},
            {"target_length",   {{1}}},
            {"input_states_1",  {{2, 1, 640}}},
            {"input_states_2",  {{2, 1, 640}}}
        });
        impl_->jointComp = impl_->core.compile_model(jointModel, "CPU");
        impl_->jointReq = impl_->jointComp.create_infer_request();

        impl_->state1In = ov::Tensor(ov::element::f32, {2, 1, 640});
        impl_->state2In = ov::Tensor(ov::element::f32, {2, 1, 640});
        std::memset(impl_->state1In.data(), 0, sizeof(float) * 2 * 1 * 640);
        std::memset(impl_->state2In.data(), 0, sizeof(float) * 2 * 1 * 640);
        impl_->targetsT = ov::Tensor(ov::element::i32, {1, 1});
        impl_->targetLenT = ov::Tensor(ov::element::i32, {1});
        *impl_->targetLenT.data<std::int32_t>() = 1;
        impl_->jointReq.set_input_tensor(1, impl_->targetsT);
        impl_->jointReq.set_input_tensor(2, impl_->targetLenT);
        impl_->jointReq.set_input_tensor(3, impl_->state1In);
        impl_->jointReq.set_input_tensor(4, impl_->state2In);
    } catch (const std::exception& e) {
        error = std::string("Chargement du modèle: ") + e.what();
        return false;
    }

    if (!mel_->load(pathOf(cfg.modelDir, "frontend.bin"))) {
        error = "frontend.bin introuvable";
        return false;
    }

    // tokens.txt : une pièce par ligne ; blank = dernier identifiant.
    std::ifstream tf(pathOf(cfg.modelDir, "tokens.txt"), std::ios::binary);
    if (!tf) { error = "tokens.txt introuvable"; return false; }
    tokens_.clear();
    std::string line;
    while (std::getline(tf, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        tokens_.push_back(line);
    }
    blankId_ = int(tokens_.size()) - 1;
    return true;
}

std::string Transcriber::transcribe(const std::vector<float>& pcm16k) {
    if (pcm16k.size() < size_t(MelFrontend::kSampleRate) / 5 || !impl_)
        return {}; // ignore <200 ms

    // ---- Frontend mel ----
    std::vector<float> feats = mel_->compute(pcm16k.data(), pcm16k.size());
    const int T = int(feats.size() / MelFrontend::kMels);
    if (T <= 0 || !impl_) return {};

    // ---- Encoder : entrée [1,128,T] channels-first ----
    std::vector<float> encIn(feats.size());
    for (int t = 0; t < T; ++t)
        for (int m = 0; m < MelFrontend::kMels; ++m)
            encIn[size_t(m) * T + t] =
                feats[size_t(t) * MelFrontend::kMels + m];

    try {
        ov::Shape shape{1, size_t(MelFrontend::kMels), size_t(T)};
        impl_->encReq.set_input_tensor(
            0, ov::Tensor(ov::element::f32, shape, encIn.data()));
        *static_cast<std::int64_t*>(
            impl_->encReq.get_input_tensor(1).data()) = T;
        impl_->encReq.infer();
    } catch (...) {
        return {};
    }

    // Sortie encoder [1,1024,T'].
    ov::Tensor encOut = impl_->encReq.get_output_tensor(0);
    const auto eshape = encOut.get_shape();
    const int Te = std::min<int>(
        int(eshape[2]),
        int(*impl_->encReq.get_output_tensor(1).data<std::int64_t>()));
    if (Te <= 0) return {};
    const float* enc = encOut.data<const float>();

    // ---- Décodage TDT glouton (algorithme de référence onnx-asr) ----
    std::memset(impl_->state1In.data(), 0, sizeof(float) * 2 * 1 * 640);
    std::memset(impl_->state2In.data(), 0, sizeof(float) * 2 * 1 * 640);

    const std::int32_t blank32 = blankId_;
    std::memcpy(impl_->targetsT.data(), &blank32, sizeof(std::int32_t));

    // Le joint prend une frame [1,1024,1] : copie contiguë depuis la
    // sortie encoder (layout channels-first).
    ov::Tensor frameT(ov::element::f32, {1, size_t(kEncDim), 1});

    std::vector<int> emitted;
    emitted.reserve(size_t(cfg_.maxDurationSec) * 25);
    int t = 0;
    int nEmitted = 0;

    while (t < Te &&
           emitted.size() < size_t(cfg_.maxDurationSec) * 25) {
        std::memcpy(frameT.data(), enc + size_t(t) * kEncDim,
                    sizeof(float) * kEncDim);
        impl_->jointReq.set_input_tensor(0, frameT);
        try {
            impl_->jointReq.infer();
        } catch (...) {
            break;
        }

        const float* out =
            impl_->jointReq.get_output_tensor(0).data<const float>();
        const float* logits = out;                // [8193]
        const float* durs = out + kVocabSize;     // [5]

        int token = 0;
        for (int i = 1; i < kVocabSize; ++i)
            if (logits[i] > logits[token]) token = i;

        int step = 0;
        for (int i = 1; i < kNumDurations; ++i)
            if (durs[i] > durs[step]) step = i;

        if (token != blankId_) {
            std::memcpy(impl_->state1In.data(),
                        impl_->jointReq.get_output_tensor(2).data(),
                        sizeof(float) * 2 * 1 * 640);
            std::memcpy(impl_->state2In.data(),
                        impl_->jointReq.get_output_tensor(3).data(),
                        sizeof(float) * 2 * 1 * 640);
            std::memcpy(impl_->targetsT.data(), &token,
                        sizeof(std::int32_t));
            emitted.push_back(token);
            ++nEmitted;
        }

        if (step > 0) {
            t += step;
            nEmitted = 0;
        } else if (token == blankId_ || nEmitted == kMaxTokensPerStep) {
            ++t;
            nEmitted = 0;
        }
    }

    // ---- Détokonisation (SentencePiece : ▁ = espace) ----
    constexpr char kSp[] = "\xe2\x96\x81";
    std::string text;
    for (int id : emitted) {
        if (id < 0 || id >= int(tokens_.size())) continue;
        std::string tk = tokens_[size_t(id)];
        size_t p;
        while ((p = tk.find(kSp)) != std::string::npos)
            tk.replace(p, 3, " ");
        text += tk;
    }
    return normalizeFrench(text);
}

} // namespace phonon
