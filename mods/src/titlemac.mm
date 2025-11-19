#pragma once

#ifdef _WIN32
#elif defined(__APPLE__)

#include "windowtitle.h"
#import <Cocoa/Cocoa.h>

inline std::wstring WindowTitle::Get()
{
    @autoreleasepool {
        NSWindow *window = [NSApp mainWindow];
        if (!window) window = [NSApp keyWindow];
        if (!window) return L"";

        NSString *title = [window title];
        if (!title) return L"";

        std::wstring wtitle([title length], L'\0');
        [title getCharacters:(unichar*)wtitle.data() range:NSMakeRange(0, [title length])];
        return wtitle;
    }
}

inline bool WindowTitle::Set(const std::wstring& title)
{
    @autoreleasepool {
        NSWindow *window = [NSApp mainWindow];
        if (!window) window = [NSApp keyWindow];
        if (!window) return false;

        NSString *nsTitle = [NSString stringWithCharacters:(const unichar*)title.data() length:title.size()];
        [window setTitle:nsTitle];
        
        return true;
    }
} 

#endif
