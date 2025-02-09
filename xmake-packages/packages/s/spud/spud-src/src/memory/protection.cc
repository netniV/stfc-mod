#include <spud/arch.h>
#include <spud/memory/protection.h>

#if SPUD_OS_WIN
#include <Windows.h>
#elif SPUD_OS_APPLE
#include <mach/mach_init.h>
#include <mach/mach_vm.h>
#include <mach/vm_map.h>
#elif SPUD_OS_LINUX
#include <sys/mman.h>
#include <unistd.h>
#else
#error "Unsupported platform"
#endif

#include <cassert>

namespace spud {

protection_scope::protection_scope(uintptr_t address, size_t size,
                                   mem_protection protect)
    : address_(address), size_(size) {

#ifdef SPUD_OS_WIN
  DWORD protection = 0;
  if (protect == mem_protection::READ_WRITE_EXECUTE) {
    protection = PAGE_EXECUTE_READWRITE;
  } else if (protect == mem_protection::READ) {
    protection = PAGE_READONLY;
  } else if (protect == mem_protection::EXECUTE) {
    protection = PAGE_EXECUTE;
  } else if (protect == mem_protection::WRITE ||
             protect == mem_protection::READ_WRITE) {
    protection = PAGE_READWRITE;
  }

  DWORD old_protection = 0;
  VirtualProtect(reinterpret_cast<LPVOID>(address_), size_, protection,
                 &old_protection);
  original_protection_ = old_protection;
#elif SPUD_OS_APPLE
  mach_port_t port = mach_task_self();
  vm_prot_t protection = 0;
  if (protect == mem_protection::READ_WRITE_EXECUTE) {
    protection = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY;
  } else if (protect == mem_protection::READ) {
    protection = VM_PROT_READ;
  } else if (protect == mem_protection::EXECUTE) {
    protection = VM_PROT_READ | VM_PROT_EXECUTE;
  } else if (protect == mem_protection::WRITE ||
             protect == mem_protection::READ_WRITE) {
    protection = VM_PROT_READ | VM_PROT_WRITE;
  }

  vm_size_t vmsize;
  vm_address_t addr = (vm_address_t)address_;
  vm_region_basic_info_data_64_t info;
  mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
  memory_object_name_t object;

  kern_return_t status =
      vm_region_64(mach_task_self(), &addr, &vmsize, VM_REGION_BASIC_INFO_64,
                   (vm_region_info_t)&info, &info_count, &object);

  original_protection_ =
      (info.protection & VM_PROT_READ) | (info.protection & VM_PROT_WRITE) |
      (info.protection & VM_PROT_EXECUTE) | (info.protection & VM_PROT_COPY);

  auto err =
      mach_vm_protect(mach_task_self(), address_, size_, FALSE, protection);
  assert(err == KERN_SUCCESS);
#elif SPUD_OS_LINUX
  auto page_size = sysconf(_SC_PAGE_SIZE);
  auto aligned_address = ((address) & ~((page_size)-1));
  size = size + address - aligned_address;
  size = (size + page_size - 1) & ~(page_size - 1);

  int protection = 0;
  if (protect == mem_protection::READ_WRITE_EXECUTE) {
    protection = PROT_READ | PROT_WRITE | PROT_EXEC;
  } else if (protect == mem_protection::READ) {
    protection = PROT_READ;
  } else if (protect == mem_protection::EXECUTE) {
    protection = PROT_READ | PROT_EXEC;
  } else if (protect == mem_protection::WRITE ||
             protect == mem_protection::READ_WRITE) {
    protection = PROT_READ | PROT_WRITE;
  }

  mprotect(reinterpret_cast<void *>(aligned_address), size, protection);
#endif
}
protection_scope::~protection_scope() {
  if (address_ == 0) {
    return;
  }
#if SPUD_OS_WIN
  DWORD old_protection = 0;
  VirtualProtect(reinterpret_cast<LPVOID>(address_), size_,
                 static_cast<DWORD>(original_protection_), &old_protection);
#elif SPUD_OS_APPLE
  auto err = mach_vm_protect(mach_task_self(), address_, size_, FALSE,
                             vm_prot_t(original_protection_));
  assert(err == KERN_SUCCESS);
#endif
}

} // namespace spud
