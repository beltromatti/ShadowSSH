#include "CredentialStore.h"
#include <Security/Security.h>
#include <vector>
#include <cstring>

namespace {
std::string make_account_key(const SSHHost& host) {
    // Build a stable identifier per host/user/port combination
    std::string alias = host.alias.empty() ? host.hostname : host.alias;
    return alias + "|" + host.user + "|" + host.port;
}

std::vector<uint8_t> serialize_secret(const std::string& password, const std::string& key_path) {
    // Simple length-prefixed encoding: [pass_len][pass][key]
    uint32_t pass_len = static_cast<uint32_t>(password.size());
    std::vector<uint8_t> data(sizeof(uint32_t) + password.size() + key_path.size());
    std::memcpy(data.data(), &pass_len, sizeof(uint32_t));
    std::memcpy(data.data() + sizeof(uint32_t), password.data(), password.size());
    if (!key_path.empty()) {
        std::memcpy(data.data() + sizeof(uint32_t) + password.size(), key_path.data(), key_path.size());
    }
    return data;
}

bool deserialize_secret(const std::vector<uint8_t>& data, std::string& out_pass, std::string& out_key) {
    if (data.size() < sizeof(uint32_t)) return false;
    uint32_t pass_len = 0;
    std::memcpy(&pass_len, data.data(), sizeof(uint32_t));
    if (data.size() < sizeof(uint32_t) + pass_len) return false;
    out_pass.assign(reinterpret_cast<const char*>(data.data() + sizeof(uint32_t)), pass_len);
    size_t key_len = data.size() - sizeof(uint32_t) - pass_len;
    out_key.assign(reinterpret_cast<const char*>(data.data() + sizeof(uint32_t) + pass_len), key_len);
    return true;
}

CFDictionaryRef build_query(const std::string& account, bool returnData) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(dict, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(dict, kSecAttrService, CFSTR("com.shadowssh.credentials"));
    CFStringRef accountRef = CFStringCreateWithCString(NULL, account.c_str(), kCFStringEncodingUTF8);
    CFDictionarySetValue(dict, kSecAttrAccount, accountRef);
    if (returnData) {
        CFDictionarySetValue(dict, kSecMatchLimit, kSecMatchLimitOne);
        CFDictionarySetValue(dict, kSecReturnData, kCFBooleanTrue);
    }
    CFRelease(accountRef);
    return dict;
}
} // namespace

namespace CredentialStore {

bool Save(const SSHHost& host, const std::string& password, const std::string& key_path) {
    // Nothing to save
    if (password.empty() && key_path.empty()) return true;

    std::string account = make_account_key(host);
    std::vector<uint8_t> data = serialize_secret(password, key_path);
    CFDataRef valueData = CFDataCreate(NULL, data.data(), data.size());

    CFMutableDictionaryRef updateQuery = (CFMutableDictionaryRef)build_query(account, false);
    CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(attrs, kSecValueData, valueData);
    OSStatus upd = SecItemUpdate(updateQuery, attrs);
    CFRelease(attrs);

    if (upd == errSecSuccess) {
        CFRelease(valueData);
        CFRelease(updateQuery);
        return true;
    }

    if (upd != errSecItemNotFound) {
        CFRelease(valueData);
        CFRelease(updateQuery);
        return false;
    }

    // Add new item
    CFDictionarySetValue(updateQuery, kSecValueData, valueData);
    CFDictionarySetValue(updateQuery, kSecAttrAccessible, kSecAttrAccessibleAfterFirstUnlock);
    OSStatus addStatus = SecItemAdd(updateQuery, nullptr);
    CFRelease(valueData);
    CFRelease(updateQuery);
    return addStatus == errSecSuccess;
}

bool Load(const SSHHost& host, std::string& out_password, std::string& out_key_path) {
    std::string account = make_account_key(host);
    CFDictionaryRef query = build_query(account, true);
    CFTypeRef result = nullptr;
    OSStatus status = SecItemCopyMatching(query, &result);
    CFRelease(query);
    if (status != errSecSuccess || result == nullptr) return false;

    CFDataRef dataRef = (CFDataRef)result;
    std::vector<uint8_t> data(CFDataGetLength(dataRef));
    CFDataGetBytes(dataRef, CFRangeMake(0, data.size()), data.data());
    CFRelease(result);

    return deserialize_secret(data, out_password, out_key_path);
}

void Delete(const SSHHost& host) {
    std::string account = make_account_key(host);
    CFDictionaryRef query = build_query(account, false);
    SecItemDelete(query);
    CFRelease(query);
}

} // namespace CredentialStore
