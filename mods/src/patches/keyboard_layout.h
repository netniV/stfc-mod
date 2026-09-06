#pragma once

#include <prime/KeyCode.h>
#include <string_view>
#include <toml++/toml.h>

namespace keyboard_layout
{
// Config-time calls: no Unity access. Preserve configured text and its provenance.
void Configure(std::string_view mode);
void RegisterShortcut(std::string_view name, std::string_view chord, KeyCode key);
void InitializeDiagnostics(toml::table& vars);

// Game-thread only, at the MapKey action boundary. Physical mode does no Unity
// layout work. Layout mode checks once per input frame and rebuilds on change.
KeyCode Resolve(KeyCode configured);
} // namespace keyboard_layout
