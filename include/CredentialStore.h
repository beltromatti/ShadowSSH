#pragma once
#include "SSHStructs.h"
#include <string>

// Persists SSH credentials in the most-secure store each OS provides:
//  - macOS  -> Keychain
//  - Linux  -> file under $XDG_CONFIG_HOME/shadowssh with 0600 perms + XOR mask
//  - Windows-> Credential Manager (DPAPI-encrypted)
namespace CredentialStore {

bool Save(const SSHHost& host, const std::string& password, const std::string& key_path);
bool Load(const SSHHost& host, std::string& out_password, std::string& out_key_path);
void Delete(const SSHHost& host);

} // namespace CredentialStore
