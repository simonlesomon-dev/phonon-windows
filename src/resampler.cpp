#include "resampler.h"
#include <cmath>
#include <algorithm>

namespace phonon {

Resampler::Resampler(int inRate, int inChannels)
    : ratio_(double(kOutRate) / double(inRate)), channels_(inChannels) {}

void Resampler::process(const float* interleaved, size_t frameCount,
                        std::vector<float>& out) {
    if (frameCount == 0) return;

    size_t written = 0;
    // Reserve upper bound.
    out.reserve(out.size() + size_t(double(frameCount) * ratio_) + 4);

    for (size_t f = 0; f < frameCount; ++f) {
        // Load current frame (downmix to mono average).
        double cur = 0.0;
        for (int c = 0; c < channels_; ++c)
            cur += interleaved[f * channels_ + c];
        cur /= channels_;

        while (pos_ <= 0.0) {
            // Emit interpolated sample between prev_ and cur.
            double t = 1.0 + pos_; // pos_ in (-1, 0]; weight of cur
            double v = havePrev_
                ? prev_[0] * (1.0 - t) + cur * t
                : cur;
            out.push_back(float(v));
            ++written;
            pos_ += 1.0 / ratio_;
        }
        pos_ -= 1.0;
        prev_[0] = cur;
        havePrev_ = true;
    }
}

void Resampler::flush(std::vector<float>& out) {
    // Hold last sample to emit remaining output positions.
    if (!havePrev_) return;
    while (pos_ > -1e9 && pos_ >= 1.0 / ratio_) {
        out.push_back(float(prev_[0]));
        pos_ -= 1.0 / ratio_;
    }
}

} // namespace phonon
