#pragma once
#include <string>

namespace phonon {

// French-friendly post-processing of raw ASR output.
std::string normalizeFrench(const std::string& utf8);
std::wstring normalizeFrenchWide(const std::wstring& text);

} // namespace phonon
