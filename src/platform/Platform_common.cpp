#include "Platform.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>

#if defined(_WIN32)
#  include <windows.h>
#  include <shlobj.h>
#else
#  include <pwd.h>
#  include <unistd.h>
#  include <sys/types.h>
#endif

namespace Platform {

std::string GetHomeDir() {
#if defined(_WIN32)
    const char* userprofile = std::getenv("USERPROFILE");
    if (userprofile && *userprofile) return userprofile;
    const char* drive = std::getenv("HOMEDRIVE");
    const char* path  = std::getenv("HOMEPATH");
    if (drive && path) return std::string(drive) + path;
    return "";
#else
    const char* home = std::getenv("HOME");
    if (home && *home) return home;
    if (struct passwd* pw = getpwuid(getuid()); pw && pw->pw_dir) return pw->pw_dir;
    return "";
#endif
}

std::string GetConfigDir() {
    std::filesystem::path dir;
#if defined(_WIN32)
    const char* appdata = std::getenv("APPDATA");
    if (appdata && *appdata) {
        dir = std::filesystem::path(appdata) / "ShadowSSH";
    } else {
        dir = std::filesystem::path(GetHomeDir()) / "AppData" / "Roaming" / "ShadowSSH";
    }
#elif defined(__APPLE__)
    dir = std::filesystem::path(GetHomeDir()) / "Library" / "Application Support" / "ShadowSSH";
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) {
        dir = std::filesystem::path(xdg) / "shadowssh";
    } else {
        dir = std::filesystem::path(GetHomeDir()) / ".config" / "shadowssh";
    }
#endif
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir.string();
}

std::string GetTempDir() {
    std::error_code ec;
    auto p = std::filesystem::temp_directory_path(ec);
    if (ec) {
#if defined(_WIN32)
        return "C:\\Windows\\Temp";
#else
        return "/tmp";
#endif
    }
    return p.string();
}

void OpenInFileManager(const std::string& /*path*/) {
    // Optional convenience; not wired into the UI today.
}

} // namespace Platform
