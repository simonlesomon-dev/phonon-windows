#pragma once
#include <windows.h>
#include <string>

namespace phonon {

// Minimal system-tray icon with runtime-drawn icon.
class TrayIcon {
public:
    void create(HWND owner, HINSTANCE hinst, UINT callbackMsg);
    void updateTooltip(const std::wstring& tooltip);
    void showBalloon(const std::wstring& title, const std::wstring& text);
    void remove();

private:
    HICON buildIcon();
    NOTIFYICONDATAW nid_{};
    HICON icon_ = nullptr;
};

} // namespace phonon
