#include "settings.h"
#include <windows.h>

namespace phonon {

bool Settings::restoreClipboard = true;
std::string Settings::modelUrl =
    "https://github.com/simonlesomon-dev/phonon-windows/releases/latest/download/"
    "parakeet-v3-openvino.zip";

static const wchar_t* kRunKey =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* kRunValue = L"PhononWindows";

void Settings::load() {
    HKEY h;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &h) ==
        ERROR_SUCCESS) {
        DWORD type = 0, size = 0;
        if (RegQueryValueExW(h, kRunValue, nullptr, &type, nullptr,
                             &size) == ERROR_SUCCESS)
            setAutostart(true); // present
        RegCloseKey(h);
    }
}

void Settings::save() {}

bool Settings::autostartEnabled() {
    HKEY h;
    bool ok = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &h) ==
        ERROR_SUCCESS) {
        ok = RegQueryValueExW(h, kRunValue, nullptr, nullptr, nullptr,
                              nullptr) == ERROR_SUCCESS;
        RegCloseKey(h);
    }
    return ok;
}

void Settings::setAutostart(bool enabled) {
    HKEY h;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE,
                      &h) != ERROR_SUCCESS)
        return;
    if (enabled) {
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::wstring cmd = L"\"" + std::wstring(exe) + L"\" --minimized";
        RegSetValueExW(h, kRunValue, 0, REG_SZ,
                       (const BYTE*)cmd.c_str(),
                       DWORD((cmd.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(h, kRunValue);
    }
    RegCloseKey(h);
}

} // namespace phonon
