#if defined(__linux__) || defined(__unix__) && !defined(__APPLE__)

#include "Platform.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>

namespace {

bool HasCommand(const char* name) {
    std::string cmd = std::string("command -v ") + name + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

std::string ShellEscape(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

std::string RunCapture(const std::string& cmd) {
    std::unique_ptr<FILE, int(*)(FILE*)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "";
    std::string out;
    std::array<char, 4096> buf{};
    while (fgets(buf.data(), (int)buf.size(), pipe.get())) out += buf.data();
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
}

const char* PickFileDialogTool() {
    if (HasCommand("zenity")) return "zenity";
    if (HasCommand("kdialog")) return "kdialog";
    return nullptr;
}

} // namespace

namespace Platform {

std::string OpenFileDialog() {
    const char* tool = PickFileDialogTool();
    if (!tool) return "";
    std::string cmd;
    if (std::string(tool) == "zenity") {
        cmd = "zenity --file-selection 2>/dev/null";
    } else {
        cmd = "kdialog --getopenfilename 2>/dev/null";
    }
    return RunCapture(cmd);
}

std::string SaveFileDialog(const std::string& suggested_name, const std::string& starting_dir) {
    const char* tool = PickFileDialogTool();
    if (!tool) {
        // Fall back to ~/Downloads/<name>
        std::filesystem::path p = std::filesystem::path(GetHomeDir()) / "Downloads";
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return (p / suggested_name).string();
    }

    std::filesystem::path start = starting_dir.empty()
        ? std::filesystem::path(GetHomeDir()) / "Downloads"
        : std::filesystem::path(starting_dir);
    std::error_code ec;
    std::filesystem::create_directories(start, ec);
    std::filesystem::path initial = start / suggested_name;

    std::string cmd;
    if (std::string(tool) == "zenity") {
        cmd = "zenity --file-selection --save --confirm-overwrite --filename="
            + ShellEscape(initial.string()) + " 2>/dev/null";
    } else {
        cmd = "kdialog --getsavefilename " + ShellEscape(initial.string()) + " 2>/dev/null";
    }
    std::string chosen = RunCapture(cmd);
    if (chosen.empty()) return "";
    return chosen;
}

bool LaunchNativeSshTerminal(const std::string& user, const std::string& host,
                             const std::string& port, const std::string& key_path) {
    std::string ssh = "ssh " + user + "@" + host;
    if (!port.empty() && port != "22") ssh += " -p " + port;
    if (!key_path.empty()) ssh += " -i " + ShellEscape(key_path);

    const char* candidates[] = {
        "x-terminal-emulator", "gnome-terminal", "konsole",
        "xfce4-terminal", "alacritty", "kitty", "tilix",
        "mate-terminal", "lxterminal", "xterm", nullptr
    };

    for (int i = 0; candidates[i]; ++i) {
        if (!HasCommand(candidates[i])) continue;
        std::string term = candidates[i];
        std::string cmd;
        if (term == "gnome-terminal" || term == "mate-terminal" || term == "tilix") {
            cmd = term + " -- bash -lc " + ShellEscape(ssh + "; exec bash") + " &";
        } else if (term == "konsole") {
            cmd = term + " -e bash -lc " + ShellEscape(ssh + "; exec bash") + " &";
        } else if (term == "xfce4-terminal" || term == "lxterminal") {
            cmd = term + " -e " + ShellEscape("bash -lc " + ShellEscape(ssh + "; exec bash")) + " &";
        } else if (term == "alacritty" || term == "kitty") {
            cmd = term + " -e bash -lc " + ShellEscape(ssh + "; exec bash") + " &";
        } else { // xterm, x-terminal-emulator
            cmd = term + " -e bash -lc " + ShellEscape(ssh + "; exec bash") + " &";
        }
        if (std::system(cmd.c_str()) == 0) return true;
    }
    return false;
}

} // namespace Platform

#endif
