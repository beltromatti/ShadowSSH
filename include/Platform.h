#pragma once

#include <string>

// Cross-platform helpers. Implementations live under src/platform/.
namespace Platform {

// Return the user's home directory. Empty on failure.
std::string GetHomeDir();

// Return a writable directory for app config/data (created if missing).
// Mac: ~/Library/Application Support/ShadowSSH
// Linux: $XDG_CONFIG_HOME/shadowssh or ~/.config/shadowssh
// Win: %APPDATA%\\ShadowSSH
std::string GetConfigDir();

// Return a writable temp directory.
std::string GetTempDir();

// Show a native "open file" dialog. Returns absolute path or "" on cancel.
std::string OpenFileDialog();

// Show a native "save file" dialog with a suggested name and starting directory.
// Returns the chosen absolute path or "" on cancel.
std::string SaveFileDialog(const std::string& suggested_name, const std::string& starting_dir);

// Launch the system's native terminal application and run `ssh <args>`.
// Returns true if the launch command was dispatched.
bool LaunchNativeSshTerminal(const std::string& user, const std::string& host,
                             const std::string& port, const std::string& key_path);

// Copy `path` to the system clipboard, when needed for legacy paths.
// (Not currently used by the UI - included for completeness.)
void OpenInFileManager(const std::string& path);

} // namespace Platform
