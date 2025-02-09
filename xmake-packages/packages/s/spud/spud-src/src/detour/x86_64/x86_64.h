#pragma once

#include "detour/fwd.h"

#include <zydis_utils.h>

#include <Zydis/Utils.h>
#include <Zydis/Zydis.h>

#include <array>
#include <cstdint>
#include <span>
#include <tuple>
#include <vector>

namespace spud::detail::x64 {
/*
    jmp [rip+0]
    0x00000000000
*/
constexpr auto kAbsoluteJumpSize = 10 + 6 + sizeof(uintptr_t);

struct relocation_entry {
  uintptr_t address;
  ZydisDecodedInstruction instruction;
  std::array<ZydisDecodedOperand, ZYDIS_MAX_OPERAND_COUNT> operands;
};

std::vector<uint8_t> create_absolute_jump(uintptr_t target,
                                          uintptr_t container);
std::tuple<relocation_info, size_t> collect_relocations(uintptr_t address,
                                                        size_t jump_size);
trampoline_buffer create_trampoline(uintptr_t return_address,
                                    std::span<uint8_t> target,
                                    const relocation_info &relocation_infos);
uintptr_t maybe_resolve_jump(uintptr_t);

} // namespace spud::detail::x64
