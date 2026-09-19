#pragma once
#if defined(_WIN32) && defined(_M_X64)
#include <Windows.h>
namespace mod_settings
{
// x64 SPUD reserves 24 bytes. Reject tiny thunks and interior entry points;
// exact-client evidence is still required before enabling a new hook target.
inline bool WindowsHookFits(const void* method)
{
  DWORD64     base    = 0;
  const auto  address = reinterpret_cast<DWORD64>(method);
  const auto* entry   = method ? RtlLookupFunctionEntry(address, &base, nullptr) : nullptr;
  return entry && base + entry->BeginAddress == address && entry->EndAddress - entry->BeginAddress >= 64;
}
} // namespace mod_settings
#endif
