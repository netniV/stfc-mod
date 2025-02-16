#pragma once

#include <spud/arch.h>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace spud {

enum cpu_feature : uint32_t {
  FEATURE_NONE = 0,
#if SPUD_ARCH_X86_FAMILY
  FEATURE_SSE42 = 1 << 1,
  FEATURE_AVX2 = 1 << 2,

  FEATURE_ALL = FEATURE_SSE42 | FEATURE_AVX2
#elif SPUD_ARCH_ARM_FAMILY
  FEATURE_NEON = 1 << 1,
  FEATURE_ALL = FEATURE_NEON
#endif
};

namespace detail {

struct signature_result {
  std::span<uint8_t> buffer;
  uintptr_t offset;
};

void generate_mask_and_data(std::string_view signature, std::string &mask,
                            std::string &data);

inline bool signature_does_match(uintptr_t offset, std::string_view mask,
                                 std::string_view data,
                                 std::span<const uint8_t> search_buffer) {
  for (size_t i = 0; i < mask.size(); ++i) {
    if (mask[i] == '?')
      continue;

    // Make sure we don't walk past the end of the search buffer
    if (search_buffer.size() < offset + i)
      return false;

    if (search_buffer[offset + i] != static_cast<uint8_t>(data[i]))
      return false;
  }
  return true;
}

std::vector<signature_result>
find_matches(std::string_view mask, std::string_view data,
             std::span<uint8_t> search_buffer,
             uint32_t features = cpu_feature::FEATURE_ALL);
} // namespace detail

struct signature_matches {
  struct match {
    match(detail::signature_result result, size_t offset = 0)
        : result(result), adjustment(offset) {}

    uintptr_t address() const {
      return reinterpret_cast<uintptr_t>(result.buffer.data()) + offset();
    }

    uintptr_t call_target() const {
      const auto data = result.buffer.data() + offset();
      return reinterpret_cast<uintptr_t>(data) +
             *reinterpret_cast<int32_t *>(data + 1u) + 5ul;
    }

    match adjust(size_t offset) {
      return {result, adjustment + offset};
    }

  private:
    uintptr_t offset() const {
      return result.offset + adjustment;
    }

    detail::signature_result result;
    uintptr_t adjustment = 0;
  };

  match get(size_t index) {
    return result[index];
  }

  size_t size() const {
    return result.size();
  }

  signature_matches(std::vector<detail::signature_result> result = {})
      : result(result) {}

private:
  std::vector<detail::signature_result> result;
};

signature_matches find_matches(std::string_view signature,
                               std::span<uint8_t> search_buffer,
                               uint32_t features = cpu_feature::FEATURE_ALL);

signature_matches find_in_module(std::string_view signature,
                                 std::string_view module = {},
                                 uint32_t features = cpu_feature::FEATURE_ALL);

} // namespace spud
