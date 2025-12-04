#include "Application.h"
#include <iostream>
#include <fstream>
#include <libssh/libssh.h>

int main(int, char**)
{
    // Initialize libssh
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
        std::ofstream log("/tmp/shadowssh_crash.log");
        log << "Crash: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "Unknown crash." << std::endl;
        std::ofstream log("/tmp/shadowssh_crash.log");
        log << "Unknown crash." << std::endl;
        return -1;
    }
    
    ssh_finalize();
    return 0;
}