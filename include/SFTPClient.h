#pragma once
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <string>
#include <vector>
#include <mutex>

struct RemoteFile {
    std::string name;
    bool is_dir;
    uint64_t size;
};

class SFTPClient {
public:
    SFTPClient();
    ~SFTPClient();

    bool init(ssh_session session, std::recursive_mutex* mutex);
    void cleanup();

    std::vector<RemoteFile> list_directory(const std::string& path);
    bool read_file(const std::string& path, std::string& out);
    std::string read_file(const std::string& path);
    bool write_file(const std::string& path, const std::string& content);
    bool delete_path(const std::string& path, bool is_dir);
    bool download_file(const std::string& remote_path, const std::string& local_path);

    std::string get_current_path() { return current_path; }
    void set_current_path(const std::string& path) { current_path = path; }

    bool is_ready() { return sftp != NULL; }

private:
    sftp_session sftp = NULL;
    std::string current_path = ".";
    std::recursive_mutex* session_mutex = nullptr; // Pointer to SSHClient's mutex
    std::mutex local_mutex; 
};
