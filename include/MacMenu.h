#pragma once

#ifdef __APPLE__
// SDL user event codes emitted by the native macOS menu.
enum MacMenuCommand {
    MacMenu_Launch = 1,
    MacMenu_Relaunch = 2,
    MacMenu_Clear = 3,
    MacMenu_Reset = 4,
    MacMenu_SendCtrlC = 5
};

// Create or update the Terminal menu in the macOS system menu bar.
// Pass whether the terminal session is already launched to toggle the label.
void Mac_CreateOrUpdateTerminalMenu(bool terminal_launched);
#endif
