#pragma once
#include <string>
#include <vector>
#include <memory>

namespace ov {
class Core;
class CompiledModel;
}

namespace phonon {

class MelFrontend;

// Runs Parakeet TDT v3 (OpenVINO IR) with greedy TDT decoding.
// Expected model directory layout:
//   encoder.xml/.bin  decoder.xml/.bin  joint.xml/.bin  tokens.txt
class Transcriber {
public:
    struct Config {
        std::string modelDir;
        std::string device;      // "NPU", "GPU" or "CPU"
        int maxDurationSec = 60; // hard cap per utterance
    };

    Transcriber();
    ~Transcriber();

    // Picks NPU -> GPU -> CPU among available OpenVINO devices.
    static std::string pickDevice();

    bool init(const Config& cfg, std::string& error);
    // pcm16k: mono 16 kHz floats. Returns recognized text (may be empty).
    std::string transcribe(const std::vector<float>& pcm16k);

private:
    struct Impl;
    Impl* impl_ = nullptr;
    std::unique_ptr<MelFrontend> mel_;

    std::vector<std::string> tokens_;
    int blankId_ = -1;
    Config cfg_;
};

} // namespace phonon
