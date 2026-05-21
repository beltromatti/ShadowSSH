#if defined(__linux__) || (defined(__unix__) && !defined(__APPLE__))

#include "CredentialStore.h"
#include "Platform.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <vector>

// Linux fallback: store credentials in $XDG_CONFIG_HOME/shadowssh/credentials
// with 0600 perms. Values are XOR-masked with a key derived from the host tuple
// (defeats casual reading without pulling in libsecret). This is intentionally
// pragmatic - users wanting an OS-native store can build with libsecret and
// extend this file. The file never leaves the user's home dir.

namespace {

std::string AccountKey(const SSHHost& host) {
    std::string alias = host.alias.empty() ? host.hostname : host.alias;
    return alias + "|" + host.user + "|" + host.port;
}

std::filesystem::path StorePath() {
    return std::filesystem::path(Platform::GetConfigDir()) / "credentials";
}

void Mask(std::string& data, const std::string& key) {
    if (key.empty()) return;
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = (char)((uint8_t)data[i] ^ (uint8_t)key[i % key.size()]);
    }
}

std::string Hex(const std::string& in) {
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(in.size() * 2);
    for (unsigned char c : in) {
        out.push_back(hex[c >> 4]);
        out.push_back(hex[c & 0xF]);
    }
    return out;
}

bool Unhex(const std::string& in, std::string& out) {
    if (in.size() % 2) return false;
    out.clear();
    out.reserve(in.size() / 2);
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + c - 'a';
        if (c >= 'A' && c <= 'F') return 10 + c - 'A';
        return -1;
    };
    for (size_t i = 0; i < in.size(); i += 2) {
        int hi = val(in[i]), lo = val(in[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back((char)((hi << 4) | lo));
    }
    return true;
}

struct Entry {
    std::string account;
    std::string masked_password;
    std::string masked_key_path;
};

std::vector<Entry> ReadAll() {
    std::vector<Entry> entries;
    std::ifstream f(StorePath());
    if (!f) return entries;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        Entry e;
        std::getline(ss, e.account, '\t');
        std::getline(ss, e.masked_password, '\t');
        std::getline(ss, e.masked_key_path, '\t');
        if (!e.account.empty()) entries.push_back(std::move(e));
    }
    return entries;
}

bool WriteAll(const std::vector<Entry>& entries) {
    auto path = StorePath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream f(path, std::ios::trunc);
    if (!f) return false;
    for (const auto& e : entries) {
        f << e.account << "\t" << e.masked_password << "\t" << e.masked_key_path << "\n";
    }
    f.close();
    ::chmod(path.c_str(), 0600);
    return true;
}

} // namespace

namespace CredentialStore {

bool Save(const SSHHost& host, const std::string& password, const std::string& key_path) {
    if (password.empty() && key_path.empty()) return true;

    std::string account = AccountKey(host);
    std::string mp = password, mk = key_path;
    Mask(mp, account);
    Mask(mk, account);

    auto entries = ReadAll();
    bool found = false;
    for (auto& e : entries) {
        if (e.account == account) {
            e.masked_password = Hex(mp);
            e.masked_key_path = Hex(mk);
            found = true;
            break;
        }
    }
    if (!found) {
        entries.push_back({account, Hex(mp), Hex(mk)});
    }
    return WriteAll(entries);
}

bool Load(const SSHHost& host, std::string& out_password, std::string& out_key_path) {
    std::string account = AccountKey(host);
    auto entries = ReadAll();
    for (const auto& e : entries) {
        if (e.account != account) continue;
        std::string mp, mk;
        if (!Unhex(e.masked_password, mp) || !Unhex(e.masked_key_path, mk)) return false;
        Mask(mp, account);
        Mask(mk, account);
        out_password = mp;
        out_key_path = mk;
        return true;
    }
    return false;
}

void Delete(const SSHHost& host) {
    std::string account = AccountKey(host);
    auto entries = ReadAll();
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                  [&](const Entry& e){ return e.account == account; }), entries.end());
    WriteAll(entries);
}

} // namespace CredentialStore

#endif
