#pragma once
#include <SDL.h>
#include "imgui.h"
#include "SSHClient.h"
#include "SFTPClient.h"
#include "SSHConfigParser.h"
#include "SystemMonitor.h" // Added
#include "EditorManager.h" // Added
#include "Terminal.h"
#include <vector>
#include <string>

enum class AppState {
    LOGIN,
    CONNECTED
};

class Application {
public:
    Application();
    ~Application();

    bool Init();
    void Run();
    void Cleanup();

private:
    // Core
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool running = true;

    // Logic
    AppState state = AppState::LOGIN;
    SSHClient sshClient;
    SFTPClient sftpClient;
    SystemMonitor monitor; // Added
    std::vector<SSHHost> known_hosts;
    std::vector<SSHHost> history_hosts;
    std::string history_path;
    
    // Login UI State
    char host_input[128] = "";
    char port_input[128] = "22";
    char user_input[128] = "";
    char pass_input[128] = "";
    char key_path_input[512] = "";
    char status_msg[256] = "Ready";
    
    // File Browser State
    std::vector<RemoteFile> current_files;
    std::string current_path = ".";
    std::vector<std::string> path_history;
    int history_index = -1;
    bool files_need_refresh = false;
    int selected_file_index = -1;

    // Editor State
    EditorManager editorManager;

    // Terminal State
    Terminal terminal;
    bool shell_ready = false;

    // Helpers
    void ApplyDarkTheme();
    void RenderLogin();
    void RenderWorkspace();
    void RenderFileBrowser();
    void RenderEditor();
    void RenderTerminal();
    void RenderMonitor(); // Added
    
    void RefreshFileList();
    void OpenFile(const std::string& filename);
    void SaveFile();
    void LaunchNativeTerminal();

    // State for external terminal
    bool terminal_launched = false;
};
