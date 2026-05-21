#if defined(_WIN32)

#include "CredentialStore.h"

#include <windows.h>
#include <wincred.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "advapi32.lib")

namespace {

std::wstring TargetName(const SSHHost& host) {
    std::string alias = host.alias.empty() ? host.hostname : host.alias;
    std::string s = "ShadowSSH:" + alias + "|" + host.user + "|" + host.port;
    int len = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(len, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), len);
    return w;
}

std::vector<uint8_t> Pack(const std::string& password, const std::string& key_path) {
    uint32_t pl = (uint32_t)password.size();
    std::vector<uint8_t> out(sizeof(uint32_t) + password.size() + key_path.size());
    std::memcpy(out.data(), &pl, sizeof(uint32_t));
    std::memcpy(out.data() + sizeof(uint32_t), password.data(), password.size());
    if (!key_path.empty()) {
        std::memcpy(out.data() + sizeof(uint32_t) + password.size(), key_path.data(), key_path.size());
    }
    return out;
}

bool Unpack(const uint8_t* data, size_t len, std::string& password, std::string& key_path) {
    if (len < sizeof(uint32_t)) return false;
    uint32_t pl = 0;
    std::memcpy(&pl, data, sizeof(uint32_t));
    if (len < sizeof(uint32_t) + pl) return false;
    password.assign((const char*)data + sizeof(uint32_t), pl);
    key_path.assign((const char*)data + sizeof(uint32_t) + pl, len - sizeof(uint32_t) - pl);
    return true;
}

} // namespace

namespace CredentialStore {

bool Save(const SSHHost& host, const std::string& password, const std::string& key_path) {
    if (password.empty() && key_path.empty()) return true;

    std::wstring target = TargetName(host);
    std::vector<uint8_t> blob = Pack(password, key_path);

    CREDENTIALW cred{};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = const_cast<wchar_t*>(target.c_str());
    cred.CredentialBlob = blob.data();
    cred.CredentialBlobSize = (DWORD)blob.size();
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;

    return ::CredWriteW(&cred, 0) != FALSE;
}

bool Load(const SSHHost& host, std::string& out_password, std::string& out_key_path) {
    std::wstring target = TargetName(host);
    PCREDENTIALW cred = nullptr;
    if (!::CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &cred)) return false;
    bool ok = Unpack((const uint8_t*)cred->CredentialBlob, cred->CredentialBlobSize,
                     out_password, out_key_path);
    ::CredFree(cred);
    return ok;
}

void Delete(const SSHHost& host) {
    std::wstring target = TargetName(host);
    ::CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0);
}

} // namespace CredentialStore

#endif
