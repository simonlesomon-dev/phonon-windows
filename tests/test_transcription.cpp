// Integration test: transcribes a French WAV through the full pipeline.
// Requirements:
//   - OpenVINO IR models converted from parakeet-tdt-0.6b-v3
//   - env PHONON_MODEL_DIR pointing to the model directory
//   - env PHONON_TEST_WAV pointing to a 16 kHz mono WAV (optional,
//     defaults to synthetic tone -> expects empty output)
#include "transcriber.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

using namespace phonon;

static bool readWav16kMono(const std::string& path,
                           std::vector<float>& pcm) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    char hdr[44];
    f.read(hdr, 44);
    if (!f || memcmp(hdr, "RIFF", 4) != 0) return false;
    // Naive parser: find fmt + data chunks.
    uint32_t sampleRate = 16000, channels = 1, bits = 16;
    f.seekg(12);
    while (f.good()) {
        char id[5] = {};
        uint32_t sz = 0;
        f.read(id, 4);
        f.read((char*)&sz, 4);
        if (!f.good()) break;
        if (memcmp(id, "fmt ", 4) == 0 && sz >= 16) {
            struct { uint16_t fmt, ch; uint32_t rate, br; uint16_t align, bits; } h{};
            f.read((char*)&h, sizeof(h));
            sampleRate = h.rate; channels = h.ch; bits = h.bits;
            if (sz > sizeof(h)) f.seekg(sz - sizeof(h), std::ios::cur);
        } else if (memcmp(id, "data", 4) == 0) {
            std::vector<char> raw(sz ? sz : 1);
            f.read(raw.data(), sz);
            size_t nSamples = size_t(f.gcount()) / (bits / 8);
            pcm.resize(nSamples);
            if (bits == 16) {
                const int16_t* s = (const int16_t*)raw.data();
                for (size_t i = 0; i < nSamples; ++i)
                    pcm[i] = s[i] / 32768.0f;
                // Downmix naive (assume mono or interleaved).
                if (channels > 1) {
                    for (size_t i = 0; i < nSamples / channels; ++i) {
                        float m = 0;
                        for (int c = 0; c < int(channels); ++c)
                            m += s[i * channels + c];
                        pcm[i] = m / channels;
                    }
                    pcm.resize(nSamples / channels);
                }
            } else if (bits == 32) {
                const float* s = (const float*)raw.data();
                pcm.assign(s, s + nSamples);
            }
            return true;
        } else {
            f.seekg(sz, std::ios::cur);
        }
    }
    return false;
}

int main() {
    const char* modelDir = getenv("PHONON_MODEL_DIR");
    if (!modelDir || !*modelDir) {
        std::printf("SKIP: PHONON_MODEL_DIR non défini\n");
        return 0;
    }

    std::string device = Transcriber::pickDevice();
    std::printf("Périphérique d'inférence: %s\n", device.c_str());

    Transcriber t;
    Transcriber::Config cfg;
    cfg.modelDir = modelDir;
    cfg.device = device;
    std::string err;
    if (!t.init(cfg, err)) {
        std::printf("FAIL init: %s\n", err.c_str());
        return 1;
    }

    std::vector<float> pcm;
    if (const char* wav = getenv("PHONON_TEST_WAV"); wav && *wav) {
        if (!readWav16kMono(wav, pcm) || pcm.empty()) {
            std::printf("FAIL lecture WAV %s\n", wav);
            return 1;
        }
        std::string text = t.transcribe(pcm);
        std::printf("Transcription (%zu échantillons): «%s»\n",
                    pcm.size(), text.c_str());
        if (text.empty()) {
            std::printf("FAIL transcription vide\n");
            return 1;
        }
        std::printf("OK\n");
        return 0;
    }

    // No WAV provided: sanity check on silence -> should not crash.
    pcm.assign(size_t(16000) * 2, 0.0f);
    t.transcribe(pcm);
    std::printf("OK (test silencieux)\n");
    return 0;
}
