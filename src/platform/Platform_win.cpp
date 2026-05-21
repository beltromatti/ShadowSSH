#if defined(_WIN32)

#include "Platform.h"

#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>

#include <array>
#include <filesystem>
#include <string>

namespace {

std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int len = ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(len, '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), s.data(), len, nullptr, nullptr);
    return s;
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int len = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(len, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), len);
    return w;
}

} // namespace

namespace Platform {

std::string OpenFileDialog() {
    std::array<wchar_t, MAX_PATH> file{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = nullptr;
    ofn.lpstrFile   = file.data();
    ofn.nMaxFile    = (DWORD)file.size();
    ofn.lpstrFilter = L"All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (::GetOpenFileNameW(&ofn)) {
        return WideToUtf8(file.data());
    }
    return "";
}

std::string SaveFileDialog(const std::string& suggested_name, const std::string& starting_dir) {
    std::wstring wname = Utf8ToWide(suggested_name);
    std::wstring wdir  = Utf8ToWide(starting_dir);

    std::array<wchar_t, MAX_PATH> file{};
    if (!wname.empty()) {
        wcsncpy_s(file.data(), file.size(), wname.c_str(), wname.size());
    }

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = nullptr;
    ofn.lpstrFile   = file.data();
    ofn.nMaxFile    = (DWORD)file.size();
    ofn.lpstrInitialDir = wdir.empty() ? nullptr : wdir.c_str();
    ofn.lpstrFilter = L"All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (::GetSaveFileNameW(&ofn)) {
        return WideToUtf8(file.data());
    }
    return "";
}

bool LaunchNativeSshTerminal(const std::string& user, const std::string& host,
                             const std::string& port, const std::string& key_path) {
    std::string ssh = "ssh " + user + "@" + host;
    if (!port.empty() && port != "22") ssh += " -p " + port;
    if (!key_path.empty()) ssh += " -i \"" + key_path + "\"";

    // Prefer Windows Terminal (`wt`), fall back to cmd /K.
    std::wstring args;
    HINSTANCE rc = ::ShellExecuteW(nullptr, L"open", L"wt.exe",
                                   (L"-d . cmd /K " + Utf8ToWide(ssh)).c_str(),
                                   nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)rc > 32) return true;

    std::wstring cmdline = L"/K " + Utf8ToWide(ssh);
    rc = ::ShellExecuteW(nullptr, L"open", L"cmd.exe", cmdline.c_str(), nullptr, SW_SHOWNORMAL);
    return (INT_PTR)rc > 32;
}

} // namespace Platform

#endif
