#pragma once
#include <string>

namespace phonon {

class Settings {
public:
    static void load();
    static void save();

    static bool autostartEnabled();
    static void setAutostart(bool enabled);   // HKCU Run key

    static bool restoreClipboard;
    static std::string modelUrl;
};

} // namespace phonon
