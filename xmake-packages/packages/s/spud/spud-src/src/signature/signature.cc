#include <spud/arch.h>
#include <spud/signature.h>

#include <array>
#include <bit>
#include <bitset>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#if SPUD_ARCH_X86_FAMILY
#include <immintrin.h>
#if SPUD_OS_WIN
#include <intrin.h>
#endif
#endif

// TODO: clean this up
// private define
#if SPUD_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winnt.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

#if SPUD_OS_MAC
#include <libproc.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/vm_map.h>
#include <syslog.h>
#include <unistd.h>
#endif

namespace spud {
namespace detail {
void generate_mask_and_data(std::string_view signature, std::string &mask,
                            std::string &data) {
  for (auto ch = signature.begin(); ch != signature.end(); ++ch) {
    if (*ch == '?') {
      data += '\0';
      mask += '?';
    } else if (*ch != ' ') {
      const char str[] = {*ch, *(++ch), '\0'};
      const auto digit = strtol(str, nullptr, 16);
      data += static_cast<char>(digit);
      mask += '1';
    }
  }
}

#if SPUD_ARCH_X86_FAMILY
// TODO: This should be elsewhere
static void run_cpuid(uint32_t eax, uint32_t ecx, uint32_t *abcd) {
#if defined(_MSC_VER)
  __cpuidex(reinterpret_cast<int *>(abcd), eax, ecx);
#else
  uint32_t ebx, edx;
#if defined(__i386__) &&                                                       \
    defined(__PIC__) /* in case of PIC under 32-bit EBX cannot be clobbered */
  __asm__("movl %%ebx, %%edi \n\t cpuid \n\t xchgl %%ebx, %%edi"
          : "=D"(ebx),
#else
  __asm__("cpuid"
          : "+b"(ebx),
#endif
            "+a"(eax), "+c"(ecx), "=d"(edx));
  abcd[0] = eax;
  abcd[1] = ebx;
  abcd[2] = ecx;
  abcd[3] = edx;
#endif
}

void find_matches_sse(std::string_view mask, std::string_view data,
                      size_t buffer_end, std::span<uint8_t> search_buffer,
                      std::vector<signature_result> &results);

void find_matches_avx2(std::string_view mask, std::string_view data,
                       size_t buffer_end, std::span<uint8_t> search_buffer,
                       std::vector<signature_result> &results);
#elif SPUD_ARCH_ARM_FAMILY
void find_matches_neon(std::string_view mask, std::string_view data,
                       size_t buffer_end, std::span<uint8_t> search_buffer,
                       std::vector<signature_result> &results);
#endif

std::vector<signature_result> find_matches(std::string_view mask,
                                           std::string_view data,
                                           std::span<uint8_t> search_buffer,
                                           uint32_t features) {
  const auto does_match = [&](uintptr_t offset) {
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
  };

  const auto buffer_end = search_buffer.size() - mask.size();

  std::vector<signature_result> results;

#if SPUD_ARCH_X86_FAMILY
  const bool want_avx2 = ((features & FEATURE_AVX2) != 0);
  const bool want_sse42 = ((features & FEATURE_SSE42) != 0);

  uint32_t abcd[4];
  run_cpuid(7, 0, abcd);
  const auto cpu_has_avx2 = !!(abcd[1] & (1 << 5));
  const auto do_avx2 = cpu_has_avx2 && want_avx2;

  run_cpuid(1, 0, abcd);
  const auto cpu_has_sse42 = !!(abcd[2] & (1 << 19));
  const auto do_sse42 = cpu_has_sse42 && want_sse42;

  if (do_avx2) {
    find_matches_avx2(mask, data, buffer_end, search_buffer, results);
  } else if (do_sse42) {
    find_matches_sse(mask, data, buffer_end, search_buffer, results);
#elif SPUD_ARCH_ARM_FAMILY
  const auto do_neon = true;
  if (do_neon) {
    find_matches_neon(mask, data, buffer_end, search_buffer, results);
#endif
  } else {
    for (size_t offset = 0; offset < buffer_end; ++offset) {
      if (does_match(offset)) {
        results.emplace_back(signature_result{search_buffer, offset});
      }
    }
  }

  return results;
}
} // namespace detail

signature_matches find_matches(std::string_view signature,
                               std::span<uint8_t> search_buffer,
                               uint32_t features) {
  std::string mask;
  std::string data;
  detail::generate_mask_and_data(signature, mask, data);

  std::vector<detail::signature_result> results;
  auto matches = detail::find_matches(mask, data, search_buffer, features);
  results.insert(results.end(), matches.begin(), matches.end());
  return results;
}

#if SPUD_OS_WIN
signature_matches find_in_module(std::string_view signature,
                                 std::string_view module, uint32_t features) {
  std::vector<std::span<uint8_t>> sections;

  const auto module_handle = reinterpret_cast<uintptr_t>(
      GetModuleHandleA(module.size() == 0 ? nullptr : module.data()));
  const auto dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>(module_handle);
  if (dos_header == nullptr) {
    return {};
  }

  if (dos_header->e_magic != IMAGE_DOS_SIGNATURE) {
    return {};
  }

  const auto nt_headers =
      reinterpret_cast<PIMAGE_NT_HEADERS>(module_handle + dos_header->e_lfanew);
  if (nt_headers->Signature != IMAGE_NT_SIGNATURE) {
    return {};
  }

  sections.reserve(nt_headers->FileHeader.NumberOfSections);

  auto nt_section = IMAGE_FIRST_SECTION(nt_headers);
  for (size_t i = 0; i < nt_headers->FileHeader.NumberOfSections;
       i++, nt_section++) {
    if ((nt_section->Characteristics & IMAGE_SCN_MEM_READ) != 0) {
      auto start = reinterpret_cast<uint8_t *>(module_handle +
                                               nt_section->VirtualAddress);
      auto end = start + nt_section->Misc.VirtualSize;
      auto data = std::span(start, end);
      sections.emplace_back(data);
    }
  }

  std::string mask;
  std::string data;
  detail::generate_mask_and_data(signature, mask, data);

  std::vector<detail::signature_result> results;
  for (auto &section : sections) {
    auto matches = detail::find_matches(mask, data, section, features);
    results.insert(results.end(), matches.begin(), matches.end());
  }
  return results;
}
#elif SPUD_OS_MAC
signature_matches find_in_module(std::string_view signature,
                                 std::string_view module, uint32_t features) {
  std::vector<std::span<uint8_t>> sections;

  pid_t pid = getpid();

  std::string_view module_name = module.size() > 0 ? module.data() : "";

  mach_port_t task = mach_task_self();
  mach_vm_address_t address = 0;
  mach_vm_size_t size = 0;
  struct vm_region_submap_info_64 region_info;
  mach_msg_type_number_t info_count = VM_REGION_SUBMAP_INFO_COUNT_64;
  natural_t depth = 0;

  kern_return_t kr;
  while ((kr = mach_vm_region_recurse(task, &address, &size, &depth,
                                      (vm_region_recurse_info_t)&region_info,
                                      &info_count)) == KERN_SUCCESS) {
    char region_path_sz[PROC_PIDPATHINFO_MAXSIZE];
    proc_regionfilename(pid, (uint64_t)address, region_path_sz,
                        sizeof(region_path_sz));
    std::string region_path = region_path_sz;

    if (region_path.ends_with(module_name)) {
      if ((region_info.protection & VM_PROT_EXECUTE)) {
        const auto start = reinterpret_cast<uint8_t *>(address);
        const auto end = start + size;
        const auto data = std::span(start, end);
        sections.emplace_back(data);
      }
    }
    address += size; // Move to the next region
  }

  std::string mask;
  std::string data;
  detail::generate_mask_and_data(signature, mask, data);

  std::vector<detail::signature_result> results;
  for (auto &section : sections) {
    auto matches = detail::find_matches(mask, data, section, features);
    results.insert(results.end(), matches.begin(), matches.end());
  }
  return results;
}
#elif SPUD_OS_LINUX
signature_matches find_in_module(std::string_view signature,
                                 std::string_view module, uint32_t features) {
  std::ifstream maps_file("/proc/self/maps");
  if (!maps_file.is_open()) {
    return {};
  }

  const std::string_view module_name = module.size() > 0 ? module.data() : "";

  std::string mask;
  std::string data;
  detail::generate_mask_and_data(signature, mask, data);

  std::string line;
  std::vector<detail::signature_result> results;
  while (std::getline(maps_file, line)) {
    int name_start = 0, name_end = 0;
    unsigned long addr_start, addr_end;
    char perms_str[8];
    ino_t _inode;
    unsigned int _deviceMajor;
    unsigned int _deviceMinor;
    unsigned long _offset;
    sscanf(line.c_str(), "%lx-%lx %7s %lx %u:%u %lu %n%*[^\n]%n", &addr_start,
           &addr_end, perms_str, &_offset, &_deviceMajor, &_deviceMinor,
           &_inode, &name_start, &name_end);
    std::string map_name;
    if (name_end > name_start) {
      line[name_end] = '\0';
      map_name.assign(&line[name_start]);
    }
    if (map_name.ends_with(module_name) && strchr(perms_str, 'r') &&
        strchr(perms_str, 'x')) {
      const auto section = std::span(reinterpret_cast<uint8_t *>(addr_start),
                                     reinterpret_cast<uint8_t *>(addr_end));
      auto matches = detail::find_matches(mask, data, section, features);
      results.insert(results.end(), matches.begin(), matches.end());
    }
  }
  return results;
}
#endif

} // namespace spud
