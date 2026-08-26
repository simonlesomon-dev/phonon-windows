// Lightweight assertion-based unit tests (no framework dependency).
#include "resampler.h"
#include "normalizer.h"
#include "mel_frontend.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

using namespace phonon;

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        ++checks;                                                          \
        if (!(cond)) {                                                     \
            ++failures;                                                    \
            std::printf("FAIL %s:%d : %s\n", __FILE__, __LINE__, #cond);   \
        }                                                                  \
    } while (0)

static void testResamplerPassthrough() {
    // Already 16 kHz mono -> ratio 1.
    Resampler r(16000, 1);
    std::vector<float> in{0.0f, 1.0f, 0.0f, -1.0f};
    std::vector<float> out;
    r.process(in.data(), in.size(), out);
    CHECK(out.size() >= 3);
}

static void testResamplerDownmixAndRate() {
    // 48 kHz stereo -> 16 kHz mono: expect roughly N/3 frames out.
    Resampler r(48000, 2);
    std::vector<float> in(48000 * 2 * 2); // 2 seconds
    for (size_t i = 0; i < in.size(); i += 2) {
        in[i] = 1.0f;
        in[i + 1] = -1.0f;
    }
    std::vector<float> out;
    r.process(in.data(), 48000 * 2, out);
    r.flush(out);
    CHECK(out.size() > 31000 && out.size() < 33000);
    float maxv = 0;
    for (float v : out) maxv = std::max(maxv, std::fabs(v));
    CHECK(maxv < 1e-6f); // stereo cancel -> mono near zero
}

static void testResamplerSine() {
    const double pi = 3.14159265358979323846;
    Resampler r(48000, 1);
    std::vector<float> in(48000);
    for (int i = 0; i < 48000; ++i)
        in[i] = float(std::sin(2 * pi * 440.0 * i / 48000.0));
    std::vector<float> out;
    r.process(in.data(), in.size(), out);
    r.flush(out);
    CHECK(out.size() > 15000);
    float rms = 0;
    for (float v : out) rms += v * v;
    rms = std::sqrt(rms / out.size());
    CHECK(rms > 0.4f && rms < 0.8f);
}

static void testNormalizer() {
    CHECK(normalizeFrenchWide(L"bonjour   le  monde") ==
          L"Bonjour le monde");
    CHECK(normalizeFrenchWide(L"ça va ? oui !") == L"Ça va ? Oui !");
    CHECK(normalizeFrenchWide(L"attends :voici.") == L"Attends : voici.");
    CHECK(normalizeFrench("hello") == "Hello");
    CHECK(normalizeFrench("") == "");
}

static void testMelShapes() {
    // Nécessite frontend.bin généré (scripts/make_frontend_bin.py).
    const char* bin = getenv("PHONON_FRONTEND_BIN");
    std::string path = bin && *bin ? bin : "../models/parakeet-v3/frontend.bin";
    MelFrontend mel;
    if (!mel.load(path)) {
        std::printf("SKIP mel: %s introuvable\n", path.c_str());
        return;
    }
    std::vector<float> pcm(MelFrontend::kSampleRate); // 1 s de silence
    auto feats = mel.compute(pcm.data(), pcm.size());
    size_t frames = feats.size() / MelFrontend::kMels;
    CHECK(frames >= 99 && frames <= 101); // ~100 frames/s
    bool finite = true;
    for (float v : feats)
        if (!std::isfinite(v)) { finite = false; break; }
    CHECK(finite);
}

int main() {
    testResamplerPassthrough();
    testResamplerDownmixAndRate();
    testResamplerSine();
    testNormalizer();
    testMelShapes();
    std::printf("%d/%d vérifications OK\n", checks - failures, checks);
    return failures == 0 ? 0 : 1;
}
