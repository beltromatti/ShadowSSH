#include "SFTPClient.h"
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

SFTPClient::SFTPClient() {}

SFTPClient::~SFTPClient() {
    cleanup();
}

void SFTPClient::cleanup() {
    if (sftp) {
        sftp_free(sftp);
        sftp = NULL;
    }
}

bool SFTPClient::init(ssh_session session, std::recursive_mutex* mutex) {
    if (sftp) cleanup();
    
    this->session_mutex = mutex;
    
    // Lock creation
    if (session_mutex) session_mutex->lock();
    
    sftp = sftp_new(session);
    if (sftp == NULL) {
        if (session_mutex) session_mutex->unlock();
        return false;
    }

    int rc = sftp_init(sftp);
    if (rc != SSH_OK) {
        sftp_free(sftp);
        sftp = NULL;
        if (session_mutex) session_mutex->unlock();
        return false;
    }
    
    if (session_mutex) session_mutex->unlock();
    return true;
}

std::vector<RemoteFile> SFTPClient::list_directory(const std::string& path) {
    std::vector<RemoteFile> files;
    if (!sftp) return files;

    std::lock_guard<std::mutex> local_lock(local_mutex);
    if (session_mutex) session_mutex->lock();
    
    sftp_dir dir = sftp_opendir(sftp, path.c_str());
    if (!dir) {
        if (session_mutex) session_mutex->unlock();
        return files;
    }

    sftp_attributes attributes;
    while ((attributes = sftp_readdir(sftp, dir)) != NULL) {
        RemoteFile rf;
        rf.name = attributes->name;
        rf.is_dir = (attributes->type == SSH_FILEXFER_TYPE_DIRECTORY);
        rf.size = attributes->size;
        
        files.push_back(rf);
        
        sftp_attributes_free(attributes);
    }
    sftp_closedir(dir);
    
    if (session_mutex) session_mutex->unlock();
    return files;
}

bool SFTPClient::read_file(const std::string& path, std::string& out) {
    if (!sftp) return false;

    std::lock_guard<std::mutex> local_lock(local_mutex);
    if (session_mutex) session_mutex->lock();

    sftp_file file = sftp_open(sftp, path.c_str(), O_RDONLY, 0);
    if (!file) {
        if (session_mutex) session_mutex->unlock();
        return false;
    }

    out.clear();
    char buffer[1024];
    ssize_t nbytes;

    while ((nbytes = sftp_read(file, buffer, sizeof(buffer))) > 0) {
        out.append(buffer, nbytes);
    }

    sftp_close(file);
    
    if (session_mutex) session_mutex->unlock();
    return nbytes == 0;
}

std::string SFTPClient::read_file(const std::string& path) {
    std::string content;
    read_file(path, content);
    return content;
}

bool SFTPClient::write_file(const std::string& path, const std::string& content) {
    if (!sftp) return false;

    std::lock_guard<std::mutex> local_lock(local_mutex);
    if (session_mutex) session_mutex->lock();

    sftp_file file = sftp_open(sftp, path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!file) {
        if (session_mutex) session_mutex->unlock();
        return false;
    }

    ssize_t nwritten = sftp_write(file, content.c_str(), content.size());
    sftp_close(file);

    if (session_mutex) session_mutex->unlock();
    return (nwritten == (ssize_t)content.size());
}

bool SFTPClient::download_file(const std::string& remote_path, const std::string& local_path) {
    if (!sftp) return false;

    std::lock_guard<std::mutex> local_lock(local_mutex);
    if (session_mutex) session_mutex->lock();

    sftp_file file = sftp_open(sftp, remote_path.c_str(), O_RDONLY, 0);
    if (!file) {
        if (session_mutex) session_mutex->unlock();
        return false;
    }

    int fd = ::open(local_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        sftp_close(file);
        if (session_mutex) session_mutex->unlock();
        return false;
    }

    char buffer[4096];
    ssize_t nread = 0;
    bool ok = true;
    while ((nread = sftp_read(file, buffer, sizeof(buffer))) > 0) {
        ssize_t written = ::write(fd, buffer, nread);
        if (written != nread) {
            ok = false;
            break;
        }
    }
    if (nread < 0) ok = false;

    ::close(fd);
    sftp_close(file);
    if (session_mutex) session_mutex->unlock();
    return ok;
}

bool SFTPClient::delete_path(const std::string& path, bool is_dir) {
    if (!sftp) return false;
    std::lock_guard<std::mutex> local_lock(local_mutex);
    if (session_mutex) session_mutex->lock();
    int rc = is_dir ? sftp_rmdir(sftp, path.c_str()) : sftp_unlink(sftp, path.c_str());
    if (session_mutex) session_mutex->unlock();
    return rc == SSH_OK;
}
