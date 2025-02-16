#include "remapper.h"

#include <spud/arch.h>

#if SPUD_OS_APPLE
#include <mach/mach_init.h>
#include <mach/mach_vm.h>
#endif

#include <cassert>

namespace spud::detail {
remapper::remapper(uintptr_t address, size_t size)
    : address_(address), size_(size) {
#if SPUD_OS_APPLE
  mach_vm_address_t remap = 0;
  vm_prot_t cur, max;
  kern_return_t ret = mach_vm_remap(
      mach_task_self(), &remap, size, 0,
      VM_FLAGS_ANYWHERE | VM_FLAGS_RETURN_DATA_ADDR, mach_task_self(), address,
      FALSE, &cur, &max, VM_INHERIT_NONE);
  remap_ = remap;
#else
  remap_ = address;
#endif
}

remapper::~remapper() {
#if SPUD_OS_APPLE
  mach_vm_address_t addr = address_;
  vm_prot_t cur, max;
  auto ret = mach_vm_remap(mach_task_self(), &addr, size_, 0,
                           VM_FLAGS_OVERWRITE | VM_FLAGS_RETURN_DATA_ADDR,
                           mach_task_self(), remap_, FALSE, &cur, &max,
                           VM_INHERIT_NONE);
  assert(ret == KERN_SUCCESS);
#endif
}
} // namespace spud::detail
