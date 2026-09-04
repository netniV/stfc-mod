#pragma once

// ScreenManager.Update has one detour owner shared by HotkeyHooks and opt-in frame services.
// Installation is idempotent so either owner can request the dispatcher.
void install_screen_manager_update_hook();

using ScreenManagerUpdateCallback = void (*)();

// Registers a process-lifetime callback that runs before the game's ScreenManager.Update handling.
// Register during patch installation. Re-registering the same callback is harmless.
bool register_screen_manager_update_callback(ScreenManagerUpdateCallback callback);
void dispatch_screen_manager_update_callbacks();
