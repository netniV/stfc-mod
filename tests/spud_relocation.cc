#include <spud/detour.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#if _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

// Match the gallery constructor's sub/cmp/jne/lea/call prologue. Both the
// RIP-relative cmp and lea expand in the trampoline before the relative call.
// The helper increments a counter, so both branch paths and call-through are
// checked by executing the installed detour, without a game installation.
int main()
{
  constexpr size_t size = 4096;
#if _WIN32
  auto* code = static_cast<uint8_t*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
#else
  auto* code = static_cast<uint8_t*>(mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                                         MAP_PRIVATE | MAP_ANON, -1, 0));
  if (code == MAP_FAILED) code = nullptr;
#endif
  if (!code) return 1;

  const uint8_t body[] = {
      0x48, 0x83, 0xec, 0x28,                   // sub rsp, 40
      0x80, 0x3d, 0x75, 0x00, 0x00, 0x00, 0,  // cmp byte [code+128], 0
      0x75, 0x18,                               // jne code+37
      0x48, 0x8d, 0x0d, 0x70, 0, 0, 0,         // lea rcx, [code+132]
      0xe8, 0x27, 0, 0, 0,                     // call code+64
      0x8b, 0x05, 0x65, 0, 0, 0,               // mov eax, [code+132]
      0x48, 0x83, 0xc4, 0x28,                   // add rsp, 40
      0xc3, 0x90,                               // ret; nop
      0x8b, 0x05, 0x59, 0, 0, 0,               // mov eax, [code+132]
      0x48, 0x83, 0xc4, 0x28, 0xc3};            // add rsp, 40; ret
  const uint8_t helper[] = {0xff, 0x05, 0x3e, 0, 0, 0, 0xc3}; // inc dword [code+132]; ret
  std::memcpy(code, body, sizeof(body));
  std::memcpy(code + 64, helper, sizeof(helper));
#if _WIN32
  FlushInstructionCache(GetCurrentProcess(), code, size);
#else
  __builtin___clear_cache(reinterpret_cast<char*>(code), reinterpret_cast<char*>(code + size));
#endif

  using Function = int (*)(void*);
  auto function = reinterpret_cast<Function>(code);
  bool passed = function(nullptr) == 1;
  {
    auto hook = spud::detour<int(Function, void*)>::create(
        function, +[](Function original, void* value) { return original(value) + 100; });
    hook.install();
    passed = passed && function(nullptr) == 102;
    code[128] = 1;
    passed = passed && function(nullptr) == 102;
    code[128] = 0;
    passed = passed && function(nullptr) == 103;
  }
  passed = passed && function(nullptr) == 4;
#if _WIN32
  VirtualFree(code, 0, MEM_RELEASE);
#else
  munmap(code, size);
#endif
  std::puts(passed ? "SPUD multi-relocation call/branch regression passed" : "SPUD relocation regression FAILED");
  return passed ? 0 : 1;
}
