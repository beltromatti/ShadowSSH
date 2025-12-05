#include "Application.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include "imgui_internal.h"
#include "SSHConfigParser.h"
#include "CredentialStore.h"
#ifdef __APPLE__
#include "MacMenu.h"
#endif
#include <fstream>
#include <cstdio>
#include <thread>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <pwd.h>

Application::Application() {
    // Load config
    const char* home = getenv("HOME");
    if (home) {
        std::string config_path = std::string(home) + "/.ssh/config";
        known_hosts = SSHConfigParser::parse_config_file(config_path);
        
        history_path = std::string(home) + "/.shadowssh_history_v2";
        std::filesystem::path old_history = std::string(home) + "/.shadowssh_history";
        std::error_code ec;
        std::filesystem::remove(old_history, ec); // purge legacy cleartext history
        history_hosts = SSHConfigParser::load_history(history_path);
    }
}

Application::~Application() {
    Cleanup();
}

bool Application::Init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return false;
    
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    window = SDL_CreateWindow("ShadowSSH", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    if (!window) return false;

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (!renderer) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigMacOSXBehaviors = false; // Use Ctrl for shortcuts to match requested behavior

    ApplyDarkTheme();
    
    // Load Fonts
    // Reverting to default for stability. Custom system font loading caused crashes.
    io.Fonts->AddFontDefault(); 
    
    // Debug log
    std::cout << "Fonts loaded." << std::endl;

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    return true;
}

void Application::ApplyDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_Text]                   = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_TextDisabled]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_WindowBg]               = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_PopupBg]                = ImVec4(0.11f, 0.11f, 0.14f, 0.92f);
    style.Colors[ImGuiCol_Border]                 = ImVec4(0.50f, 0.50f, 0.50f, 0.50f);
    style.Colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    style.Colors[ImGuiCol_FrameBg]                = ImVec4(0.20f, 0.20f, 0.20f, 0.54f);
    style.Colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.40f, 0.40f, 0.40f, 0.40f);
    style.Colors[ImGuiCol_FrameBgActive]          = ImVec4(0.28f, 0.28f, 0.28f, 0.67f);
    style.Colors[ImGuiCol_TitleBg]                = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]          = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    style.Colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    style.Colors[ImGuiCol_CheckMark]              = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]             = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    style.Colors[ImGuiCol_Button]                 = ImVec4(0.20f, 0.20f, 0.20f, 0.40f);
    style.Colors[ImGuiCol_ButtonHovered]          = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]           = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    style.Colors[ImGuiCol_Header]                 = ImVec4(0.20f, 0.20f, 0.20f, 0.31f);
    style.Colors[ImGuiCol_HeaderHovered]          = ImVec4(0.30f, 0.30f, 0.30f, 0.80f);
    style.Colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    
    style.WindowRounding = 4.0f;
    style.FrameRounding = 4.0f;
}

void Application::Cleanup() {
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

void Application::Run() {
    ImVec4 clear_color = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
             if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                running = false;
#ifdef __APPLE__
            if (event.type == SDL_USEREVENT) {
                switch (event.user.code) {
                    case MacMenu_Launch: LaunchNativeTerminal(); break;
                    case MacMenu_Relaunch: LaunchNativeTerminal(); break;
                    case MacMenu_Clear: terminal.ClearScrollback(); break;
                    case MacMenu_Reset: terminal.Reset(); shell_ready = false; break;
                    case MacMenu_SendCtrlC: sshClient.send_shell_command("\x03"); break;
                    case MacMenu_SendCtrlZ: sshClient.send_shell_command("\x1A"); break;
                    case MacMenu_SendCtrlD: sshClient.send_shell_command("\x04"); break;
                    case MacMenu_SendCtrlX: sshClient.send_shell_command("\x18"); break;
                    case MacMenu_SendCtrlO: sshClient.send_shell_command("\x0F"); break;
                }
            }
#endif
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        // No Cmd remap: Ctrl remains the modifier for copy/paste/save
        ImGui::NewFrame();

#ifndef __APPLE__
        // Fallback menu bar for non-macOS
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Terminal")) {
                if (!terminal_launched) {
                    if (ImGui::MenuItem("Launch Native Terminal")) {
                        LaunchNativeTerminal();
                    }
                } else {
                    if (ImGui::MenuItem("Relaunch Native Terminal")) {
                        LaunchNativeTerminal();
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Clear In-App Terminal")) {
                    terminal.ClearScrollback();
                }
                if (ImGui::MenuItem("Reset In-App Terminal")) {
                    terminal.Reset();
                    shell_ready = false;
                }
                if (ImGui::MenuItem("Send Ctrl+C to Shell")) {
                    sshClient.send_shell_command("\x03");
                }
                if (ImGui::MenuItem("Send Ctrl+Z to Shell")) {
                    sshClient.send_shell_command("\x1A");
                }
                if (ImGui::MenuItem("Send Ctrl+D to Shell")) {
                    sshClient.send_shell_command("\x04");
                }
                if (ImGui::MenuItem("Send Ctrl+X to Shell")) {
                    sshClient.send_shell_command("\x18");
                }
                if (ImGui::MenuItem("Send Ctrl+O to Shell")) {
                    sshClient.send_shell_command("\x0F");
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
#else
        Mac_CreateOrUpdateTerminalMenu(terminal_launched);
#endif

        // Dockspace
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpaceOverViewport(dockspace_id, ImGui::GetMainViewport());

        static bool first_time = true;
        if (first_time) {
            first_time = false;
            
            ImGui::DockBuilderRemoveNode(dockspace_id); // Clear any existing layout
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

            ImGuiID dock_main_id = dockspace_id;
            ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
            ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);

            ImGui::DockBuilderDockWindow("File Browser", dock_id_left);
            ImGui::DockBuilderDockWindow("Terminal", dock_id_bottom);
            ImGui::DockBuilderDockWindow("System Monitor", dock_id_bottom); // Stacked
            ImGui::DockBuilderDockWindow("Editor", dock_main_id);
            ImGui::DockBuilderFinish(dockspace_id);
        }

        if (state == AppState::LOGIN) {
            RenderLogin();
        } else {
            RenderWorkspace();
        }

        ImGui::Render();
        SDL_RenderSetScale(renderer, ImGui::GetIO().DisplayFramebufferScale.x, ImGui::GetIO().DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }
}

void Application::RenderLogin() {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    // Dynamic title with timestamp to prove update
    std::string title = "Connect to Server (Build: " + std::string(__DATE__) + " " + std::string(__TIME__) + ")";
    ImGui::Begin(title.c_str(), nullptr, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("History");
    static int selected_history = -1;
    if (ImGui::BeginListBox("##history", ImVec2(-1, 100))) {
        for (int i = 0; i < history_hosts.size(); i++) {
            bool is_selected = (selected_history == i);
            std::string label = history_hosts[i].alias + " (" + history_hosts[i].user + "@" + history_hosts[i].hostname + ")";
            if (ImGui::Selectable(label.c_str(), is_selected)) {
                selected_history = i;
                strcpy(host_input, history_hosts[i].hostname.c_str());
                strcpy(user_input, history_hosts[i].user.c_str());
                strcpy(port_input, history_hosts[i].port.c_str());
                std::string saved_pass, saved_key;
                if (CredentialStore::Load(history_hosts[i], saved_pass, saved_key)) {
                    strncpy(pass_input, saved_pass.c_str(), sizeof(pass_input) - 1);
                    pass_input[sizeof(pass_input) - 1] = '\0';
                    strncpy(key_path_input, saved_key.c_str(), sizeof(key_path_input) - 1);
                    key_path_input[sizeof(key_path_input) - 1] = '\0';
                } else {
                    pass_input[0] = '\0';
                }
            }
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndListBox();
    }

    ImGui::Separator();
    ImGui::Text("Known Hosts (.ssh/config)");
    ImGui::Separator();
    
    static int selected_host = -1;
    if (ImGui::BeginListBox("##hosts", ImVec2(-1, 150))) {
        for (int i = 0; i < known_hosts.size(); i++) {
            bool is_selected = (selected_host == i);
            if (ImGui::Selectable(known_hosts[i].alias.c_str(), is_selected)) {
                selected_host = i;
                // Autofill fields
                strcpy(host_input, known_hosts[i].hostname.c_str());
                strcpy(user_input, known_hosts[i].user.c_str());
                strcpy(port_input, known_hosts[i].port.c_str());
                strcpy(key_path_input, known_hosts[i].identity_file.c_str());
                std::string saved_pass, saved_key;
                if (CredentialStore::Load(known_hosts[i], saved_pass, saved_key)) {
                    strncpy(pass_input, saved_pass.c_str(), sizeof(pass_input) - 1);
                    pass_input[sizeof(pass_input) - 1] = '\0';
                    if (!saved_key.empty()) {
                        strncpy(key_path_input, saved_key.c_str(), sizeof(key_path_input) - 1);
                        key_path_input[sizeof(key_path_input) - 1] = '\0';
                    }
                } else {
                    pass_input[0] = '\0';
                }
            }
            if (is_selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndListBox();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Manual Connection");
    ImGui::InputText("Host (IP/Domain)", host_input, IM_ARRAYSIZE(host_input));
    ImGui::InputText("Port", port_input, IM_ARRAYSIZE(port_input));
    ImGui::InputText("User", user_input, IM_ARRAYSIZE(user_input));
    ImGui::InputText("Password (Optional)", pass_input, IM_ARRAYSIZE(pass_input), ImGuiInputTextFlags_Password);
    ImGui::InputText("Key Path (Optional)", key_path_input, IM_ARRAYSIZE(key_path_input));

    ImGui::Spacing();

    if (sshClient.is_busy()) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Connecting...");
    } else {
        if (ImGui::Button("Connect", ImVec2(-1, 40))) {
             sshClient.connect(host_input, atoi(port_input));
             sprintf(status_msg, "Connecting to %s...", host_input);
        }
    }

    ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", sshClient.get_error().c_str());

    // Logic check
    if (sshClient.is_connected() && !sshClient.is_authenticated() && !sshClient.is_busy()) {
        // Connected but need auth
        // If we have password, try it. If not, try publickey auto.
        // This loop is simple: once connected, we try auth immediately.
        // If we are here, it means connect() succeeded.
        
        static bool tried_auth = false;
        // We need a way to know if we just finished connecting.
        // For now, let's just trigger auth once.
        // Ideally, we'd use a state machine variable.
        
        // Hacky "auto-auth" trigger:
        sshClient.authenticate(user_input, pass_input, key_path_input);
    }
    
    if (sshClient.is_authenticated()) {
        state = AppState::CONNECTED;
        files_need_refresh = true;
        sftpClient.init(sshClient.get_session(), &sshClient.get_mutex());
        monitor.Start(host_input, atoi(port_input), user_input, pass_input, key_path_input);
        current_path = ".";
        path_history.clear();
        path_history.push_back(current_path);
        history_index = 0;
        shell_ready = false;
        terminal.Reset();
        
        // Save to history
        SSHHost h;
        h.alias = host_input; // Use hostname as alias for manual entry if not from config
        h.hostname = host_input;
        h.user = user_input;
        h.port = port_input;
        
        CredentialStore::Save(h, pass_input, key_path_input);

        if (!history_path.empty()) {
            SSHConfigParser::save_history(history_path, h);
        }
    }

    ImGui::End();
}

void Application::RenderWorkspace() {
    RenderFileBrowser();
    RenderEditor();
    
    RenderTerminal();
    monitor.Render(); 
}

static std::string PickLocalFile() {
#ifdef __APPLE__
    return Mac_ShowOpenFilePanel();
#else
    return "";
#endif
}

static std::string JoinPath(const std::string& base, const std::string& name) {
    if (base == "." || base.empty()) return name;
    if (base == "/") return "/" + name;
    if (base.back() == '/') return base + name;
    return base + "/" + name;
}

static std::string GetHomePath() {
    const char* home_env = getenv("HOME");
    if (home_env && *home_env) return std::string(home_env);
    struct passwd* pw = getpwuid(getuid());
    if (pw && pw->pw_dir) return std::string(pw->pw_dir);
    return "";
}

void Application::RenderFileBrowser() {
    ImGui::Begin("File Browser");
    if (files_need_refresh) {
        RefreshFileList();
        files_need_refresh = false;
    }
    // Navigation Header
    if (ImGui::Button("<")) {
        if (history_index > 0) {
            history_index--;
            current_path = path_history[history_index];
            files_need_refresh = true;
        } else if (current_path != "/") {
            // push root and navigate
            path_history.insert(path_history.begin(), "/");
            history_index = 0;
            current_path = "/";
            files_need_refresh = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(">")) {
        if (history_index + 1 < (int)path_history.size()) {
            history_index++;
            current_path = path_history[history_index];
            files_need_refresh = true;
        }
    }
    ImGui::SameLine();
    ImGui::Text("Path: %s", current_path.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Upload")) {
        std::string local = PickLocalFile();
        if (!local.empty()) {
            std::ifstream in(local, std::ios::binary);
            if (in) {
                std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                std::string filename = std::filesystem::path(local).filename().string();
                std::string remote_path = JoinPath(current_path, filename);
                sftpClient.write_file(remote_path, content);
                files_need_refresh = true;
            }
        }
    }
    ImGui::Separator();

    if (ImGui::BeginTable("Files", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        std::sort(current_files.begin(), current_files.end(), [](const RemoteFile& a, const RemoteFile& b) {
            if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir; 
            return a.name < b.name;
        });

        for (int i = 0; i < current_files.size(); i++) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (current_files[i].name == "." || current_files[i].name == "..") continue;

            std::string label = (current_files[i].is_dir ? "[D] " : "[F] ") + current_files[i].name;
            bool is_selected = (selected_file_index == i);
            if (ImGui::Selectable(label.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap)) {
                selected_file_index = i;
                if (ImGui::IsMouseDoubleClicked(0)) {
                    if (current_files[i].is_dir) {
                        std::string next = JoinPath(current_path, current_files[i].name);
                        // update history
                        if (history_index + 1 < (int)path_history.size()) {
                            path_history.erase(path_history.begin() + history_index + 1, path_history.end());
                        }
                        path_history.push_back(next);
                        history_index = (int)path_history.size() - 1;
                        current_path = next;
                        files_need_refresh = true;
                    } else {
                        OpenFile(current_files[i].name);
                    }
                }
            }
            if (ImGui::BeginPopupContextItem()) {
                if (!current_files[i].is_dir) {
                    if (ImGui::MenuItem("Open")) {
                        OpenFile(current_files[i].name);
                    }
                    if (ImGui::MenuItem("Download")) {
                        std::string full_path = JoinPath(current_path, current_files[i].name);
                        std::string home = GetHomePath();
                        if (home.empty()) {
                            snprintf(status_msg, sizeof(status_msg), "Download failed: cannot resolve home path");
                        } else {
                            std::filesystem::path downloads_dir = std::filesystem::path(home) / "Downloads";
                            std::error_code ec;
                            std::filesystem::create_directories(downloads_dir, ec);
                            std::filesystem::path dst = downloads_dir / current_files[i].name;
                            bool ok = sftpClient.download_file(full_path, dst.string());
                            if (ok) {
                                snprintf(status_msg, sizeof(status_msg), "Downloaded to %s", dst.string().c_str());
                            } else {
                                snprintf(status_msg, sizeof(status_msg), "Download failed: transfer error");
                            }
                        }
                    }
                    if (ImGui::MenuItem("Delete")) {
                        std::string full_path = JoinPath(current_path, current_files[i].name);
                        sftpClient.delete_path(full_path, false);
                        files_need_refresh = true;
                    }
                } else {
                    if (ImGui::MenuItem("Open Folder")) {
                        std::string next = JoinPath(current_path, current_files[i].name);
                        if (history_index + 1 < (int)path_history.size()) {
                            path_history.erase(path_history.begin() + history_index + 1, path_history.end());
                        }
                        path_history.push_back(next);
                        history_index = (int)path_history.size() - 1;
                        current_path = next;
                        files_need_refresh = true;
                    }
                    if (ImGui::MenuItem("Delete")) {
                        std::string full_path = JoinPath(current_path, current_files[i].name);
                        sftpClient.delete_path(full_path, true);
                        files_need_refresh = true;
                    }
                }
                ImGui::EndPopup();
            }
            ImGui::TableNextColumn();
            if (!current_files[i].is_dir) {
                 if (current_files[i].size < 1024) ImGui::Text("%llu B", current_files[i].size);
                 else if (current_files[i].size < 1024*1024) ImGui::Text("%.1f KB", current_files[i].size/1024.0f);
                 else ImGui::Text("%.1f MB", current_files[i].size/(1024.0f*1024.0f));
            } else {
                ImGui::Text("-");
            }
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

void Application::RenderEditor() {
    ImGui::Begin("Editor");
    
    editorManager.Render([this](const std::string& path, const std::string& content) -> bool {
        return sftpClient.write_file(path, content);
    });
    
    ImGui::End();
}

void Application::RenderTerminal() {
    ImGui::Begin("Terminal");

    if (state != AppState::CONNECTED) {
        ImGui::TextDisabled("Connect to a server to use the in-app terminal.");
        ImGui::End();
        return;
    }

    if (!shell_ready) {
        if (sshClient.init_shell()) {
            shell_ready = true;
        } else {
            ImGui::TextColored(ImVec4(1,0.5f,0.5f,1), "Shell not ready. Check authentication.");
            if (ImGui::Button("Retry Shell Init")) {
                shell_ready = sshClient.init_shell();
            }
            ImGui::End();
            return;
        }
    }

    // Pull remote output
    std::string chunk = sshClient.read_shell_output();
    while (!chunk.empty()) {
        terminal.Feed(chunk);
        chunk = sshClient.read_shell_output();
    }

    terminal.Render();

    // Send user input
    std::string outgoing = terminal.ConsumeOutgoing();
    if (!outgoing.empty()) {
        sshClient.send_shell_command(outgoing);
    }

    ImGui::End();
}

void Application::RefreshFileList() {
    current_files = sftpClient.list_directory(current_path);
}

void Application::OpenFile(const std::string& filename) {
    std::string full_path = JoinPath(current_path, filename);
    std::string content;
    sftpClient.read_file(full_path, content);
    
    editorManager.OpenFile(filename, full_path, content);
}

void Application::SaveFile() {
    // We need to know WHICH file to save. 
    // For now, we will implement the saving logic inside RenderEditor's loop or via a "Save Active" logic.
    // But since SaveFile() is a helper, let's adapt it.
    // Actually, EditorManager should probably handle the "GetActiveFileContent" and return it to us.
    
    // In this new architecture, SaveFile() is called when user presses Cmd+S inside the specific editor tab.
    // EditorManager's Render() will handle the UI.
    // We need a mechanism for EditorManager to request a save.
}

void Application::LaunchNativeTerminal() {
    std::string ssh_cmd = "ssh " + std::string(user_input) + "@" + std::string(host_input) + " -p " + std::string(port_input);
    if (strlen(key_path_input) > 0) {
        ssh_cmd += " -i " + std::string(key_path_input);
    }

    std::string osascript_cmd = "osascript -e 'tell application \"Terminal\" to do script \"" + ssh_cmd + "\"' -e 'tell application \"Terminal\" to activate'";
    system(osascript_cmd.c_str());
    terminal_launched = true;
}
