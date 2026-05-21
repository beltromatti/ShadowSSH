#include "Application.h"
#include "Platform.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <libssh/libssh.h>

namespace {
void WriteCrashLog(const std::string& message) {
    std::filesystem::path log_path = std::filesystem::path(Platform::GetTempDir()) / "shadowssh_crash.log";
    std::ofstream log(log_path);
    if (log) log << message << std::endl;
}
}

int main(int, char**)
{
    if (ssh_init() < 0) {
        std::cerr << "Failed to initialize libssh" << std::endl;
        return -1;
    }

    try {
        Application app;
        if (!app.Init()) {
            std::cerr << "Failed to init application." << std::endl;
            return -1;
        }
        app.Run();
    } catch (const std::exception& e) {
        std::cerr << "Crash detected: " << e.what() << std::endl;
        WriteCrashLog(std::string("Crash: ") + e.what());
        return -1;
    } catch (...) {
        std::cerr << "Unknown crash." << std::endl;
        WriteCrashLog("Unknown crash.");
        return -1;
    }

    ssh_finalize();
    return 0;
}