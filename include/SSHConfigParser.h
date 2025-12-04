#pragma once
#include "SSHStructs.h"
#include <vector>
#include <string>

class SSHConfigParser {
public:
    static std::vector<SSHHost> parse_config_file(const std::string& path);
    static std::vector<SSHHost> load_history(const std::string& path);
    static void save_history(const std::string& path, const SSHHost& host);
};
