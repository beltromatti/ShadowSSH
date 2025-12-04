# ShadowSSH

A minimal, modern, dark-themed SSH Client for macOS.

## Features
- **Dark UI**: Sleek ImGui-based interface.
- **SFTP Browser**: Navigate remote files, double-click to edit.
- **Editor**: Integrated text editor with Save (Cmd+S) uploading directly to server.
- **Terminal**: Execute shell commands on the server.
- **Config & History**: Auto-loads `~/.ssh/config` and remembers recent connections.

## Installation
The app is built as `ShadowSSH.app`. You can drag it to your Applications folder.

## Development
### Requirements
- CMake
- libssh
- SDL2
- Freetype

```bash
brew install cmake libssh sdl2 freetype
```

### Build
```bash
mkdir build
cd build
cmake ..
make
```

### Deploy
Run the included script to build and package:
```bash
./build_and_deploy.sh
```
