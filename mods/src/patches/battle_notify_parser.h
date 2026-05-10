#pragma once

#include <string>

struct Toast;

// Attempt to build a detailed notification body from a battle toast's Data.
// Returns empty string if the toast has no battle data or parsing fails.
std::string battle_notify_parse(Toast* toast);
