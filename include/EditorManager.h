#pragma once
#include "TextEditor.h"
#include <string>
#include <vector>
#include <memory>

struct EditorTab {
    std::string filename;
    std::string full_path;
    std::shared_ptr<TextEditor> editor;
    bool is_dirty = false;
    bool open = true;
    
    // For tracking close request
    bool want_close = false;
};

class EditorManager {
public:
    EditorManager();
    
    void OpenFile(const std::string& name, const std::string& path, const std::string& content);
    
    // Function pointer or std::function for saving: returns true on success
    void Render(std::function<bool(const std::string& path, const std::string& content)> on_save);
    
    // Helper to get content for saving
    std::string GetContent(int index);
    std::string GetPath(int index) { return tabs[index].full_path; }
    void MarkSaved(int index);
    
    bool HasOpenFiles() { return !tabs.empty(); }
    
    // Callback for save action
    // We return true if save was requested for the active tab
    bool CheckSaveRequest(std::string& out_path, std::string& out_content);

private:
    std::vector<EditorTab> tabs;
    
    void ConfigureLanguage(TextEditor* editor, const std::string& filename);
    void HandleClose(int index);
    
    // State for Save Modal
    bool show_save_modal = false;
    int tab_to_close = -1;
};
