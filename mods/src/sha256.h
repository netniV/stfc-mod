#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace sha256
{
using digest_t = std::array<uint8_t, 32>;

[[nodiscard]] digest_t digest(std::string_view data);
[[nodiscard]] std::string hex(const digest_t& digest);
[[nodiscard]] std::string hex(std::string_view data);
} // namespace sha256
