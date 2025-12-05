#include "SFTPClient.h"
#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>

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

std::string SFTPClient::read_file(const std::string& path) {
    if (!sftp) return "";

    std::lock_guard<std::mutex> local_lock(local_mutex);
    if (session_mutex) session_mutex->lock();

    sftp_file file = sftp_open(sftp, path.c_str(), O_RDONLY, 0);
    if (!file) {
        if (session_mutex) session_mutex->unlock();
        return "";
    }

    std::string content;
    char buffer[1024];
    ssize_t nbytes;

    while ((nbytes = sftp_read(file, buffer, sizeof(buffer))) > 0) {
        content.append(buffer, nbytes);
    }

    sftp_close(file);
    
    if (session_mutex) session_mutex->unlock();
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

bool SFTPClient::delete_path(const std::string& path, bool is_dir) {
    if (!sftp) return false;
    std::lock_guard<std::mutex> local_lock(local_mutex);
    if (session_mutex) session_mutex->lock();
    int rc = is_dir ? sftp_rmdir(sftp, path.c_str()) : sftp_unlink(sftp, path.c_str());
    if (session_mutex) session_mutex->unlock();
    return rc == SSH_OK;
}
