#include "paster.h"

#include <windows.h>
#include <string>

namespace phonon {

bool Paster::pasteText(const std::wstring& text, bool restoreClipboard) {
    if (text.empty()) return false;

    // Snapshot current clipboard (as unicode text) for restore.
    std::wstring prev;
    if (restoreClipboard && OpenClipboard(nullptr)) {
        HANDLE h = GetClipboardData(CF_UNICODETEXT);
        if (h) {
            const wchar_t* p = (const wchar_t*)GlobalLock(h);
            if (p) { prev.assign(p); GlobalUnlock(h); }
        }
        CloseClipboard();
    }

    if (!OpenClipboard(nullptr)) return false;
    EmptyClipboard();

    SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!g) { CloseClipboard(); return false; }
    memcpy(GlobalLock(g), text.c_str(), bytes);
    GlobalUnlock(g);

    if (!SetClipboardData(CF_UNICODETEXT, g)) {
        GlobalFree(g);
        CloseClipboard();
        return false;
    }
    CloseClipboard();

    // Give the foreground app a beat to see the new clipboard.
    Sleep(80);

    INPUT seq[6] = {};
    seq[0].type = INPUT_KEYBOARD; seq[0].ki.wVk = VK_CONTROL;
    seq[1].type = INPUT_KEYBOARD; seq[1].ki.wVk = 'V';
    seq[2].type = INPUT_KEYBOARD; seq[2].ki.wVk = 'V';
    seq[2].ki.dwFlags = KEYEVENTF_KEYUP;
    seq[3].type = INPUT_KEYBOARD; seq[3].ki.wVk = VK_CONTROL;
    seq[3].ki.dwFlags = KEYEVENTF_KEYUP;

    UINT sent = SendInput(4, seq, sizeof(INPUT));
    bool ok = sent == 4;

    if (restoreClipboard && !prev.empty()) {
        std::thread([prev]() {
            Sleep(400);
            if (OpenClipboard(nullptr)) {
                EmptyClipboard();
                HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE,
                                        (prev.size() + 1) * sizeof(wchar_t));
                if (g) {
                    memcpy(GlobalLock(g), prev.c_str(),
                           (prev.size() + 1) * sizeof(wchar_t));
                    GlobalUnlock(g);
                    SetClipboardData(CF_UNICODETEXT, g);
                }
                CloseClipboard();
            }
        }).detach();
    }
    return ok;
}

} // namespace phonon
