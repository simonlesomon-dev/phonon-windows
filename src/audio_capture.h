#pragma once
#include <windows.h>
#include <functional>
#include <thread>
#include <atomic>
#include <cstdint>

namespace phonon {

// Captures microphone audio via WASAPI shared mode and delivers
// mono 16 kHz float samples through the callback.
class AudioCapture {
public:
    using SinkFn = std::function<void(const float* samples, size_t count)>;
    ~AudioCapture();

    bool start(SinkFn sink, std::string& error);
    void stop();

private:
    void captureLoop();

    std::thread thread_;
    std::atomic<bool> running_{false};
    HANDLE stopEvent_ = nullptr;
    SinkFn sink_;
};

} // namespace phonon
