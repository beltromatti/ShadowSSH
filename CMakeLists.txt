cmake_minimum_required(VERSION 3.16)

# Project version is injected at build time via -DSHADOWSSH_VERSION=X.Y.Z.
# Default 0.0.0 keeps local builds runnable without flags.
if(NOT DEFINED SHADOWSSH_VERSION)
    set(SHADOWSSH_VERSION "0.0.0")
endif()

project(ShadowSSH
    VERSION ${SHADOWSSH_VERSION}
    LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
# Dependency discovery
# -----------------------------------------------------------------------------
include(FindPkgConfig OPTIONAL)

# libssh
find_package(libssh CONFIG QUIET)
if(NOT libssh_FOUND)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBSSH_PC QUIET libssh)
    endif()
    find_path(LIBSSH_INCLUDE_DIR libssh/libssh.h
        HINTS ${LIBSSH_PC_INCLUDE_DIRS}
        PATHS /opt/homebrew/include /usr/local/include /usr/include)
    find_library(LIBSSH_LIBRARY NAMES ssh libssh ssh.dll
        HINTS ${LIBSSH_PC_LIBRARY_DIRS}
        PATHS /opt/homebrew/lib /usr/local/lib /usr/lib)
endif()

if(TARGET ssh)
    set(LIBSSH_TARGETS ssh)
elseif(TARGET libssh::ssh)
    set(LIBSSH_TARGETS libssh::ssh)
elseif(TARGET libssh::libssh)
    set(LIBSSH_TARGETS libssh::libssh)
elseif(LIBSSH_LIBRARY)
    set(LIBSSH_TARGETS ${LIBSSH_LIBRARY})
else()
    message(FATAL_ERROR "libssh not found. Install it (brew/apt/vcpkg) and re-run cmake.")
endif()

# SDL2
find_package(SDL2 CONFIG QUIET)
if(NOT SDL2_FOUND)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2_PC QUIET sdl2)
    endif()
    find_path(SDL2_INCLUDE_DIR SDL.h
        HINTS ${SDL2_PC_INCLUDE_DIRS}
        PATH_SUFFIXES SDL2
        PATHS /opt/homebrew/include /usr/local/include /usr/include)
    find_library(SDL2_LIBRARY NAMES SDL2 SDL2-2.0
        HINTS ${SDL2_PC_LIBRARY_DIRS}
        PATHS /opt/homebrew/lib /usr/local/lib /usr/lib)
    set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
    set(SDL2_LIBRARIES ${SDL2_LIBRARY})
endif()
if(TARGET SDL2::SDL2)
    set(SDL2_TARGETS SDL2::SDL2)
elseif(SDL2_LIBRARIES)
    set(SDL2_TARGETS ${SDL2_LIBRARIES})
else()
    message(FATAL_ERROR "SDL2 not found.")
endif()

# Freetype (kept for parity with original cmake; not directly used by the code)
find_package(Freetype QUIET)
if(NOT FREETYPE_FOUND AND PKG_CONFIG_FOUND)
    pkg_check_modules(FREETYPE_PC QUIET freetype2)
    set(FREETYPE_INCLUDE_DIRS ${FREETYPE_PC_INCLUDE_DIRS})
    set(FREETYPE_LIBRARIES ${FREETYPE_PC_LIBRARIES})
endif()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/vendor/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp
    ${CMAKE_SOURCE_DIR}/vendor/ImGuiColorTextEdit/TextEditor.cpp)

set(SHADOWSSH_SOURCES
    src/main.cpp
    src/Application.cpp
    src/EditorManager.cpp
    src/SFTPClient.cpp
    src/SSHClient.cpp
    src/SSHConfigParser.cpp
    src/SystemMonitor.cpp
    src/terminal/Terminal.cpp
    src/platform/Platform_common.cpp)

if(APPLE)
    list(APPEND SHADOWSSH_SOURCES
        src/platform/Platform_mac.mm
        src/platform/CredentialStore_mac.mm)
elseif(WIN32)
    list(APPEND SHADOWSSH_SOURCES
        src/platform/Platform_win.cpp
        src/platform/CredentialStore_win.cpp)
else()
    list(APPEND SHADOWSSH_SOURCES
        src/platform/Platform_linux.cpp
        src/platform/CredentialStore_linux.cpp)
endif()

file(GLOB LIBVTERM_SOURCES
    "src/terminal/libvterm/src/*.c"
    "src/terminal/libvterm/src/encoding/*.c")
list(APPEND SHADOWSSH_SOURCES ${LIBVTERM_SOURCES})

# -----------------------------------------------------------------------------
# Executable
# -----------------------------------------------------------------------------
if(WIN32)
    add_executable(ShadowSSH WIN32 ${SHADOWSSH_SOURCES} ${IMGUI_SOURCES})
else()
    add_executable(ShadowSSH ${SHADOWSSH_SOURCES} ${IMGUI_SOURCES})
endif()

target_include_directories(ShadowSSH PRIVATE
    include
    vendor/imgui
    vendor/imgui/backends
    vendor/ImGuiColorTextEdit
    src/terminal
    src/terminal/libvterm/include)

if(LIBSSH_INCLUDE_DIR)
    target_include_directories(ShadowSSH PRIVATE ${LIBSSH_INCLUDE_DIR})
endif()
if(SDL2_INCLUDE_DIRS)
    target_include_directories(ShadowSSH PRIVATE ${SDL2_INCLUDE_DIRS})
endif()
if(FREETYPE_INCLUDE_DIRS)
    target_include_directories(ShadowSSH PRIVATE ${FREETYPE_INCLUDE_DIRS})
endif()

target_compile_definitions(ShadowSSH PRIVATE
    SHADOWSSH_VERSION_STRING="${SHADOWSSH_VERSION}")

target_link_libraries(ShadowSSH PRIVATE ${LIBSSH_TARGETS} ${SDL2_TARGETS})
if(FREETYPE_LIBRARIES)
    target_link_libraries(ShadowSSH PRIVATE ${FREETYPE_LIBRARIES})
endif()

if(APPLE)
    target_link_libraries(ShadowSSH PRIVATE
        "-framework Security"
        "-framework CoreFoundation"
        "-framework Cocoa")
    set_source_files_properties(
        src/platform/Platform_mac.mm
        src/platform/CredentialStore_mac.mm
        PROPERTIES COMPILE_FLAGS "-ObjC++ -fobjc-arc")

    set_target_properties(ShadowSSH PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "ShadowSSH"
        MACOSX_BUNDLE_BUNDLE_VERSION "${SHADOWSSH_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${SHADOWSSH_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.beltromatti.shadowssh"
        MACOSX_BUNDLE_ICON_FILE "ShadowSSH"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist.in")

    if(EXISTS "${CMAKE_SOURCE_DIR}/assets/icon/ShadowSSH.icns")
        set_source_files_properties(
            "${CMAKE_SOURCE_DIR}/assets/icon/ShadowSSH.icns"
            PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(ShadowSSH PRIVATE "${CMAKE_SOURCE_DIR}/assets/icon/ShadowSSH.icns")
    endif()
elseif(WIN32)
    # SDL2's WIN32 subsystem mode requires SDL2main as the entry point.
    if(TARGET SDL2::SDL2main)
        target_link_libraries(ShadowSSH PRIVATE SDL2::SDL2main)
    endif()
    target_link_libraries(ShadowSSH PRIVATE
        ws2_32 crypt32 secur32 advapi32 user32 gdi32 shell32 comdlg32 ole32)
    if(MSVC)
        target_compile_options(ShadowSSH PRIVATE /W3 /utf-8)
    endif()
else()
    find_package(Threads REQUIRED)
    target_link_libraries(ShadowSSH PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
endif()

if(NOT MSVC)
    target_compile_options(ShadowSSH PRIVATE -Wno-unused-parameter -Wno-unused-variable)
endif()

message(STATUS "ShadowSSH version: ${SHADOWSSH_VERSION}")
message(STATUS "libssh: ${LIBSSH_TARGETS}")
message(STATUS "SDL2:   ${SDL2_TARGETS}")
