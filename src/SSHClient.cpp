#include "SSHClient.h"
#include <iostream>

SSHClient::SSHClient() {
    my_session = ssh_new();
    if (my_session == NULL) {
        // Critical failure
        std::cerr << "Error allocating SSH session" << std::endl;
    }
}

SSHClient::~SSHClient() {
    disconnect();
    ssh_free(my_session);
}

void SSHClient::disconnect() {
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
    return busy_flag;
}

void SSHClient::connect(const std::string& hostname, int port) {
    if (busy_flag) return;
    
    busy_flag = true;
    // Run in a detached thread to not block UI
    std::thread([this, hostname, port]() {
        std::lock_guard<std::recursive_mutex> lock(session_mutex);
        ssh_options_set(my_session, SSH_OPTIONS_HOST, hostname.c_str());
        ssh_options_set(my_session, SSH_OPTIONS_PORT, &port);

        int rc = ssh_connect(my_session);
        if (rc != SSH_OK) {
            set_error(ssh_get_error(my_session));
            connected_flag = false;
        } else {
            // Verify server known (skipping for minimal prototype, usually requires known_hosts check)
            // For now, we assume "Trust On First Use" logic or just ignore for prototype speed
            connected_flag = true;
            set_error("");
        }
        busy_flag = false;
    }).detach();
}

void SSHClient::authenticate(const std::string& user, const std::string& password, const std::string& key_path) {
     if (busy_flag) return;
     
     busy_flag = true;
     std::thread([this, user, password, key_path]() {
        std::lock_guard<std::recursive_mutex> lock(session_mutex);
        int rc;
        ssh_options_set(my_session, SSH_OPTIONS_USER, user.c_str());

        // Try Public Key Auto (Agent or Default keys) first
        rc = ssh_userauth_publickey_auto(my_session, NULL, NULL);
        if (rc == SSH_AUTH_SUCCESS) {
            authenticated_flag = true;
            busy_flag = false;
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
                    busy_flag = false;
                    return;
                }
            }
        }

        // Try Password
        if (!password.empty()) {
            rc = ssh_userauth_password(my_session, NULL, password.c_str());
            if (rc == SSH_AUTH_SUCCESS) {
                authenticated_flag = true;
                busy_flag = false;
                return;
            }
        }

        set_error("Authentication failed: " + std::string(ssh_get_error(my_session)));
        authenticated_flag = false;
        busy_flag = false;
     }).detach();
}

bool SSHClient::init_shell() {
    if (!connected_flag || !authenticated_flag) return false;
    
    shell_channel = ssh_channel_new(my_session);
    if (shell_channel == NULL) return false;

    if (ssh_channel_open_session(shell_channel) != SSH_OK) {
        ssh_channel_free(shell_channel);
        return false;
    }

    if (ssh_channel_request_pty(shell_channel) != SSH_OK) {
        ssh_channel_close(shell_channel);
        ssh_channel_free(shell_channel);
        return false;
    }

    if (ssh_channel_change_pty_size(shell_channel, 80, 24) != SSH_OK) {
        // Not critical
    }

    if (ssh_channel_request_shell(shell_channel) != SSH_OK) {
        ssh_channel_close(shell_channel);
        ssh_channel_free(shell_channel);
        return false;
    }

    return true;
}

void SSHClient::send_shell_command(const std::string& cmd) {
    if (shell_channel && ssh_channel_is_open(shell_channel)) {
        // Raw send (cmd contains control codes or newlines if needed)
        ssh_channel_write(shell_channel, cmd.c_str(), cmd.size());
    }
}

std::string SSHClient::read_shell_output() {
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
