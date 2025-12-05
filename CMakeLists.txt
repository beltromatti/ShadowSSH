cmake_minimum_required(VERSION 3.14)
project(ShadowSSH LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Packages
find_package(libssh REQUIRED)
find_package(SDL2 REQUIRED)
find_package(Freetype REQUIRED)

# Fallback if variables are empty (common with Homebrew's cmake config for libssh)
if (NOT LIBSSH_LIBRARIES)
    if (TARGET ssh)
        set(LIBSSH_LIBRARIES ssh)
    elseif (TARGET libssh::ssh)
        set(LIBSSH_LIBRARIES libssh::ssh)
    elseif (TARGET libssh::libssh)
        set(LIBSSH_LIBRARIES libssh::libssh)
    else()
        find_library(LIBSSH_LIBRARY NAMES ssh libssh PATHS /opt/homebrew/lib /usr/local/lib)
        set(LIBSSH_LIBRARIES ${LIBSSH_LIBRARY})
    endif()
endif()

if (NOT LIBSSH_INCLUDE_DIR)
     find_path(LIBSSH_INCLUDE_DIR libssh/libssh.h PATHS /opt/homebrew/include /usr/local/include)
endif()

message(STATUS "LibSSH Include: ${LIBSSH_INCLUDE_DIR}")
message(STATUS "LibSSH Libs: ${LIBSSH_LIBRARIES}")

# Include Directories
include_directories(${LIBSSH_INCLUDE_DIR})

include_directories(${SDL2_INCLUDE_DIRS})
include_directories(${FREETYPE_INCLUDE_DIRS})
include_directories(vendor/imgui)
include_directories(vendor/imgui/backends)
include_directories(include)

# ImGui Sources
set(IMGUI_DIR "vendor/imgui")
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer2.cpp
    vendor/ImGuiColorTextEdit/TextEditor.cpp
)

# Project Sources
file(GLOB_RECURSE PROJECT_SOURCES "src/*.cpp")
file(GLOB LIBVTERM_SOURCES
    "src/terminal/libvterm/src/*.c"
    "src/terminal/libvterm/src/encoding/*.c"
)
message(STATUS "Libvterm sources: ${LIBVTERM_SOURCES}")
list(APPEND PROJECT_SOURCES ${LIBVTERM_SOURCES})
if(APPLE)
    list(APPEND PROJECT_SOURCES src/mac/MenuBridge.mm)
endif()

# Executable
add_executable(ShadowSSH ${PROJECT_SOURCES} ${IMGUI_SOURCES})

# Link Libraries
target_link_libraries(ShadowSSH PRIVATE ${LIBSSH_LIBRARIES} ${SDL2_LIBRARIES} ${FREETYPE_LIBRARIES})
if(APPLE)
    target_link_libraries(ShadowSSH PRIVATE "-framework Security" "-framework CoreFoundation" "-framework Cocoa")
    set_source_files_properties(src/mac/MenuBridge.mm PROPERTIES COMPILE_FLAGS "-ObjC++")
endif()

# Include Directories
include_directories(${LIBSSH_INCLUDE_DIR})
include_directories(${SDL2_INCLUDE_DIRS})
include_directories(${FREETYPE_INCLUDE_DIRS})
include_directories(vendor/imgui)
include_directories(vendor/imgui/backends)
include_directories(vendor/ImGuiColorTextEdit)
include_directories(src/terminal/libvterm/include)
include_directories(src/terminal)
include_directories(include)
