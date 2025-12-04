#include "EditorManager.h"
#include "imgui.h"
#include <filesystem>

EditorManager::EditorManager() {}

void EditorManager::ConfigureLanguage(TextEditor* editor, const std::string& filename) {
    // Simple extension check
    std::string ext = filename.substr(filename.find_last_of(".") + 1);
    
    if (ext == "cpp" || ext == "h" || ext == "hpp" || ext == "c") 
        editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    else if (ext == "sql")
        editor->SetLanguageDefinition(TextEditor::LanguageDefinition::SQL());
    else if (ext == "lua")
        editor->SetLanguageDefinition(TextEditor::LanguageDefinition::Lua());
    else if (ext == "py")
        // Basic Python support (TextEditor usually has C++, SQL, Lua, etc. built-in. Python might need custom def or reuse C-style)
        // ImGuiColorTextEdit defaults: C++, SQL, AngelScript, Lua, C, GLSL. 
        // We can map others to C++ for basic colors or implement custom.
        editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus()); // Fallback
    else
        // Plain text?
        ; // No specific definition, keeps default
}

void EditorManager::OpenFile(const std::string& name, const std::string& path, const std::string& content) {
    // Check if already open
    for (size_t i=0; i<tabs.size(); ++i) {
        if (tabs[i].full_path == path) {
            // Focus it (handled by TabItem logic implicitly if we set ID? No, ImGui requires managing SetTabItemClosed)
            // We can't force focus easily without ID tricks, but we can assume user sees it.
            return; 
        }
    }

    EditorTab tab;
    tab.filename = name;
    tab.full_path = path;
    tab.editor = std::make_shared<TextEditor>();
    tab.editor->SetText(content);
    tab.editor->SetPalette(TextEditor::GetDarkPalette());
    
    ConfigureLanguage(tab.editor.get(), name);
    
    tabs.push_back(tab);
}

void EditorManager::Render(std::function<bool(const std::string& path, const std::string& content)> on_save) {
    if (tabs.empty()) {
        ImVec2 window_size = ImGui::GetContentRegionAvail();
        ImVec2 text_size = ImGui::CalcTextSize("No file opened");
        
        ImGui::SetCursorPos(ImVec2((window_size.x - text_size.x) * 0.5f, (window_size.y - text_size.y) * 0.5f));
        ImGui::TextDisabled("No file opened");
        return;
    }

    if (ImGui::BeginTabBar("EditorTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
        for (int i = 0; i < tabs.size(); i++) {
            // Build label
            std::string label = tabs[i].filename;
            // Use CanUndo as dirty check because Save (SetText) resets undo history.
            // This ensures * appears when we have typed something new.
            if (tabs[i].editor->CanUndo()) {
                label += "*";
                tabs[i].is_dirty = true;
            } else {
                tabs[i].is_dirty = false;
            }
            
            // Unique stable ID using full path
            std::string id = label + "###" + tabs[i].full_path;
            bool keep_open = true;
            ImGuiTabItemFlags flags = (tabs[i].is_dirty ? ImGuiTabItemFlags_UnsavedDocument : 0);
            
            if (ImGui::BeginTabItem(id.c_str(), &keep_open, flags)) {
                // Shortcut: Cmd+S to save CURRENT tab
                if (ImGui::GetIO().KeySuper && ImGui::IsKeyPressed(ImGuiKey_S)) {
                     if (on_save(tabs[i].full_path, tabs[i].editor->GetText())) {
                         MarkSaved(i);
                     }
                }
                
                // Use unique stable ID based on path for each editor instance
                std::string editor_id = "TextEditor###" + tabs[i].full_path;
                tabs[i].editor->Render(editor_id.c_str());
                ImGui::EndTabItem();
            }
            
            if (!keep_open) {
                if (tabs[i].is_dirty) {
                    show_save_modal = true;
                    tab_to_close = i;
                } else {
                    tabs.erase(tabs.begin() + i);
                    i--;
                }
            }
        }
        ImGui::EndTabBar();
    }
    
    if (show_save_modal) {
        ImGui::OpenPopup("Save Changes?");
    }

    if (ImGui::BeginPopupModal("Save Changes?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Do you want to save changes to %s?", tabs[tab_to_close].filename.c_str());
        ImGui::Separator();

        if (ImGui::Button("Yes", ImVec2(120, 0))) {
            if (on_save(tabs[tab_to_close].full_path, tabs[tab_to_close].editor->GetText())) {
                MarkSaved(tab_to_close); // Though we close it immediately
            }
            tabs.erase(tabs.begin() + tab_to_close);
            tab_to_close = -1;
            show_save_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        
        if (ImGui::Button("No", ImVec2(120, 0))) {
            tabs.erase(tabs.begin() + tab_to_close);
            tab_to_close = -1;
            show_save_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            tab_to_close = -1;
            show_save_modal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

bool EditorManager::CheckSaveRequest(std::string& out_path, std::string& out_content) {
    // Global shortcut Cmd+S for ACTIVE tab
    // We need to know which tab is active. ImGui doesn't easily tell us outside the loop.
    // However, TextEditor handles its own input. 
    
    // Let's check all tabs. If one is focused... 
    // ImGuiColorTextEdit has IsFocused()?
    // We can just rely on the Application to call this when Cmd+S is pressed globally.
    // But which file?
    
    // Better: In Render loop, if a tab is active, store its index.
    // But we can't access it easily.
    
    // Fallback: Just check if the save modal "Yes" was clicked?
    // No, let's fix the Save Modal logic above first.
    // Since I can't easily change the class structure during 'write_file' with callbacks...
    // I will defer the Save action to the Application loop using a polling getter.
    
    return false;
}

std::string EditorManager::GetContent(int index) {
    if (index >= 0 && index < tabs.size()) return tabs[index].editor->GetText();
    return "";
}

void EditorManager::MarkSaved(int index) {
     if (index >= 0 && index < tabs.size()) {
         // Preserve cursor
         auto cursor = tabs[index].editor->GetCursorPosition();
         auto text = tabs[index].editor->GetText();
         tabs[index].editor->SetText(text);
         tabs[index].editor->SetCursorPosition(cursor);
         tabs[index].is_dirty = false;
     }
}
