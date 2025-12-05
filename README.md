# ShadowSSH

ShadowSSH is a modern, macOS-focused SSH client built with SDL2 + ImGui. It combines a dark, dockable UI with an in-app terminal powered by libvterm, SFTP browsing/editing, and live system monitoring.

## Features
- **Native Terminal Experience**: libvterm-based VT emulation with colors, cursor, selection, copy/paste (Cmd+C/V), control shortcuts (^C/^Z/^D), bracketed paste, scrollback, and macOS menu entries for launch/relaunch/clear/reset.
- **SFTP Browser**: Navigate remote directories, double-click to open files, upload on save, and track history.
- **Integrated Editor**: Tabbed ImGuiColorTextEdit with syntax highlighting for common extensions, dirty-state handling, Cmd+S to save/upload.
- **System Monitor**: Live load/CPU, memory, disk, and network stats via a dedicated SSH connection.
- **SSH Config & History**: Auto-load `~/.ssh/config`, remember recent connections (stored at `~/.shadowssh_history_v2`), and pull credentials from the macOS Keychain.
- **Native Terminal.app Launcher**: One-click launch (or relaunch) of a native Terminal session with your current host settings.

## Requirements
- macOS (tested on Apple Silicon)
- CMake
- libssh
- SDL2
- Freetype

```bash
brew install cmake libssh sdl2 freetype
```

## Build
```bash
mkdir build
cd build
cmake ..
make
```

## Run
- App bundle: `./build_and_deploy.sh` creates `ShadowSSH.app` in the project root.
- Binary: `build/ShadowSSH`

## Key Shortcuts
- Terminal: Cmd+C/Cmd+V to copy/paste selection; Ctrl+C/Z/D send control codes; selection via mouse drag; right-click for context copy/paste.
- Editor: Cmd+S to save/upload active tab; Cmd+C/V obey macOS behaviors.

## Notes
- Known host keys are enforced; ensure your hosts are in `~/.ssh/known_hosts`.
- Credentials are stored securely in the macOS Keychain when available.
- System monitor uses `/proc` data (best on Linux targets).
