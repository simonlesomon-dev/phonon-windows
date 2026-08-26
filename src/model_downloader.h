#pragma once
#include <string>
#include <vector>

namespace phonon {

// Downloads the converted Parakeet OpenVINO model package (zip) over WinHTTP
// and extracts it into %LOCALAPPDATA%\PhononWindows\models\parakeet-v3.
class ModelDownloader {
public:
    static std::wstring modelRootDir();          // ...\models
    static std::wstring modelDir();              // ...\models\parakeet-v3
    static bool isModelPresent();                // checks encoder.xml etc.

    // Downloads + extracts if missing. Returns final model dir or empty.
    static std::wstring ensureModel(const std::string& url, std::string& error);

private:
    static bool httpGetToFile(const std::string& url,
                              const std::wstring& destPath,
                              std::string& error);
    static bool extractZip(const std::wstring& zipPath,
                           const std::wstring& destDir,
                           std::string& error);
};

} // namespace phonon
