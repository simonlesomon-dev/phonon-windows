#pragma once
#include <string>
#include <vector>
#include <cstddef>

namespace phonon {

// Log-mel frontend reproduisant exactement le préprocesseur NeMo exporté
// par onnx-asr (istupakov/parakeet-tdt-0.6b-v3-onnx) :
//   - pré-accentuation 0.97 (masquée par la longueur valide)
//   - pad 256 zéros de chaque côté, fenêtre hanning(400) paddée à 512
//   - FFT 512, puissance |X|^2, banc mel slaney/slaney 257x128
//   - log(mel + 2^-24), normalisation par énoncé (mean/std par bande)
// Les constantes (fenêtre + banc) sont chargées depuis frontend.bin.
class MelFrontend {
public:
    static constexpr int kSampleRate = 16000;
    static constexpr int kN = 512;        // taille FFT / fenêtre paddée
    static constexpr int kHop = 160;      // 10 ms
    static constexpr int kMels = 128;

    // Charge frontend.bin : [512 float32 fenêtre][257x128 float32 banc].
    bool load(const std::string& path);

    // pcm : mono 16 kHz floats [-1,1], n = nombre d'échantillons valides.
    // Retourne les frames [T x kMels] row-major ; frames invalides à zéro.
    std::vector<float> compute(const float* pcm, size_t n);

    // Nombre de frames valides du dernier compute() (= n / kHop).
    size_t validFrames() const { return validFrames_; }

private:
    void fft(float* re, float* im) const;   // radix-2, taille kN

    std::vector<float> window_;              // [kN]
    std::vector<float> melBankT_;            // [128 x 257] row-major (transposé pour matmul simple)
    std::vector<float> twRe_, twIm_;
    std::vector<int> bitRev_;
    size_t validFrames_ = 0;
};

} // namespace phonon
