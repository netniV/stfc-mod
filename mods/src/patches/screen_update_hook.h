#pragma once

// ScreenManager.Update has one detour owner shared by HotkeyHooks and FleetWatchHooks.
// Installation is idempotent so either patch can request the dispatcher.
void install_screen_manager_update_hook();
