#pragma once
#include <string>

namespace phonon {

// Places text on the clipboard and sends Ctrl+V to the foreground window.
class Paster {
public:
    // restoreClipboard: put back the previous content after pasting.
    static bool pasteText(const std::wstring& text, bool restoreClipboard = true);
};

} // namespace phonon
