#pragma once

#include <cstddef>
#include <cstdint>

namespace spud {

enum class mem_protection {
  READ = 1 << 1,
  WRITE = 1 << 2,
  EXECUTE = 1 << 3,

  READ_WRITE = READ | WRITE,
  READ_WRITE_EXECUTE = READ_WRITE | EXECUTE,
};

class protection_scope {
public:
  protection_scope(uintptr_t address, size_t size, mem_protection protect);
  ~protection_scope();

  void detach() {
    address_ = 0;
    size_ = 0;
  }

private:
  uintptr_t address_ = 0;
  size_t size_ = 0;

  uintptr_t extra_ = 0;

  uintptr_t original_protection_;
};
} // namespace spud

#define SPUD_SCOPED_PROTECTION_IMPL(n, addr, size, protection)                 \
  protection_scope n{addr, size, protection};

#define SPUD_SCOPED_PROTECTION(addr, size, protection)                         \
  SPUD_SCOPED_PROTECTION_IMPL(SPUD_PP_CAT(spud_protect, __COUNTER__), addr,    \
                              size, protection)
