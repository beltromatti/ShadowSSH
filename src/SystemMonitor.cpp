#include "SystemMonitor.h"
#include "imgui.h"
#include <sstream>
#include <vector>
#include <iostream>
#include <chrono>
#include <cctype>
#include <algorithm>
#include <cstdio>

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
        std::string cmd =
            "LANG=C; "
            "echo '>>LOAD'; cat /proc/loadavg; "
            "echo '>>CPU'; head -n1 /proc/stat; "
            "echo '>>MEM'; cat /proc/meminfo; "
            "echo '>>DISK'; df -kP /; "
            "echo '>>NET'; cat /proc/net/dev; "
            "echo '>>UP'; cat /proc/uptime";
        
        std::string output = client.exec_command_sync(cmd);
        
        if (output.empty() && !client.is_connected()) {
            break; // Connection lost
        }

        {
            std::lock_guard<std::mutex> lock(data_mutex);
            ParseData(output);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
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
    uint64_t mem_total_kb = 0;
    uint64_t mem_avail_kb = 0;

    while (std::getline(ss, line)) {
        if (line.find(">>") == 0) {
            section = line.substr(2);
            continue;
        }
        if (line.empty()) continue;

        std::stringstream ls(line);
        
        if (section == "LOAD") {
            ls >> stats.load1 >> stats.load5 >> stats.load15;
        }
        else if (section == "CPU") {
            if (line.rfind("cpu", 0) == 0) {
                std::string label;
                uint64_t user=0,nice=0,system=0,idle=0,iowait=0,irq=0,softirq=0,steal=0;
                ls >> label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;
                double idle_all = (double)idle + (double)iowait;
                double non_idle = (double)user + (double)nice + (double)system + (double)irq + (double)softirq + (double)steal;
                double total = idle_all + non_idle;

                if (last_cpu_total > 0.0) {
                    double totald = total - last_cpu_total;
                    double idled = idle_all - last_cpu_idle;
                    if (totald > 0.0) {
                        stats.cpu_usage = (float)((totald - idled) / totald * 100.0);
                        if (stats.cpu_usage < 0) stats.cpu_usage = 0;
                        if (stats.cpu_usage > 100) stats.cpu_usage = 100;
                    }
                }
                last_cpu_total = total;
                last_cpu_idle = idle_all;
            }
        }
        else if (section == "MEM") {
            if (line.rfind("MemTotal:", 0) == 0) {
                std::string label;
                ls >> label >> mem_total_kb;
            } else if (line.rfind("MemAvailable:", 0) == 0) {
                std::string label;
                ls >> label >> mem_avail_kb;
            }
        }
        else if (section == "DISK") {
            if (line.find("/dev/") != std::string::npos || line.find("rootfs") != std::string::npos || line.find("overlay") != std::string::npos) {
                std::string fs, mount;
                uint64_t size = 0, used = 0, avail = 0;
                std::string cap;
                ls >> fs >> size >> used >> avail >> cap >> mount;
                stats.disk_total = size;
                stats.disk_used = used;
            }
        }
        else if (section == "NET") {
            // Expect lines like: eth0: 123 0 0 0 0 0 0 0 456 ...
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string iface = line.substr(0, colon);
                // trim spaces
                iface.erase(iface.begin(), std::find_if(iface.begin(), iface.end(), [](unsigned char c){return !std::isspace(c);} ));
                if (iface != "lo" && (active_iface.empty() || iface == active_iface)) {
                    uint64_t rx=0, tx=0;
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
                    active_iface = iface;
                }
            }
        }
        else if (section == "UP") {
            // /proc/uptime format: "12345.67 9876.54"
            double up_seconds = 0.0;
            ls >> up_seconds;
            int days = (int)(up_seconds / 86400);
            int hours = (int)((up_seconds - days*86400) / 3600);
            int minutes = (int)((up_seconds - days*86400 - hours*3600) / 60);
            char buf[64];
            if (days > 0)
                snprintf(buf, sizeof(buf), "%dd %dh %dm", days, hours, minutes);
            else
                snprintf(buf, sizeof(buf), "%dh %dm", hours, minutes);
            stats.uptime_str = buf;
        }
    }

    if (mem_total_kb > 0) {
        stats.ram_total = mem_total_kb / 1024; // MB
        uint64_t used_kb = mem_total_kb - mem_avail_kb;
        stats.ram_used = used_kb / 1024;
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
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "CPU / Load");
        char cpu_overlay[64];
        snprintf(cpu_overlay, sizeof(cpu_overlay), "%.0f%%  (%.2f / %.2f / %.2f)", stats.cpu_usage, stats.load1, stats.load5, stats.load15);
        ImGui::ProgressBar(stats.cpu_usage / 100.0f, ImVec2(-1, 0), cpu_overlay);
        
        ImGui::Spacing();
        
        // RAM
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Memory Usage");
        char ram_overlay[64];
        snprintf(ram_overlay, sizeof(ram_overlay), "%llu / %llu MB", stats.ram_used, stats.ram_total);
        float ram_fraction = (stats.ram_total > 0) ? (float)stats.ram_used / (float)stats.ram_total : 0.0f;
        ImGui::ProgressBar(ram_fraction, ImVec2(-1, 0), ram_overlay);
        
        ImGui::Spacing();
        ImGui::Text("Uptime: %s", stats.uptime_str.c_str());

        ImGui::TableNextColumn();
        
        // Disk
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Disk Usage (Root)");
        char disk_overlay[64];
        float disk_fraction = (stats.disk_total > 0) ? (float)stats.disk_used / (float)stats.disk_total : 0.0f;
        snprintf(disk_overlay, sizeof(disk_overlay), "%.1f%%", disk_fraction * 100.0f);
        ImGui::ProgressBar(disk_fraction, ImVec2(-1, 0), disk_overlay);
        
        ImGui::Spacing();
        
        // Network
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 1.0f, 1.0f), "Network Traffic");
        std::string iface_label = active_iface.empty() ? "auto" : active_iface;
        ImGui::Text("Iface: %s", iface_label.c_str());
        ImGui::Text("Down: %.1f KB/s", stats.net_down_speed);
        ImGui::SameLine(150);
        ImGui::Text("Up:   %.1f KB/s", stats.net_up_speed);

        ImGui::EndTable();
    }

    ImGui::End();
}
