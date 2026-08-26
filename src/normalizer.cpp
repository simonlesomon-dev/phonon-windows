#include "normalizer.h"

#include <algorithm>
#include <cwctype>
#include <vector>

namespace phonon {

static bool isPunct(wchar_t c) {
    return c == L'?' || c == L'!' || c == L':' || c == L';' ||
           c == L'.' || c == L',';
}

std::wstring normalizeFrenchWide(const std::wstring& in) {
    // 1. Collapse whitespace.
    std::wstring s;
    s.reserve(in.size());
    bool lastSpace = true; // trim leading
    for (wchar_t c : in) {
        if (std::iswspace(c)) {
            if (!lastSpace) { s += L' '; lastSpace = true; }
        } else {
            s += c;
            lastSpace = false;
        }
    }
    while (!s.empty() && s.back() == L' ') s.pop_back();

    // 2. Remove space before punctuation, ensure single space after.
    std::wstring t;
    t.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t c = s[i];
        if (isPunct(c)) {
            while (!t.empty() && t.back() == L' ') t.pop_back();
            t += c;
            if (i + 1 < s.size() && !isPunct(s[i + 1]) &&
                s[i + 1] != L' ')
                t += L' ';
        } else {
            t += c;
        }
    }
    t.swap(s);

    // 3. Capitalize sentence starts.
    bool newSentence = true;
    for (auto& c : s) {
        if (newSentence && std::iswalpha(c)) {
            c = std::towupper(c);
            newSentence = false;
        } else if (c == L'.' || c == L'?' || c == L'!') {
            newSentence = true;
        } else if (!std::iswspace(c)) {
            newSentence = false;
        }
    }
    return s;
}

std::string normalizeFrench(const std::string& utf8) {
    if (utf8.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1,
                                nullptr, 0);
    std::wstring w(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, w.data(), n);
    std::wstring out = normalizeFrenchWide(w);
    int m = WideCharToMultiByte(CP_UTF8, 0, out.c_str(), -1,
                                nullptr, 0, nullptr, nullptr);
    std::string r(m - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, out.c_str(), -1, r.data(), m,
                        nullptr, nullptr);
    return r;
}

} // namespace phonon
