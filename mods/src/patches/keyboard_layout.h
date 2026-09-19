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
// Editor-only queries, without held-key suppression. Capture is converted back
// to the configured character identity; layout ambiguity is rejected.
ResolvedChord DescribeChord(KeyCode configured);
KeyCode       CaptureIdentity(KeyCode physical, bool shift);
} // namespace keyboard_layout
