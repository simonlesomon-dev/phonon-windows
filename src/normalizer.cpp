#include "normalizer.h"

#include <windows.h>
#include <algorithm>
#include <cwctype>
#include <vector>

namespace phonon {

namespace {

bool isWeakPunct(wchar_t c) { return c == L'.' || c == L','; }
bool isStrongPunct(wchar_t c) {
    return c == L'?' || c == L'!' || c == L':' || c == L';';
}

// Majuscule indépendante de la locale (accents français couverts).
wchar_t toUpperFr(wchar_t c) {
    switch (c) {
    case L'à': return L'À'; case L'â': return L'Â'; case L'ä': return L'Ä';
    case L'é': return L'É'; case L'è': return L'È'; case L'ê': return L'Ê';
    case L'ë': return L'Ë'; case L'î': return L'Î'; case L'ï': return L'Ï';
    case L'ô': return L'Ô'; case L'ö': return L'Ö';
    case L'ù': return L'Ù'; case L'û': return L'Û'; case L'ü': return L'Ü';
    case L'ç': return L'Ç'; case L'œ': return L'Œ'; case L'æ': return L'Æ';
    default: return std::towupper(c);
    }
}

} // namespace

std::wstring normalizeFrenchWide(const std::wstring& in) {
    // 1. Réduction des espaces.
    std::wstring s;
    s.reserve(in.size());
    bool lastSpace = true; // ignore les espaces de début
    for (wchar_t c : in) {
        if (std::iswspace(c)) {
            if (!lastSpace) { s += L' '; lastSpace = true; }
        } else {
            s += c;
            lastSpace = false;
        }
    }
    while (!s.empty() && s.back() == L' ') s.pop_back();

    // 2. Typographie française :
    //    - ? ! : ; -> une espace avant et après
    //    - . , -> pas d'espace avant, une après
    std::wstring t;
    t.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t c = s[i];
        if (isStrongPunct(c) || isWeakPunct(c)) {
            while (!t.empty() && t.back() == L' ') t.pop_back();
            if (isStrongPunct(c) && !t.empty()) t += L' ';
            t += c;
            if (i + 1 < s.size() && !std::iswspace(s[i + 1]) &&
                !isStrongPunct(s[i + 1]) && !isWeakPunct(s[i + 1]))
                t += L' ';
        } else {
            t += c;
        }
    }
    t.swap(s);

    // 3. Majuscule en début de phrase (après . ? ! ou au tout début).
    bool newSentence = true;
    for (auto& c : s) {
        if (newSentence && std::iswalpha(c)) {
            c = toUpperFr(c);
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
    std::wstring w(size_t(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, w.data(), n);
    std::wstring out = normalizeFrenchWide(w);
    int m = WideCharToMultiByte(CP_UTF8, 0, out.c_str(), -1,
                                nullptr, 0, nullptr, nullptr);
    std::string r(size_t(m - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, out.c_str(), -1, r.data(), m,
                        nullptr, nullptr);
    return r;
}

} // namespace phonon
