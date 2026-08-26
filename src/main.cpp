#include "app.h"
#include "settings.h"

#include <objbase.h>

namespace phonon {

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(size_t(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0,
                                nullptr, nullptr);
    std::string s(size_t(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n,
                        nullptr, nullptr);
    return s;
}

} // namespace phonon

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    (void)nCmdShow;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    phonon::Settings::load();
    bool minimized =
        GetCommandLineW() && wcsstr(GetCommandLineW(), L"--minimized");
    (void)minimized;
    int rc = phonon::App::instance().run(hInstance);
    CoUninitialize();
    return rc;
}
