#include "patches/native_hook_extent.h"
#include <cstdio>

// Real Mach-O entries exercise the loaded image, not a mocked function table.
// Keep the bodies independent of optimizer/prologue choices on both CPUs.
__attribute__((naked, noinline, used)) void LongEntry()
{ __asm__ volatile(".rept 96\n nop\n .endr\n ret"); }
__attribute__((naked, noinline, used)) void ShortEntry()
{
#if defined(__aarch64__)
  __asm__ volatile(".rept 10\n nop\n .endr\n ret");
#else
  __asm__ volatile(".rept 40\n nop\n .endr\n ret");
#endif
}
__attribute__((naked, noinline, used)) void PaddedReturn()
{ __asm__ volatile("ret\n .rept 96\n nop\n .endr"); }
__attribute__((naked, noinline, used)) void TailThunk()
{
#if defined(__aarch64__)
  __asm__ volatile("b 1f\n .rept 96\n nop\n .endr\n 1: ret");
#else
  __asm__ volatile("jmp 1f\n .rept 96\n nop\n .endr\n 1: ret");
#endif
}
__attribute__((naked, noinline, used)) void LastEntry()
{ __asm__ volatile("ret"); }

int main()
{
  const auto* entry = reinterpret_cast<const unsigned char*>(&LongEntry);
  const auto* short_entry = reinterpret_cast<const void*>(&ShortEntry);
  if (native_hooks::MacHookFits(short_entry) || !native_hooks::MacHookFits(short_entry, 32)
      || native_hooks::MacHookFits(reinterpret_cast<const void*>(&PaddedReturn), 32)
      || native_hooks::MacHookFits(reinterpret_cast<const void*>(&TailThunk), 32)
      || !native_hooks::MacHookFits(entry) || native_hooks::MacHookFits(entry + 4)
      || native_hooks::MacHookFits(reinterpret_cast<const void*>(&PaddedReturn))
      || native_hooks::MacHookFits(reinterpret_cast<const void*>(&TailThunk)) || native_hooks::MacHookFits(nullptr)) {
    std::fputs("Mac hook extent regression failed\n", stderr);
    return 1;
  }
  std::puts("Mac hook extent regression passed");
}
