#include "model_downloader.h"
#include "settings.h"

#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <fstream>
#include <thread>
#pragma comment(lib, "winhttp")

namespace phonon {

std::wstring ModelDownloader::modelRootDir() {
    wchar_t path[MAX_PATH];
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path);
    std::wstring dir = std::wstring(path) + L"\\PhononWindows\\models";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::wstring ModelDownloader::modelDir() {
    std::wstring dir = modelRootDir() + L"\\parakeet-v3";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

static bool fileExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool ModelDownloader::isModelPresent() {
    std::wstring d = modelDir();
    return fileExists(d + L"\\encoder.xml") &&
           fileExists(d + L"\\decoder_joint.xml") &&
           fileExists(d + L"\\frontend.bin") &&
           fileExists(d + L"\\tokens.txt");
}

bool ModelDownloader::httpGetToFile(const std::string& url,
                                    const std::wstring& destPath,
                                    std::string& error) {
    // Parse https://host/path
    if (url.rfind("https://", 0) != 0) {
        error = "URL non supportée: " + url;
        return false;
    }
    std::string rest = url.substr(8);
    auto slash = rest.find('/');
    std::string host = rest.substr(0, slash);
    std::string path = slash == std::string::npos ? "/" : rest.substr(slash);

    URL_COMPONENTSA uc{};
    uc.dwStructSize = sizeof(uc);
    char hostBuf[256] = {}, pathBuf[2048] = {};
    uc.lpszHostName = hostBuf; uc.dwHostNameLength = sizeof(hostBuf);
    uc.lpszUrlPath = pathBuf; uc.dwUrlPathLength = sizeof(pathBuf);
    if (!WinHttpCrackUrlA(url.c_str(), 0, 0, &uc)) {
        error = "URL invalide"; return false;
    }

    HINTERNET sess = WinHttpOpen(L"PhononWindows/0.1",
                                 WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                 WINHTTP_NO_PROXY_NAME,
                                 WINHTTP_NO_PROXY_BYPASS, 0);
    if (!sess) { error = "WinHttpOpen"; return false; }

    bool ok = false;
    HINTERNET conn = WinHttpConnect(sess,
        std::wstring(hostBuf, hostBuf + uc.dwHostNameLength).c_str(),
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (conn) {
        HINTERNET req = WinHttpOpenRequest(conn, L"GET",
            std::wstring(pathBuf, pathBuf + uc.dwUrlPathLength).c_str(),
            nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (req && WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                      WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(req, nullptr)) {
            DWORD status = 0, sz = sizeof(status);
            WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE |
                                     WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX,
                                &status, &sz, WINHTTP_NO_HEADER_INDEX);
            if (status == 200) {
                std::ofstream f(destPath, std::ios::binary);
                DWORD read = 0;
                char buf[65536];
                while (WinHttpQueryDataAvailable(req, &read) && read > 0) {
                    DWORD got = 0;
                    if (!WinHttpReadData(req, buf, read, &got) || got == 0)
                        break;
                    f.write(buf, got);
                }
                ok = bool(f) && f.good() || f.eof();
            } else {
                error = "HTTP " + std::to_string(status);
            }
        }
        if (req) WinHttpCloseHandle(req);
        WinHttpCloseHandle(conn);
    }
    WinHttpCloseHandle(sess);
    if (!ok && error.empty()) error = "Téléchargement échoué";
    return ok;
}

bool ModelDownloader::extractZip(const std::wstring& zipPath,
                                 const std::wstring& destDir,
                                 std::string& error) {
    CreateDirectoryW(destDir.c_str(), nullptr);
    std::wstring cmd =
        L"-NoProfile -NonInteractive -WindowStyle Hidden -Command "
        L"Expand-Archive -Force -LiteralPath '" + zipPath + L"' '" +
        destDir + L"'";
    int rc = (int)reinterpret_cast<intptr_t>(
        ShellExecuteW(nullptr, L"open", L"powershell.exe", cmd.c_str(),
                      nullptr, SW_HIDE));
    if (rc <= 32) { error = "Extraction impossible"; return false; }

    // Wait for files to appear (PowerShell runs detached).
    for (int i = 0; i < 1200 && !isModelPresent(); ++i) Sleep(250);
    if (!isModelPresent()) { error = "Archive incomplète"; return false; }
    return true;
}

std::wstring ModelDownloader::ensureModel(const std::string& url,
                                          std::string& error) {
    if (isModelPresent()) return modelDir();

    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring zip = std::wstring(tmp) + L"phonon-parakeet-v3.zip";

    if (!httpGetToFile(url, zip, error)) return L"";
    if (!extractZip(zip, modelDir(), error)) return L"";

    DeleteFileW(zip.c_str());
    return modelDir();
}

} // namespace phonon
