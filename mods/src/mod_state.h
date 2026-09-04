#pragma once

#include <nlohmann/json.hpp>

#include <functional>
#include <optional>

namespace mod_state
{
// Reads the latest complete state snapshot. Missing, malformed, and non-object files return no value.
std::optional<nlohmann::json> Read();

// Performs a serialized read-modify-write transaction, retrying briefly when another writer holds the state lock.
bool                          Update(const std::function<void(nlohmann::json&)>& update);

// Performs the same transaction only when the state lock is immediately available.
bool                          TryUpdate(const std::function<void(nlohmann::json&)>& update);
} // namespace mod_state
