#pragma once
#include "SSHStructs.h"
#include <string>

namespace CredentialStore {
// Persist credentials in the macOS Keychain (encrypted by the OS).
// We store password and optional key path for a given host/user tuple.
bool Save(const SSHHost& host, const std::string& password, const std::string& key_path);
bool Load(const SSHHost& host, std::string& out_password, std::string& out_key_path);
void Delete(const SSHHost& host);
}
