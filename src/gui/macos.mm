// SPDX-License-Identifier: GPL-2.0-or-later

#include "macos.h"

#import <AppKit/AppKit.h>

namespace macos {

void hideFromDock()
{
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}

} // namespace macos
