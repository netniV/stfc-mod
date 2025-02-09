#pragma once

#include <cstddef>
#include <cstdint>

namespace spud::detail {
struct remapper {
  remapper(uintptr_t address, size_t size);
  ~remapper();

  operator uintptr_t() {
    return remap_;
  }

private:
  uintptr_t address_;
  uintptr_t remap_;
  size_t size_;
};
} // namespace spud::detail
