#pragma once
#include "keyboard_layout_mapping.h"
#include <string_view>
#include <toml++/toml.h>

namespace keyboard_layout
{
// Config-time calls do not access Unity or change configured shortcut text.
void Configure(std::string_view mode);
void RegisterShortcut(KeyCode key);
void InitializeDiagnostics(toml::table& vars);
// Game thread only; refresh on device notifications, never by polling.
ResolvedChord ResolveChord(KeyCode configured);
} // namespace keyboard_layout
