#include "SSHConfigParser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

// Helper to trim whitespace
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t");
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t");
    return str.substr(first, (last - first + 1));
}

std::vector<SSHHost> SSHConfigParser::parse_config_file(const std::string& path) {
    std::vector<SSHHost> hosts;
    std::ifstream file(path);
    
    if (!file.is_open()) {
        return hosts;
    }

    std::string line;
    SSHHost current_host;
    bool parsing_host = false;

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        std::stringstream ss(line);
        std::string keyword, value;
        ss >> keyword;
        
        // Get the rest of the line as value (handling spaces if needed, though usually SSH config is simple)
        std::getline(ss, value);
        value = trim(value);

        // Case insensitive check for keywords could be better, but standard is Case Sensitive keywords often or mixed.
        // OpenSSH config is case-insensitive for keywords.
        std::transform(keyword.begin(), keyword.end(), keyword.begin(), ::tolower);

        if (keyword == "host") {
            // If we were parsing a host, push it (unless it was a wildcard which we might skip or handle differently)
            if (parsing_host && !current_host.alias.empty() && current_host.alias.find('*') == std::string::npos) {
                if (current_host.hostname.empty()) current_host.hostname = current_host.alias; // Fallback
                hosts.push_back(current_host);
            }
            
            // Start new host
            current_host = SSHHost();
            current_host.alias = value;
            parsing_host = true;
        } else if (parsing_host) {
            if (keyword == "hostname") current_host.hostname = value;
            else if (keyword == "user") current_host.user = value;
            else if (keyword == "port") current_host.port = value;
            else if (keyword == "identityfile") {
                // Handle tilde expansion if simple
                if (value.size() > 0 && value[0] == '~') {
                     const char* home = getenv("HOME");
                     if (home) {
                         value.replace(0, 1, home);
                     }
                }
                current_host.identity_file = value;
            }
        }
    }

    // Push last one
    if (parsing_host && !current_host.alias.empty() && current_host.alias.find('*') == std::string::npos) {
        if (current_host.hostname.empty()) current_host.hostname = current_host.alias;
        hosts.push_back(current_host);
    }

    return hosts;
}

// Basic CSV implementation for history
std::vector<SSHHost> SSHConfigParser::load_history(const std::string& path) {
    std::vector<SSHHost> hosts;
    std::ifstream file(path);
    if (!file.is_open()) return hosts;

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string segment;
        std::vector<std::string> seglist;
        while(std::getline(ss, segment, '|')) {
            seglist.push_back(segment);
        }
        if (seglist.size() >= 4) {
            SSHHost host;
            host.alias = seglist[0];
            host.hostname = seglist[1];
            host.user = seglist[2];
            host.port = seglist[3];
            hosts.push_back(host);
        }
    }
    return hosts;
}

void SSHConfigParser::save_history(const std::string& path, const SSHHost& host) {
    // Read existing to avoid duplicates (simple check)
    std::vector<SSHHost> existing = load_history(path);
    for (const auto& h : existing) {
        if (h.hostname == host.hostname && h.user == host.user && h.port == host.port) {
            return; // Already in history
        }
    }

    std::ofstream file(path, std::ios::app);
    if (file.is_open()) {
        file << host.alias << "|" << host.hostname << "|" << host.user << "|" << host.port << "\n";
    }
}
