#pragma once
#include <windows.h>
#include <string>
#include <atomic>
#include <mutex>
#include <vector>
#include <memory>
#include "tray_icon.h"

namespace phonon {

class Transcriber;

enum class EngineState { Idle, Recording, Processing };

class App {
public:
    static App& instance();
    int run(HINSTANCE hInstance);

    void toggleRecording();
    void setEngineState(EngineState s);
    EngineState engineState() const { return state_; }
    const std::wstring& deviceName() const { return deviceName_; }

private:
    App() = default;
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
    void onHotkey();
    void finishAndTranscribe();
    void createTray();
    void updateTray();
    void showTrayMenu();

    HWND hwnd_ = nullptr;
    HINSTANCE hinst_ = nullptr;
    std::atomic<EngineState> state_{EngineState::Idle};
    std::wstring deviceName_ = L"NPU";
    NOTIFYICONDATAW nid_{}; // unused legacy
    TrayIcon tray_;

    std::mutex mtx_;
    std::vector<float> samples_;
    std::unique_ptr<Transcriber> transcriber_;
    bool transcriberReady_ = false;
};

std::wstring utf8ToWide(const std::string& s);
std::string wideToUtf8(const std::wstring& w);

} // namespace phonon

