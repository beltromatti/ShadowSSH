#ifdef __APPLE__

#import <Cocoa/Cocoa.h>
#include "Platform.h"
#include <string>
#include <cstdlib>

namespace Platform {

std::string OpenFileDialog() {
    @autoreleasepool {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setAllowsMultipleSelection:NO];
        [panel setCanChooseDirectories:NO];
        [panel setCanChooseFiles:YES];
        NSInteger result = [panel runModal];
        if (result == NSModalResponseOK) {
            if (NSURL* url = [[panel URLs] firstObject]) {
                return std::string([[url path] UTF8String]);
            }
        }
    }
    return "";
}

std::string SaveFileDialog(const std::string& suggested_name, const std::string& starting_dir) {
    @autoreleasepool {
        NSSavePanel* panel = [NSSavePanel savePanel];
        if (!starting_dir.empty()) {
            NSString* dir = [NSString stringWithUTF8String:starting_dir.c_str()];
            [panel setDirectoryURL:[NSURL fileURLWithPath:dir]];
        }
        if (!suggested_name.empty()) {
            NSString* name = [NSString stringWithUTF8String:suggested_name.c_str()];
            [panel setNameFieldStringValue:name];
        }
        NSInteger result = [panel runModal];
        if (result == NSModalResponseOK) {
            if (NSURL* url = [panel URL]) {
                return std::string([[url path] UTF8String]);
            }
        }
    }
    return "";
}

bool LaunchNativeSshTerminal(const std::string& user, const std::string& host,
                             const std::string& port, const std::string& key_path) {
    std::string ssh = "ssh " + user + "@" + host;
    if (!port.empty() && port != "22") ssh += " -p " + port;
    if (!key_path.empty()) ssh += " -i " + key_path;

    std::string script =
        "osascript -e 'tell application \"Terminal\" to do script \"" + ssh + "\"' "
        "-e 'tell application \"Terminal\" to activate'";
    return std::system(script.c_str()) == 0;
}

} // namespace Platform

#endif
