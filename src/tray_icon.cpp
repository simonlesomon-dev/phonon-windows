#include "tray_icon.h"
#include "resource.h"

#include <algorithm>

namespace phonon {

HICON TrayIcon::buildIcon() {
    // Draw a simple microphone glyph at runtime: dark rounded rect + stem.
    int size = GetSystemMetrics(SM_CXICON);
    HDC scr = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(scr);
    HBITMAP bmp = CreateCompatibleBitmap(scr, size, size);
    HBITMAP old = (HBITMAP)SelectObject(mem, bmp);

    RECT rc{0, 0, size, size};
    HBRUSH bg = CreateSolidBrush(RGB(30, 30, 46));
    FillRect(mem, &rc, bg);
    DeleteObject(bg);

    HPEN pen = CreatePen(PS_SOLID, (std::max)(3, size / 10), RGB(166, 227, 161));
    HPEN oldPen = (HPEN)SelectObject(mem, pen);
    HBRUSH brush = CreateSolidBrush(RGB(166, 227, 161));
    HBRUSH oldBrush = (HBRUSH)SelectObject(mem, brush);

    // Capsule
    Ellipse(mem, int(size * .35), int(size * .15),
            int(size * .65), int(size * .55));
    Rectangle(mem, int(size * .35), int(size * .25),
              int(size * .65), int(size * .45));
    // Stem + base
    MoveToEx(mem, size / 2, int(size * .55), nullptr);
    LineTo(mem, size / 2, int(size * .75));
    MoveToEx(mem, int(size * .3), int(size * .75), nullptr);
    LineTo(mem, int(size * .7), int(size * .75));

    SelectObject(mem, oldBrush);
    SelectObject(mem, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
    SelectObject(mem, old);
    DeleteDC(mem);
    ReleaseDC(nullptr, scr);

    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmColor = bmp;
    ii.hbmMask = CreateBitmap(size, size, 1, 1, nullptr);
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(ii.hbmMask);
    DeleteObject(bmp);
    return icon;
}

void TrayIcon::create(HWND owner, HINSTANCE hinst, UINT callbackMsg) {
    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = owner;
    nid_.uID = IDI_TRAY;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = callbackMsg;
    icon_ = buildIcon();
    nid_.hIcon = icon_;
    lstrcpyW(nid_.szTip, L"Phonon");
    Shell_NotifyIconW(NIM_ADD, &nid_);
}

void TrayIcon::updateTooltip(const std::wstring& tooltip) {
    nid_.uFlags |= NIF_INFO;
    wcsncpy_s(nid_.szTip, tooltip.c_str(), _TRUNCATE);
    nid_.uFlags &= ~NIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void TrayIcon::showBalloon(const std::wstring& title,
                           const std::wstring& text) {
    nid_.uFlags |= NIF_INFO;
    wcsncpy_s(nid_.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid_.szInfo, text.c_str(), _TRUNCATE);
    nid_.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
    nid_.dwInfoFlags = 0;
}

void TrayIcon::remove() {
    Shell_NotifyIconW(NIM_DELETE, &nid_);
    if (icon_) { DestroyIcon(icon_); icon_ = nullptr; }
}

} // namespace phonon
