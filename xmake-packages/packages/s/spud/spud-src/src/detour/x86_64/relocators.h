#pragma once

#include "x86_64.h"

#include <Zydis/Utils.h>
#include <Zydis/Zydis.h>

#include <asmjit/asmjit.h>

#include <span>
#include <unordered_map>

namespace spud::detail::x64 {

struct relocation_meta {
  uintptr_t size;
  void (*gen_relo_data)(std::span<uint8_t>, const relocation_entry &,
                        asmjit::Label data_label, asmjit::x86::Assembler &,
                        const relocation_info &relocation_info);
  void (*gen_relo_code)(std::span<uint8_t>, const relocation_entry &relo,
                        const relocation_info &relocation_info,
                        asmjit::Label relocation_data,
                        asmjit::x86::Assembler &assembler);
  bool copy_instruction = false;
};

const relocation_meta &
get_relocator_for_instruction(const ZydisDecodedInstruction &instruction);

} // namespace spud::detail::x64
