#include "SystemMonitor.h"
#include "imgui.h"
#include <sstream>
#include <vector>
#include <iostream>
#include <chrono>

SystemMonitor::SystemMonitor() {}

SystemMonitor::~SystemMonitor() {
    Stop();
}

void SystemMonitor::Start(const std::string& host, int port, const std::string& user, const std::string& pass, const std::string& key) {
    if (running) return;
    
    m_host = host;
    m_port = port;
    m_user = user;
    m_pass = pass;
    m_key = key;
    
    running = true;
    monitor_thread = std::thread(&SystemMonitor::PollLoop, this);
}

void SystemMonitor::Stop() {
    running = false;
    // Disconnect client to break any blocking reads
    client.disconnect();
    
    if (monitor_thread.joinable()) {
        monitor_thread.join();
    }
}

void SystemMonitor::PollLoop() {
    // 1. Connect
    client.connect(m_host, m_port);
    
    // Wait for connection (simple timeout loop)
    int retries = 0;
    while (running && !client.is_connected() && retries < 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        retries++;
    }
    
    if (!running || !client.is_connected()) return;

    // 2. Authenticate
    client.authenticate(m_user, m_pass, m_key);
    
    retries = 0;
    while (running && !client.is_authenticated() && retries < 50) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        retries++;
    }

    if (!running || !client.is_authenticated()) return;

    // 3. Poll Loop
    while (running) {
        // Composite command
        std::string cmd = "echo '>>LOAD'; cat /proc/loadavg; echo '>>MEM'; free -m; echo '>>DISK'; df -P /; echo '>>NET'; cat /proc/net/dev; echo '>>UP'; uptime -p";
        
        std::string output = client.exec_command_sync(cmd);
        
        if (output.empty() && !client.is_connected()) {
            break; // Connection lost
        }

        {
            std::lock_guard<std::mutex> lock(data_mutex);
            ParseData(output);
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    client.disconnect();
}

// Helper to get timestamp
static double GetTime() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() / 1000.0;
}

void SystemMonitor::ParseData(const std::string& raw_data) {
    std::stringstream ss(raw_data);
    std::string line;
    std::string section;

    while (std::getline(ss, line)) {
        if (line.find(">>") == 0) {
            section = line.substr(2);
            continue;
        }
        if (line.empty()) continue;

        std::stringstream ls(line);
        
        if (section == "LOAD") {
            ls >> stats.load_avg;
            stats.cpu_usage = stats.load_avg * 25.0f; // Fake scale for visualization
            if(stats.cpu_usage > 100.0f) stats.cpu_usage = 100.0f;
        }
        else if (section == "MEM") {
            if (line.find("Mem:") != std::string::npos) {
                std::string label;
                ls >> label >> stats.ram_total >> stats.ram_used;
            }
        }
        else if (section == "DISK") {
            if (line.find("/dev/") != std::string::npos || line.find("rootfs") != std::string::npos || line.find("overlay") != std::string::npos) {
                std::string fs, size, used, avail, cap, mount;
                ls >> fs >> size >> used >> avail >> cap >> mount;
                try {
                    stats.disk_total = std::stoull(size);
                    stats.disk_used = std::stoull(used);
                } catch(...) {}
            }
        }
        else if (section == "NET") {
            if (line.find("eth0:") != std::string::npos || line.find("enp") != std::string::npos || line.find("ens") != std::string::npos) {
                 uint64_t rx, tx;
                 size_t colon = line.find(':');
                 if (colon != std::string::npos) {
                     std::stringstream ns(line.substr(colon + 1));
                     ns >> rx;
                     for(int i=0; i<7; i++) { std::string tmp; ns >> tmp; }
                     ns >> tx;

                     double now = GetTime();
                     double dt = now - last_poll_time;
                     if (dt > 0 && last_poll_time > 0) {
                         if (rx >= last_net_bytes_rx) stats.net_down_speed = (float)((rx - last_net_bytes_rx) / 1024.0 / dt);
                         if (tx >= last_net_bytes_tx) stats.net_up_speed = (float)((tx - last_net_bytes_tx) / 1024.0 / dt);
                     }
                     
                     last_net_bytes_rx = rx;
                     last_net_bytes_tx = tx;
                     last_poll_time = now;
                 }
            }
        }
        else if (section == "UP") {
            stats.uptime_str = line;
        }
    }
}

void SystemMonitor::Render() {
    std::lock_guard<std::mutex> lock(data_mutex);
    
    ImGui::Begin("System Monitor");
    
    // Grid Layout
    if (ImGui::BeginTable("StatsTable", 2, ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("CPU & Memory", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Disk & Network", ImGuiTableColumnFlags_WidthStretch);
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        
        // CPU / Load
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "System Load (Avg)");
        ImGui::ProgressBar(stats.cpu_usage / 100.0f, ImVec2(-1, 0), std::to_string(stats.load_avg).c_str());
        
        ImGui::Spacing();
        
        // RAM
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Memory Usage");
        char ram_overlay[64];
        sprintf(ram_overlay, "%llu / %llu MB", stats.ram_used, stats.ram_total);
        float ram_fraction = (stats.ram_total > 0) ? (float)stats.ram_used / (float)stats.ram_total : 0.0f;
        ImGui::ProgressBar(ram_fraction, ImVec2(-1, 0), ram_overlay);
        
        ImGui::Spacing();
        ImGui::Text("Uptime: %s", stats.uptime_str.c_str());

        ImGui::TableNextColumn();
        
        // Disk
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Disk Usage (Root)");
        char disk_overlay[64];
        float disk_fraction = (stats.disk_total > 0) ? (float)stats.disk_used / (float)stats.disk_total : 0.0f;
        sprintf(disk_overlay, "%.1f%%", disk_fraction * 100.0f);
        ImGui::ProgressBar(disk_fraction, ImVec2(-1, 0), disk_overlay);
        
        ImGui::Spacing();
        
        // Network
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 1.0f, 1.0f), "Network Traffic");
        ImGui::Text("Down: %.1f KB/s", stats.net_down_speed);
        ImGui::SameLine(150);
        ImGui::Text("Up:   %.1f KB/s", stats.net_up_speed);

        ImGui::EndTable();
    }

    ImGui::End();
}