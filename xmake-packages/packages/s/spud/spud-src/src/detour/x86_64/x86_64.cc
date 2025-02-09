#include "x86_64.h"

#include "detour/detour_impl.h"
#include "relocators.h"

#include <spud/detour.h>
#include <spud/memory/protection.h>
#include <spud/utils.h>

// Private helper stuff
#include <zydis_utils.h>

#include <Zycore/Status.h>
#include <Zycore/Types.h>
#include <Zydis/DecoderTypes.h>
#include <Zydis/Utils.h>
#include <Zydis/Zydis.h>

#include <asmjit/asmjit.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <vector>

// Here shall be dragons, at some point

namespace spud {
namespace detail {
namespace x64 {

struct RelocationResult {
  std::vector<asmjit::Label> data_labels;
  size_t copy_offset;
  std::vector<std::pair<uintptr_t, uintptr_t>> relocation_offsets;
};

static RelocationResult
do_far_relocations(std::span<uint8_t> target,
                   const relocation_info &relocation_info,
                   asmjit::CodeHolder &code, asmjit::x86::Assembler &assembler);
static void write_relocation_data(
    std::span<uint8_t> target, const relocation_info &relocation_info,
    std::vector<asmjit::Label> relocation_data,
    std::vector<std::pair<uintptr_t, uintptr_t>> relocation_offsets,
    asmjit::CodeHolder &code, asmjit::x86::Assembler &assembler);

static bool is_jump(ZydisDecodedInstruction &instruction) {
  // TODO(alexander): This is less than ideal
  // actually we need relative plus jump?
  const auto non_ret_branch =
      (instruction.meta.branch_type != ZYDIS_BRANCH_TYPE_NONE) &&
      (instruction.mnemonic != ZYDIS_MNEMONIC_RET);

  if (non_ret_branch) {
    assert((instruction.attributes & ZYDIS_ATTRIB_IS_RELATIVE) != 0);
  }
  return non_ret_branch;
}

static bool
needs_relocate(uintptr_t decoder_offset, uintptr_t code_end,
               uintptr_t jump_size, ZydisDecodedInstruction &instruction,
               ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]) {

  if ((instruction.attributes & ZYDIS_ATTRIB_IS_RELATIVE) == 0) {
    return false;
  }

  constexpr auto kRuntimeAddress = 0x7700000000u;

  for (auto i = 0; i < instruction.operand_count; ++i) {
    ZyanU64 result = 0;
    const auto &operand = operands[i];
    if (ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&instruction, &operand,
                                              kRuntimeAddress + decoder_offset,
                                              &result))) {
      const auto instruction_start = decoder_offset;
      const auto inside_trampoline =
          instruction_start < code_end || instruction_start <= jump_size;

      const auto reaches_into =
          (!inside_trampoline && result > kRuntimeAddress &&
           result <= (kRuntimeAddress + code_end));
      const auto reaches_outof =
          (inside_trampoline && (result >= (kRuntimeAddress + code_end) ||
                                 result < kRuntimeAddress));
      const auto need_relocate = (reaches_into || reaches_outof);

      return need_relocate;
    }
  }

  return false;
};

static inline bool decode_instruction(const ZydisDecoder *decoder,
                                      const void *buffer, ZyanUSize length,
                                      ZydisDecodedInstruction *instruction,
                                      ZydisDecodedOperand *operands) {
  ZydisDecoderContext ctx;
  if (ZYAN_FAILED(ZydisDecoderDecodeInstruction(decoder, &ctx, buffer, length,
                                                instruction))) {
    return false;
  }
  if (ZYAN_FAILED(ZydisDecoderDecodeOperands(
          decoder, &ctx, instruction, operands, instruction->operand_count))) {
    return false;
  }
  return true;
}

std::tuple<relocation_info, size_t> collect_relocations(uintptr_t address,
                                                        size_t jump_size) {
  //
  // Create a temp absolute jump
  // * For this we can use a rip+0 jump so we don't spoil a register
  //
  // Look at target location
  //
  // Figure out all the instructions that need relocating
  // * this might require us to go further than absolute jump
  //
  // Copy all instructions found in previous step, into new buffer
  // * New buffer, ideally is in 2GB range, if we can't do that we'll need
  // an intermediary jump/call table for far-ish jumps
  //
  // Relocate all instructions in that buffer to a designated target location
  // * This will be our trampoline, it'll need an absolute at the end as well
  //
  ZydisDecoder decoder;
  ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

  ZydisDecodedInstruction instruction;
  uintptr_t decoder_offset = 0;
  intptr_t decode_length = 0x40;

  relocation_info relocation_info;

  uintptr_t extend_trampoline_to = 0;

  std::vector<relocation_entry> relocations;
  relocations.reserve(10);

  ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
  while (decode_length >= 0 &&
         decode_instruction(
             &decoder, reinterpret_cast<void *>(address + decoder_offset),
             decode_length - decoder_offset, &instruction, operands)) {

    if (needs_relocate(decoder_offset, extend_trampoline_to, jump_size,
                       instruction, operands)) {
      const auto entry = relocation_entry{
          decoder_offset, instruction,
          std::array{operands[0], operands[1], operands[2], operands[3],
                     operands[4], operands[5], operands[6], operands[7],
                     operands[8], operands[9]}};

      /*const auto relocation_meta_info = relo_meta.find(entry.instruction);
      if (relocation_meta_info == relo_meta.end()) {
        char text[50] = {};
        ZydisFormatter formatter;
        ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);
        ZydisFormatterFormatInstruction(&formatter, &instruction, operands,
                                        instruction.operand_count_visible, text,
                                        sizeof(text), kRuntimeAddress,
                                        ZYAN_NULL);
        fprintf(stderr,
                "Instruction that needs relocation is missing relocator: %s\n",
                text);
      }*/

      relocations.emplace_back(entry);
      extend_trampoline_to =
          std::max(decoder_offset + instruction.length, extend_trampoline_to);
    } else if (extend_trampoline_to < jump_size) {
      extend_trampoline_to =
          std::max(decoder_offset + instruction.length, extend_trampoline_to);
    }

    decoder_offset += instruction.length;
  }

  // Remove everything that doesn't actually end up needing relocations after we
  // have determined the total trampoline size
  std::erase_if(relocations, [&](relocation_entry &v) {
    return !(is_jump(v.instruction) ||
             needs_relocate(v.address,
                            extend_trampoline_to - 1,
                            jump_size, v.instruction,
                            v.operands.data()));
  });

  relocation_info.relocations = {relocations.begin(), relocations.end()};

  return {relocation_info, extend_trampoline_to};
}

trampoline_buffer create_trampoline(uintptr_t return_address,
                                    std::span<uint8_t> target,
                                    const relocation_info &relocations) {
  using namespace asmjit;
  using namespace asmjit::x86;

  CodeHolder code;
  code.init(Environment::host(), 0);
  x86::Assembler assembler(&code);

  const auto is_far_relocate = true;

  if (is_far_relocate) {
    // We don't embed the data here as we are re-building instructions, so
    // we can't just copy it
    auto relocation_result =
        do_far_relocations(target, relocations, code, assembler);
    assembler.embed(target.data() + relocation_result.copy_offset,
                    target.size() - relocation_result.copy_offset);
    assembler.jmp(ptr(rip, 0));
    assembler.embed(&return_address, sizeof(return_address));
    write_relocation_data(target, relocations, relocation_result.data_labels,
                          relocation_result.relocation_offsets, code,
                          assembler);
  } else {
    assembler.embed(target.data(), target.size());
    assembler.jmp(ptr(rip, 0));
    assembler.embed(&return_address, sizeof(return_address));
    assert(false && "Once we have 2gb relative allocation, we can use this "
                    "branch to generate a slighly faster version of "
                    "the trampoline");
  }

  auto &buffer = code.textSection()->buffer();

  return {0, {buffer.begin(), buffer.end()}};
}

static void offset_target(auto size, auto target_code, auto target) {
  switch (size) {
  case 8: {
    assert(target < 0xFF && "immediate size too small for relocation");
    *(int8_t *)(target_code) += static_cast<int8_t>(target);
  } break;
  case 16: {
    *(int16_t *)(target_code) += static_cast<int16_t>(target);
  } break;
  case 32: {
    *(int32_t *)(target_code) += static_cast<int32_t>(target);
  } break;
  case 64: {
    *(int64_t *)(target_code) += static_cast<int64_t>(target);
  } break;
  }
}

static RelocationResult do_far_relocations(
    std::span<uint8_t> target, const relocation_info &relocation_info,
    asmjit::CodeHolder &code, asmjit::x86::Assembler &assembler) {
  using namespace asmjit;
  using namespace asmjit::x86;

  std::vector<Label> relocation_data;
  relocation_data.reserve(10);

  size_t copy_offset = 0;

  std::vector<std::pair<uintptr_t, uintptr_t>> relocation_offsets;

  uintptr_t relocation_offset = 0u;
  for (const auto &relocation : relocation_info.relocations) {
    const auto &relo = std::get<x64::relocation_entry>(relocation);
    const auto &r_meta = get_relocator_for_instruction(relo.instruction);
    auto data_label = assembler.newLabel();
    relocation_data.emplace_back(data_label);
    if (r_meta.copy_instruction) {
      // Embed everything up to here from the last end of instruction
      // Including the currently operated on instruction since we are going to
      // modify it
      assembler.embed(target.data() + copy_offset,
                      relo.address - copy_offset + relo.instruction.length);
    } else if ((relo.address - copy_offset) > 0) {
      // Embed everything up to here from the last end of instruction
      // This skips the currently operated on instruction
      assembler.embed(target.data() + copy_offset, relo.address - copy_offset);
    }

    // Place the cursor at the end of the instruction
    copy_offset = relo.address + relo.instruction.length;
    relocation_offsets.emplace_back(std::pair{relo.address, relocation_offset});
    r_meta.gen_relo_code(target, relo, relocation_info, data_label, assembler);
    const auto relocated_size = assembler.offset();
    relocation_offset +=
        relocated_size - relo.address - relo.instruction.length;
  }

  return {relocation_data, copy_offset, relocation_offsets};
}

static void write_adjusted_target(auto size, auto target_code, auto target) {
  switch (size) {
  case 8: {
    assert(target < 0xFF && "immediate size too small for relocation");
    *(int8_t *)(target_code) = static_cast<int8_t>(target);
  } break;
  case 16: {
    *(int16_t *)(target_code) = static_cast<int16_t>(target);
  } break;
  case 32: {
    *(int32_t *)(target_code) = static_cast<int32_t>(target);
  } break;
  case 64: {
    *(int64_t *)(target_code) = static_cast<int64_t>(target);
  } break;
  }
}

static void write_relocation_data(
    std::span<uint8_t> target, const relocation_info &relocation_info,
    std::vector<asmjit::Label> relocation_data,
    std::vector<std::pair<uintptr_t, uintptr_t>> relocation_offsets,
    asmjit::CodeHolder &code, asmjit::x86::Assembler &assembler) {
  size_t relocation_data_idx = 0;
  for (const auto &relocation : relocation_info.relocations) {
    const auto &relo = std::get<x64::relocation_entry>(relocation);
    const auto &r_meta = get_relocator_for_instruction(relo.instruction);
    r_meta.gen_relo_data(target, relo, relocation_data[relocation_data_idx],
                         assembler, relocation_info);

    if (relo.instruction.meta.branch_type != ZYDIS_BRANCH_TYPE_NONE) {

      auto *code_data = code.textSection()->data();

      const auto data_offset = relo.instruction.raw.imm[0].size > 0
                                   ? relo.instruction.raw.imm[0].offset
                                   : relo.instruction.raw.disp.offset;
      const auto data_size = relo.instruction.raw.imm[0].size > 0
                                 ? relo.instruction.raw.imm[0].size
                                 : relo.instruction.raw.disp.size;
      auto target_code = reinterpret_cast<uint8_t *>(code_data + data_offset);

      auto target_start = reinterpret_cast<uintptr_t>(target.data());
      auto target_end = target_start + target.size();

      ZyanU64 jump_target = 0;
      if (ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&relo.instruction,
                                                &relo.operands[0], target_start,
                                                &jump_target))) {

        uintptr_t relocated_location = 0u;
        for (auto &&[k, v] : relocation_offsets) {
          relocated_location = v;
          if (k >= relo.address)
            break;
        }
        relocated_location += relo.address;

        if (code.isLabelBound(relocation_data[relocation_data_idx])) {
          const auto label_jump_target =
              code.labelOffset(relocation_data[relocation_data_idx]);
          const auto new_target =
              label_jump_target - relocated_location - relo.instruction.length;
          write_adjusted_target(data_size, target_code + relocated_location,
                                new_target);
        } else {
          const auto target_offset_address = jump_target - target_start;

          uintptr_t offset = 0u;
          for (auto &&[k, v] : relocation_offsets) {
            offset = v;
            if (k >= target_offset_address)
              break;
          }

          uintptr_t preceeding_relo_offset = 0;
          for (auto &&[k, v] : relocation_offsets) {
            if (k >= target_offset_address)
              break;
            preceeding_relo_offset = v;
          }

          offset_target(data_size, target_code + relocated_location,
                        offset - preceeding_relo_offset);
        }
      } else {
        assert(false && "Failed to caluclate absolute jump target");
      }
    }
    ++relocation_data_idx;
  }
}

std::vector<uint8_t> create_absolute_jump(uintptr_t target_address,
                                          uintptr_t data) {
  using namespace asmjit;
  using namespace asmjit::x86;

  CodeHolder code;
  code.init(Environment{asmjit::Arch::kX64});
  Assembler assembler(&code);

  assembler.mov(r11, data);
  assembler.jmp(ptr(rip, 0));
  assembler.embed(&target_address, sizeof(target_address));

  auto &buffer = code.textSection()->buffer();
  return {buffer.begin(), buffer.end()};
}

uintptr_t maybe_resolve_jump(uintptr_t address) {
  const uint8_t *memory = reinterpret_cast<const uint8_t *>(address);
  if (memory[0] != 0xE9 && memory[0] != 0x55) {
    return address;
  }

  // Resolve code that looks roughly like this
  // 00 push    rbp
  // 01 mov     rbp, rsp
  // 05 pop     rbp
  // 06 jmp     offset
  if (memory[0] == 0x55) {
    if (memory[1] == 0x48 && memory[2] == 0x89 && memory[3] == 0xE5 &&
        memory[4] == 0x5D) {
      return maybe_resolve_jump(address + 5);
    }
    return address;
  }

  int32_t offset = 0;
  std::memcpy(&offset, &memory[1], sizeof(offset));
  return address + offset + 5;
}

} // namespace x64
} // namespace detail
} // namespace spud
