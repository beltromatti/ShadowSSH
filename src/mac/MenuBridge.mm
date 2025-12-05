#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#include <SDL.h>
#include "MacMenu.h"

@interface MacMenuHandler : NSObject
@property (nonatomic, assign) BOOL launched;
@end

@implementation MacMenuHandler
- (void)emit:(MacMenuCommand)cmd {
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_USEREVENT;
    e.user.code = cmd;
    SDL_PushEvent(&e);
}

- (void)launch:(id)sender { [self emit: MacMenu_Launch]; }
- (void)relaunch:(id)sender { [self emit: MacMenu_Relaunch]; }
- (void)clear:(id)sender { [self emit: MacMenu_Clear]; }
- (void)reset:(id)sender { [self emit: MacMenu_Reset]; }
- (void)sendCtrlC:(id)sender { [self emit: MacMenu_SendCtrlC]; }
@end

static NSMenuItem* gTerminalMenuItem = nil;
static MacMenuHandler* gHandler = nil;
static NSMenuItem* gLaunchItem = nil;
static NSMenuItem* gRelaunchItem = nil;

void Mac_CreateOrUpdateTerminalMenu(bool terminal_launched) {
    if (!gHandler) {
        gHandler = [MacMenuHandler new];
    }
    gHandler.launched = terminal_launched;

    NSMenu* mainMenu = [NSApp mainMenu];
    if (!mainMenu) {
        mainMenu = [[NSMenu alloc] initWithTitle:@""];
        [NSApp setMainMenu:mainMenu];
    }

    if (!gTerminalMenuItem) {
        NSMenu* termMenu = [[NSMenu alloc] initWithTitle:@"Terminal"];
        gLaunchItem = [[NSMenuItem alloc] initWithTitle:@"Launch Native Terminal" action:@selector(launch:) keyEquivalent:@""];
        [gLaunchItem setTarget:gHandler];
        gRelaunchItem = [[NSMenuItem alloc] initWithTitle:@"Relaunch Native Terminal" action:@selector(relaunch:) keyEquivalent:@""];
        [gRelaunchItem setTarget:gHandler];
        NSMenuItem* clearItem = [[NSMenuItem alloc] initWithTitle:@"Clear In-App Terminal" action:@selector(clear:) keyEquivalent:@""];
        [clearItem setTarget:gHandler];
        NSMenuItem* resetItem = [[NSMenuItem alloc] initWithTitle:@"Reset In-App Terminal" action:@selector(reset:) keyEquivalent:@""];
        [resetItem setTarget:gHandler];
        NSMenuItem* ctrlCItem = [[NSMenuItem alloc] initWithTitle:@"Send Ctrl+C" action:@selector(sendCtrlC:) keyEquivalent:@""];
        [ctrlCItem setTarget:gHandler];

        [termMenu addItem:gLaunchItem];
        [termMenu addItem:gRelaunchItem];
        [termMenu addItem:[NSMenuItem separatorItem]];
        [termMenu addItem:clearItem];
        [termMenu addItem:resetItem];
        [termMenu addItem:ctrlCItem];

        gTerminalMenuItem = [[NSMenuItem alloc] initWithTitle:@"Terminal" action:nil keyEquivalent:@""];
        [gTerminalMenuItem setSubmenu:termMenu];
        [mainMenu addItem:gTerminalMenuItem];
    }

    [gLaunchItem setHidden:terminal_launched];
    [gRelaunchItem setHidden:!terminal_launched];
}

#endif
