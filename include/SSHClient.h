#pragma once
#include <libssh/libssh.h>
#include <string>
#include <functional>
#include <atomic>
#include <thread>
#include <mutex>
#include "SSHStructs.h"

class SSHClient {
public:
    SSHClient();
    ~SSHClient();

    // Connection
    void connect(const std::string& hostname, int port);
    void authenticate(const std::string& user, const std::string& password = "", const std::string& key_path = "");
    void disconnect();

    // State checks
    bool is_connected();
    bool is_authenticated();
    std::string get_error();
    bool is_busy(); // Connection/Auth in progress

    // Shell
    bool init_shell();
    void send_shell_command(const std::string& cmd);
    std::string read_shell_output();
    
    // Sync Exec
    std::string exec_command_sync(const std::string& cmd);

    ssh_session get_session() { return my_session; }
    std::recursive_mutex& get_mutex() { return session_mutex; }

private:
    ssh_session my_session;
    ssh_channel shell_channel = NULL;
    
    std::recursive_mutex session_mutex; // Protects my_session usage across threads
    
    std::atomic<bool> connected_flag{false};
    std::atomic<bool> authenticated_flag{false};
    std::atomic<bool> busy_flag{false};
    
    std::string last_error;
    std::mutex error_mutex;

    void set_error(const std::string& err);
};
