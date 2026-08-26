#pragma once
#include <cstddef>
#include <vector>

namespace phonon {

// Linear-interpolation resampler with channel downmix to mono.
class Resampler {
public:
    // inRate/inChannels: source format. Output is always 1ch @ 16 kHz.
    Resampler(int inRate, int inChannels);

    // Consumes interleaved input frames, appends 16 kHz mono output.
    void process(const float* interleaved, size_t frameCount,
                 std::vector<float>& out16k);
    void flush(std::vector<float>& out16k);

    double ratio() const { return ratio_; }

    static constexpr int kOutRate = 16000;

private:
    double ratio_;       // outRate / inRate
    double pos_ = 0.0;   // fractional read position (in input frames)
    bool havePrev_ = false;
    double prev_[8] = {};   // per-channel previous sample
    double cur_[8] = {};    // per-channel current sample
    int channels_;
};

} // namespace phonon
