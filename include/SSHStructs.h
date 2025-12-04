#pragma once
#include <string>
#include <vector>

struct SSHHost {
    std::string alias;      // "Host" in config
    std::string hostname;   // "HostName" (IP or domain)
    std::string user;       // "User"
    std::string port = "22";
    std::string identity_file;
    
    // For manual entries or history
    std::string last_connected; 
};

struct SSHConnectionState {
    bool is_connected = false;
    std::string error_message;
    bool authenticated = false;
};
