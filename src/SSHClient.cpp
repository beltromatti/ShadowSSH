#include "SSHClient.h"
#include <iostream>
#include <cstdlib>

SSHClient::SSHClient() {
    my_session = ssh_new();
    if (my_session == NULL) {
        // Critical failure
        std::cerr << "Error allocating SSH session" << std::endl;
    }
}

SSHClient::~SSHClient() {
    disconnect();
    join_worker();
    ssh_free(my_session);
}

void SSHClient::disconnect() {
    join_worker();
    std::lock_guard<std::recursive_mutex> lock(session_mutex);
    close_shell_channel();
    if (my_session && ssh_is_connected(my_session)) {
        ssh_disconnect(my_session);
    }
    connected_flag = false;
    authenticated_flag = false;
}

void SSHClient::set_error(const std::string& err) {
    std::lock_guard<std::mutex> lock(error_mutex);
    last_error = err;
}

std::string SSHClient::get_error() {
    std::lock_guard<std::mutex> lock(error_mutex);
    return last_error;
}

bool SSHClient::is_connected() {
    return connected_flag;
}

bool SSHClient::is_authenticated() {
    return authenticated_flag;
}

bool SSHClient::is_busy() {
    return busy_flag.load();
}

void SSHClient::connect(const std::string& hostname, int port) {
    bool expected = false;
    if (!busy_flag.compare_exchange_strong(expected, true)) return;
    
    join_worker();
    worker_thread = std::thread([this, hostname, port]() {
        struct BusyReset { std::atomic<bool>& flag; ~BusyReset(){ flag = false; } } reset{busy_flag};
        std::lock_guard<std::recursive_mutex> lock(session_mutex);
        close_shell_channel();
        authenticated_flag = false;

        const char* home = getenv("HOME");
        if (home) {
            std::string known_hosts = std::string(home) + "/.ssh/known_hosts";
            ssh_options_set(my_session, SSH_OPTIONS_KNOWNHOSTS, known_hosts.c_str());
        }
#ifdef SSH_OPTIONS_STRICTHOSTKEYCHECK
        int strict =
#ifdef SSH_STRICTHOSTKEYCHECK_YES
            SSH_STRICTHOSTKEYCHECK_YES;
#else
            1;
#endif
        ssh_options_set(my_session, SSH_OPTIONS_STRICTHOSTKEYCHECK, &strict);
#endif

        ssh_options_set(my_session, SSH_OPTIONS_HOST, hostname.c_str());
        ssh_options_set(my_session, SSH_OPTIONS_PORT, &port);

        int rc = ssh_connect(my_session);
        if (rc != SSH_OK) {
            set_error(ssh_get_error(my_session));
            connected_flag = false;
            return;
        }

        if (!verify_known_host()) {
            connected_flag = false;
            return;
        }

        connected_flag = true;
        set_error("");
    });
}

void SSHClient::authenticate(const std::string& user, const std::string& password, const std::string& key_path) {
    bool expected = false;
    if (!busy_flag.compare_exchange_strong(expected, true)) return;
     
    join_worker();
    worker_thread = std::thread([this, user, password, key_path]() {
        struct BusyReset { std::atomic<bool>& flag; ~BusyReset(){ flag = false; } } reset{busy_flag};
        std::lock_guard<std::recursive_mutex> lock(session_mutex);
        int rc;

        if (!connected_flag) {
            set_error("Cannot authenticate: not connected");
            authenticated_flag = false;
            return;
        }
        authenticated_flag = false;

        ssh_options_set(my_session, SSH_OPTIONS_USER, user.c_str());

        // Try Public Key Auto (Agent or Default keys) first
        rc = ssh_userauth_publickey_auto(my_session, NULL, NULL);
        if (rc == SSH_AUTH_SUCCESS) {
            authenticated_flag = true;
            set_error("");
            return;
        }

        // Try specific key if provided
        if (!key_path.empty()) {
            ssh_key privkey = NULL;
            int rc_key = ssh_pki_import_privkey_file(key_path.c_str(), NULL, NULL, NULL, &privkey);
            
            if (rc_key == SSH_OK) {
                rc = ssh_userauth_publickey(my_session, NULL, privkey);
                ssh_key_free(privkey);
                
                if (rc == SSH_AUTH_SUCCESS) {
                    authenticated_flag = true;
                    set_error("");
                    return;
                }
            }
        }

        // Try Password
        if (!password.empty()) {
            rc = ssh_userauth_password(my_session, NULL, password.c_str());
            if (rc == SSH_AUTH_SUCCESS) {
                authenticated_flag = true;
                set_error("");
                return;
            }
        }

        set_error("Authentication failed: " + std::string(ssh_get_error(my_session)));
        authenticated_flag = false;
    });
}

bool SSHClient::init_shell() {
    if (!connected_flag || !authenticated_flag) return false;
    std::lock_guard<std::recursive_mutex> lock(session_mutex);
    
    close_shell_channel();
    shell_channel = ssh_channel_new(my_session);
    if (shell_channel == NULL) return false;

    if (ssh_channel_open_session(shell_channel) != SSH_OK) {
        close_shell_channel();
        return false;
    }

    if (ssh_channel_request_pty(shell_channel) != SSH_OK) {
        close_shell_channel();
        return false;
    }

    ssh_channel_change_pty_size(shell_channel, 80, 24);

    if (ssh_channel_request_shell(shell_channel) != SSH_OK) {
        close_shell_channel();
        return false;
    }

    return true;
}

void SSHClient::send_shell_command(const std::string& cmd) {
    std::lock_guard<std::recursive_mutex> lock(session_mutex);
    if (shell_channel && ssh_channel_is_open(shell_channel)) {
        // Raw send (cmd contains control codes or newlines if needed)
        ssh_channel_write(shell_channel, cmd.c_str(), cmd.size());
    }
}

std::string SSHClient::read_shell_output() {
    std::lock_guard<std::recursive_mutex> lock(session_mutex);
    if (!shell_channel || !ssh_channel_is_open(shell_channel)) return "";

    char buffer[4096];
    int nbytes = ssh_channel_read_nonblocking(shell_channel, buffer, sizeof(buffer), 0);
    
    if (nbytes > 0) {
        return std::string(buffer, nbytes);
    }
    return "";
}

std::string SSHClient::exec_command_sync(const std::string& cmd) {
    std::lock_guard<std::recursive_mutex> lock(session_mutex);
    if (!is_connected() || !is_authenticated()) return "";

    ssh_channel channel = ssh_channel_new(my_session);
    if (channel == NULL) return "";

    if (ssh_channel_open_session(channel) != SSH_OK) {
        ssh_channel_free(channel);
        return "";
    }

    if (ssh_channel_request_exec(channel, cmd.c_str()) != SSH_OK) {
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return "";
    }

    std::string output;
    char buffer[1024];
    int nbytes;

    while ((nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0)) > 0) {
        output.append(buffer, nbytes);
    }

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);

    return output;
}

bool SSHClient::verify_known_host() {
    int state = ssh_session_is_known_server(my_session);
    switch (state) {
        case SSH_KNOWN_HOSTS_OK:
            return true;
        case SSH_KNOWN_HOSTS_CHANGED:
        case SSH_KNOWN_HOSTS_OTHER:
#ifdef SSH_KNOWN_HOSTS_REVOKED
        case SSH_KNOWN_HOSTS_REVOKED:
#endif
            set_error("Host key mismatch or revoked. Connection aborted.");
            ssh_disconnect(my_session);
            return false;
        case SSH_KNOWN_HOSTS_NOT_FOUND:
        case SSH_KNOWN_HOSTS_UNKNOWN:
            set_error("Unknown host key. Add the host to known_hosts before connecting.");
            ssh_disconnect(my_session);
            return false;
        case SSH_KNOWN_HOSTS_ERROR:
        default:
            set_error("Failed to verify host key: " + std::string(ssh_get_error(my_session)));
            ssh_disconnect(my_session);
            return false;
    }
}

void SSHClient::close_shell_channel() {
    if (shell_channel) {
        if (ssh_channel_is_open(shell_channel)) {
            ssh_channel_send_eof(shell_channel);
            ssh_channel_close(shell_channel);
        }
        ssh_channel_free(shell_channel);
        shell_channel = NULL;
    }
}

void SSHClient::join_worker() {
    if (worker_thread.joinable()) {
        worker_thread.join();
    }
}
