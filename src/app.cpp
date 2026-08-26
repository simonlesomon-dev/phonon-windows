#include "app.h"
#include "audio_capture.h"
#include "transcriber.h"
#include "paster.h"
#include "model_downloader.h"
#include "settings.h"

#include <objbase.h>
#include <shellapi.h>
#include <string>
#include <memory>
#include <thread>

namespace phonon {

namespace {
constexpr UINT WM_APP_TRAY = WM_APP + 1;
constexpr int HOTKEY_ID = 1;
constexpr int ID_TRAYMENU_TOGGLE = 2001;
constexpr int ID_TRAYMENU_AUTOSTART = 2002;
constexpr int ID_TRAYMENU_QUIT = 2003;

AudioCapture& sharedCapture() {
    static AudioCapture cap;
    return cap;
}
}

App& App::instance() {
    static App app;
    return app;
}

int App::run(HINSTANCE hInstance) {
    hinst_ = hInstance;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &App::wndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"PhononWindow";
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(0, L"PhononWindow", L"Phonon", WS_OVERLAPPED,
                            0, 0, 0, 0, nullptr, nullptr, hInstance, this);
    if (!hwnd_) return 1;

    createTray();
    RegisterHotKey(hwnd_, HOTKEY_ID, MOD_CONTROL | MOD_NOREPEAT,
                   VK_SPACE);

    // Init model on a background thread.
    std::thread([this]() {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        std::string modelDirW;
        if (!ModelDownloader::isModelPresent()) {
            auto dir = ModelDownloader::ensureModel(Settings::modelUrl,
                                                    modelDirW);
            if (dir.empty()) {
                PostMessage(hwnd_, WM_APP + 2, 0, 0);
                CoUninitialize();
                return;
            }
        }
        PostMessage(hwnd_, WM_APP + 3, 0, 0);
        CoUninitialize();
    }).detach();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterHotKey(hwnd_, HOTKEY_ID);
    tray_.remove();
    return 0;
}

LRESULT CALLBACK App::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App* self = nullptr;
    if (msg == WM_NCCREATE) {
        self = (App*)((CREATESTRUCTW*)lp)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
    } else {
        self = (App*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    }
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_HOTKEY:
        if (wp == HOTKEY_ID) self->onHotkey();
        return 0;

    case WM_APP_TRAY: {
        if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU)
            self->showTrayMenu();
        break;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_TRAYMENU_TOGGLE: self->toggleRecording(); break;
        case ID_TRAYMENU_AUTOSTART:
            Settings::setAutostart(!Settings::autostartEnabled());
            break;
        case ID_TRAYMENU_QUIT:
            PostQuitMessage(0);
            break;
        }
        return 0;

    case WM_APP + 2: // model download failed
        self->tray_.showBalloon(
            L"Phonon — modèle indisponible",
            L"Le modèle n'a pas pu être téléchargé. "
            L"Vérifiez votre connexion puis relancez l'application.");
        return 0;

    case WM_APP + 3: // model present
        self->updateTray();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void App::createTray() { tray_.create(hwnd_, hinst_, WM_APP_TRAY); }

void App::setEngineState(EngineState s) {
    state_.store(s);
    updateTray();
}

void App::updateTray() {
    const wchar_t* st =
        state_ == EngineState::Recording ? L"● Enregistrement" :
        state_ == EngineState::Processing ? L"… Transcription" :
                                            L"Prêt";
    tray_.updateTooltip(std::wstring(L"Phonon — ") + st + L" (" +
                        deviceName_ + L")");
}

void App::onHotkey() {
    if (state_ == EngineState::Processing) return;

    if (state_ == EngineState::Idle) {
        // Start capture.
        samples_.clear();
        std::string err;
        bool ok = sharedCapture().start(
            [this](const float* data, size_t n) {
                if (!data) return; // error
                std::lock_guard<std::mutex> lk(mtx_);
                samples_.insert(samples_.end(), data, data + n);
            },
            err);
        setEngineState(ok ? EngineState::Recording : EngineState::Idle);
        if (!ok)
            tray_.showBalloon(L"Phonon",
                              utf8ToWide(err.empty()
                                             ? "Erreur micro"
                                             : err));
    } else {
        setEngineState(EngineState::Processing);
        finishAndTranscribe();
    }
}

void App::finishAndTranscribe() {
    // Stop capture and transcribe off the UI thread.
    std::thread([this]() {
        sharedCapture().stop();

        std::vector<float> pcm;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            pcm.swap(samples_);
        }

        if (pcm.size() < size_t(16000 * 0.2)) {
            setEngineState(EngineState::Idle);
            return;
        }

        if (!transcriberReady_) {
            std::string err;
            transcriber_ = std::make_unique<Transcriber>();
            Transcriber::Config cfg;
            cfg.modelDir = wideToUtf8(ModelDownloader::modelDir());
            cfg.device = Transcriber::pickDevice();
            deviceName_ = utf8ToWide(cfg.device);
            if (!transcriber_->init(cfg, err)) {
                tray_.showBalloon(L"Phonon — erreur modèle",
                                  utf8ToWide(err));
                setEngineState(EngineState::Idle);
                return;
            }
            transcriberReady_ = true;
            updateTray();
        }

        std::string text = transcriber_->transcribe(pcm);
        setEngineState(EngineState::Idle);

        if (!text.empty())
            Paster::pasteText(utf8ToWide(text), Settings::restoreClipboard);
    }).detach();
}

void App::showTrayMenu() {
    POINT p;
    GetCursorPos(&p);
    HMENU menu = CreatePopupMenu();
    const wchar_t* toggle =
        state_ == EngineState::Idle ? L"Démarrer la dictée\tCtrl+Espace"
                                    : L"Arrêter et coller";
    AppendMenuW(menu, MF_STRING, ID_TRAYMENU_TOGGLE, toggle);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu,
                Settings::autostartEnabled() ? MF_STRING | MF_CHECKED
                                             : MF_STRING,
                ID_TRAYMENU_AUTOSTART, L"Lancer au démarrage de Windows");
    AppendMenuW(menu, MF_STRING, ID_TRAYMENU_QUIT, L"Quitter");

    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, p.x, p.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}

} // namespace phonon
