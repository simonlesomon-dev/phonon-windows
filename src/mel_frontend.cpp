#include "mel_frontend.h"

#include <cmath>
#include <algorithm>
#include <cstring>
#include <fstream>

namespace phonon {
namespace {
constexpr float kPi = 3.14159265358979f;
}

bool MelFrontend::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    window_.resize(kN);
    f.read(reinterpret_cast<char*>(window_.data()),
           std::streamsize(kN * sizeof(float)));

    constexpr int kBins = kN / 2 + 1; // 257
    std::vector<float> bank(kBins * kMels);
    f.read(reinterpret_cast<char*>(bank.data()),
           std::streamsize(bank.size() * sizeof(float)));
    if (!f) return false;

    // Transposition en [mel][bin] pour un accès cache-friendly.
    melBankT_.resize(kMels * kBins);
    for (int b = 0; b < kBins; ++b)
        for (int m = 0; m < kMels; ++m)
            melBankT_[size_t(m) * kBins + b] =
                bank[size_t(b) * kMels + m];

    // Twiddles + bit-reversal pour FFT radix-2 de taille kN.
    bitRev_.resize(kN);
    int bits = 0;
    while ((1 << bits) < kN) ++bits;
    for (int i = 0; i < kN; ++i) {
        int r = 0;
        for (int b = 0; b < bits; ++b)
            if (i & (1 << b)) r |= 1 << (bits - 1 - b);
        bitRev_[i] = r;
    }
    twRe_.resize(kN / 2);
    twIm_.resize(kN / 2);
    for (int i = 0; i < kN / 2; ++i) {
        twRe_[i] = std::cos(-2.0f * float(kPi) * i / kN);
        twIm_[i] = std::sin(-2.0f * float(kPi) * i / kN);
    }
    return true;
}

void MelFrontend::fft(float* re, float* im) const {
    for (int i = 0; i < kN; ++i) {
        int j = bitRev_[i];
        if (j > i) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    for (int size = 2; size <= kN; size <<= 1) {
        int half = size >> 1;
        int step = kN / size;
        for (int start = 0; start < kN; start += size) {
            for (int i = 0; i < half; ++i) {
                float wr = twRe_[i * step], wi = twIm_[i * step];
                int a = start + i, b = a + half;
                float tr = re[b] * wr - im[b] * wi;
                float ti = re[b] * wi + im[b] * wr;
                re[b] = re[a] - tr;
                im[b] = im[a] - ti;
                re[a] += tr;
                im[a] += ti;
            }
        }
    }
}

std::vector<float> MelFrontend::compute(const float* pcm, size_t n) {
    static thread_local std::vector<float> pre;
    pre.assign(n, 0.0f);
    if (n > 0) {
        // Pré-accentuation : x[0] inchangé, x[t] -= 0.97 * x[t-1].
        pre[0] = pcm[0];
        for (size_t i = 1; i < n; ++i)
            pre[i] = pcm[i] - 0.97f * pcm[i - 1];
    }

    const size_t nFrames = n / kHop + 1; // signal paddé 256 de chaque côté
    validFrames_ = n / kHop;             // lens / hop_length (division entière)

    constexpr int kBins = kN / 2 + 1;
    constexpr float kGuard = 5.9604645e-08f; // 2^-24

    std::vector<float> out(nFrames * kMels, 0.0f);
    std::vector<float> fre(kN), fim(kN), power(kBins), mel(kMels);

    for (size_t t = 0; t < nFrames; ++t) {
        const long long center = long long(t) * kHop;
        for (int i = 0; i < kN; ++i) {
            long long idx = center - kN / 2 + i; // padding 256
            float s = (idx >= 0 && idx < long long(n)) ? pre[size_t(idx)] : 0.0f;
            fre[i] = s * window_[i];
            fim[i] = 0.0f;
        }
        fft(fre.data(), fim.data());

        for (int b = 0; b < kBins; ++b)
            power[b] = fre[b] * fre[b] + fim[b] * fim[b];

        for (int m = 0; m < kMels; ++m) {
            const float* row = &melBankT_[size_t(m) * kBins];
            float e = 0.0f;
            for (int b = 0; b < kBins; ++b) e += row[b] * power[b];
            mel[m] = std::log(e + kGuard);
        }
        std::memcpy(&out[size_t(t) * kMels], mel.data(),
                    sizeof(float) * kMels);
    }

    // Normalisation par énoncé : (x - mean) / (sqrt(var) + 1e-5),
    // calculée sur les frames valides uniquement ; frames invalides -> 0.
    const size_t T = validFrames_;
    if (T > 1) {
        for (int m = 0; m < kMels; ++m) {
            double mean = 0.0;
            for (size_t t = 0; t < T; ++t)
                mean += out[size_t(t) * kMels + m];
            mean /= double(T);

            double var = 0.0;
            for (size_t t = 0; t < T; ++t) {
                double d = out[size_t(t) * kMels + m] - mean;
                var += d * d;
            }
            var /= double(T - 1); // unbiased, comme ReduceSumSquare/(lens-1)

            float invStd = float(1.0 / (std::sqrt(var) + 1e-5));
            for (size_t t = 0; t < T; ++t)
                out[size_t(t) * kMels + m] =
                    (out[size_t(t) * kMels + m] - float(mean)) * invStd;
        }
    } else {
        out.assign(out.size(), 0.0f);
    }
    return out;
}

} // namespace phonon
