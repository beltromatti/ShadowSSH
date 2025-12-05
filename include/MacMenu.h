#pragma once

#ifdef __APPLE__
#include <string>
// SDL user event codes emitted by the native macOS menu.
enum MacMenuCommand {
    MacMenu_Launch = 1,
    MacMenu_Relaunch = 2,
    MacMenu_Clear = 3,
    MacMenu_Reset = 4,
    MacMenu_SendCtrlC = 5,
    MacMenu_SendCtrlZ = 6,
    MacMenu_SendCtrlD = 7,
    MacMenu_SendCtrlX = 8,
    MacMenu_SendCtrlO = 9
};

// Create or update the Terminal menu in the macOS system menu bar.
// Pass whether the terminal session is already launched to toggle the label.
void Mac_CreateOrUpdateTerminalMenu(bool terminal_launched);
std::string Mac_ShowOpenFilePanel();
std::string Mac_ShowSaveFilePanel(const std::string& suggested_name, const std::string& directory);
#endif
