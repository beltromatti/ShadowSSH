#pragma once
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include "SSHClient.h"

struct ServerStats {
    float load_avg = 0.0f;
    float cpu_usage = 0.0f;
    
    uint64_t ram_total = 1;
    uint64_t ram_used = 0;
    
    uint64_t disk_total = 1;
    uint64_t disk_used = 0;
    
    float net_down_speed = 0.0f; // KB/s
    float net_up_speed = 0.0f;
    
    std::string uptime_str;
};

class SystemMonitor {
public:
    SystemMonitor();
    ~SystemMonitor();

    void Start(const std::string& host, int port, const std::string& user, const std::string& pass, const std::string& key);
    void Stop();
    void Render();

private:
    SSHClient client; // Dedicated client for monitoring
    std::atomic<bool> running{false};
    std::thread monitor_thread;
    std::mutex data_mutex;
    
    ServerStats stats;
    
    // Credentials for internal connection
    std::string m_host, m_user, m_pass, m_key;
    int m_port;

    // Helpers for parsing
    void PollLoop();
    void ParseData(const std::string& raw_data);
    
    // State for calculation
    uint64_t last_net_bytes_rx = 0;
    uint64_t last_net_bytes_tx = 0;
    double last_poll_time = 0.0;
};
