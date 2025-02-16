#include <spud/arch.h>
#include <spud/utils.h>

#ifdef SPUD_OS_WIN
#include <Windows.h>
#elif SPUD_OS_APPLE
#include <libkern/OSCacheControl.h>
#include <pthread.h>
#include <sys/mman.h>
#elif SPUD_OS_LINUX
#include <sys/mman.h>
#else
#error "Unsupported platform"
#endif

#include <cstdint>
#include <cstdio>

namespace spud {
void *alloc_executable_memory(size_t size) {
#ifdef SPUD_OS_WIN
  return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE,
                      PAGE_EXECUTE_READWRITE);
#elif SPUD_OS_APPLE
  auto addr = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT, -1, 0);
  return addr;
#elif SPUD_OS_LINUX
  auto addr = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  return addr;
#endif
}

void free_executable_memory(void *ptr, size_t size) {
#ifdef SPUD_OS_WIN
  VirtualFree(ptr, size, MEM_RELEASE);
#elif SPUD_OS_APPLE
  munmap(ptr, size);
#elif SPUD_OS_LINUX
  munmap(ptr, size);
#endif
}

void disable_jit_write_protection() {
#if SPUD_OS_APPLE
  pthread_jit_write_protect_np(0);
#endif
}

void enable_jit_write_protection() {
#if SPUD_OS_APPLE
  pthread_jit_write_protect_np(1);
#endif
}

} // namespace spud
