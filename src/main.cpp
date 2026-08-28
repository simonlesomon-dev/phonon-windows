#include "app.h"
#include "settings.h"

#include <objbase.h>

namespace phonon {

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 1) return {};
    std::wstring w(size_t(n), L'\0');
    int written = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1,
                                      w.data(), n);
    if (written <= 1) return {};
    w.resize(size_t(written - 1));
    return w;
}

std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0,
                                nullptr, nullptr);
    if (n <= 1) return {};
    std::string s(size_t(n), '\0');
    int written = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1,
                                      s.data(), n, nullptr, nullptr);
    if (written <= 1) return {};
    s.resize(size_t(written - 1));
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
