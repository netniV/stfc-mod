#pragma once

#include <filesystem>
#include <string_view>
#include <toml++/toml.h>

// Synchronous whole-document output for startup, not a runtime setting editor.
// Throws on failure; the caller owns reporting. Does not guarantee power-loss durability.
void SaveConfigDocument(const toml::table& config, const std::filesystem::path& path, std::string_view header = {});
