#include "audio_capture.h"
#include "resampler.h"

#include <mmdeviceapi.h>
#include <audioclient.h>
#include <cstring>
#include <string>

namespace phonon {

AudioCapture::~AudioCapture() { stop(); }

bool AudioCapture::start(SinkFn sink, std::string& error) {
    if (running_.load()) return true;
    sink_ = std::move(sink);

    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stopEvent_) { error = "CreateEvent a échoué"; return false; }

    running_.store(true);
    thread_ = std::thread(&AudioCapture::captureLoop, this);
    return true;
}

void AudioCapture::stop() {
    if (!running_.load()) return;
    running_.store(false);
    if (stopEvent_) SetEvent(stopEvent_);
    if (thread_.joinable()) thread_.join();
    if (stopEvent_) { CloseHandle(stopEvent_); stopEvent_ = nullptr; }
}

void AudioCapture::captureLoop() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool comInit = SUCCEEDED(hr);

    IMMDeviceEnumerator* enumr = nullptr;
    IAudioClient* client = nullptr;
    IAudioCaptureClient* capture = nullptr;
    WAVEFORMATEX* fmt = nullptr;
    std::string err;

    auto fail = [&](const char* msg) {
        err = msg;
        running_.store(false);
        if (sink_) sink_(nullptr, 0); // signal error/EOF
    };

    do {
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              __uuidof(IMMDeviceEnumerator), (void**)&enumr);
        if (FAILED(hr)) { fail("MMDeviceEnumerator"); break; }

        IMMDevice* dev = nullptr;
        hr = enumr->GetDefaultAudioEndpoint(eCapture, eCommunications, &dev);
        if (FAILED(hr)) { fail("Aucun micro par défaut"); break; }

        hr = dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                           (void**)&client);
        dev->Release();
        if (FAILED(hr)) { fail("Activation audio"); break; }

        hr = client->GetMixFormat(&fmt);
        if (FAILED(hr)) { fail("GetMixFormat"); break; }

        // Shared-mode float capture is expected; fall back to PCM16 handling.
        const bool isFloat = (fmt->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
            (fmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
             reinterpret_cast<WAVEFORMATEXTENSIBLE*>(fmt)->SubFormat ==
                 KSDATAFORMAT_SUBTYPE_IEEE_FLOAT));

        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                10000000 /*1s buffer*/, 0, fmt, nullptr);
        if (FAILED(hr)) { fail("Initialize audio"); break; }

        HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        client->SetEventHandle(ev);

        hr = client->GetService(__uuidof(IAudioCaptureClient),
                                (void**)&capture);
        if (FAILED(hr)) { CloseHandle(ev); fail("GetService capture"); break; }

        client->Start();
        Resampler resampler(fmt->nSamplesPerSec, fmt->nChannels);
        std::vector<float> chunk16k;

        while (running_.load()) {
            DWORD wait = WaitForSingleObject(ev, 100);
            if (wait != WAIT_OBJECT_0) continue;

            UINT32 packet = 0;
            while (SUCCEEDED(capture->GetNextPacketSize(&packet)) && packet > 0) {
                BYTE* data;
                UINT32 frames;
                DWORD flags;
                if (FAILED(capture->GetBuffer(&data, &frames, &flags,
                                              nullptr, nullptr)))
                    break;

                chunk16k.clear();
                if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data) {
                    if (isFloat) {
                        resampler.process(
                            reinterpret_cast<const float*>(data), frames,
                            chunk16k);
                    } else if (fmt->wBitsPerSample == 16) {
                        // Convert int16 -> float on the fly via temp buffer.
                        static thread_local std::vector<float> tmp;
                        tmp.resize(size_t(frames) * fmt->nChannels);
                        const int16_t* s =
                            reinterpret_cast<const int16_t*>(data);
                        for (size_t i = 0; i < tmp.size(); ++i)
                            tmp[i] = s[i] / 32768.0f;
                        resampler.process(tmp.data(), frames, chunk16k);
                    }
                } else {
                    // Silence: feed zeros to keep the timeline continuous.
                    size_t need = size_t(double(frames) * resampler.ratio());
                    chunk16k.assign(need, 0.0f);
                }
                capture->ReleaseBuffer(frames);

                if (!chunk16k.empty() && sink_)
                    sink_(chunk16k.data(), chunk16k.size());
            }
        }

        client->Stop();
        CloseHandle(ev);
    } while (false);

    if (fmt) CoTaskMemFree(fmt);
    if (capture) capture->Release();
    if (client) client->Release();
    if (enumr) enumr->Release();
    if (comInit) CoUninitialize();

    if (!err.empty() && sink_) {
        OutputDebugStringA(("Phonon: erreur audio: " + err + "\n").c_str());
        sink_(nullptr, 0);
    }
}

} // namespace phonon
