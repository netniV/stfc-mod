#include "relocators.h"
#include "asmjit/core/globals.h"
#include "detour/detour_impl.h"

#include <spud/utils.h>

#include <capstone/arm64.h>

#include <cassert>

namespace spud::detail::arm64 {

using namespace asmjit;
using namespace asmjit::a64;

const static relocation_meta generic_relocator = {
    .size = sizeof(uintptr_t),
    .gen_relo_data =
        [](auto target, auto &relo, auto data_label, Assembler &assembler,
           auto &) {
          auto target_start = reinterpret_cast<uintptr_t>(target.data());
          ZyanU64 absolute_target = 0;
          const auto &op = relo.detail.operands[1];
          if (op.type == AARCH64_OP_IMM) {
            const auto offset = op.imm;
            assembler.bind(data_label);
            assembler.embed(&offset, sizeof(offset));
            assembler.embed(&offset, sizeof(offset));
          } else {
            assert(false && "Expected IMM register");
          }
        },
    .gen_relo_code =
        [](std::span<uint8_t>, const relocation_entry &relo,
           const relocation_info &, asmjit::Label relocation_data,
           Assembler &assembler) {
          auto instruction = relo.instruction;

          const auto &detail = relo.detail;
          const auto &op = detail.operands[0];

          GpX target_register;
          switch (op.reg) {
          case ARM64_REG_X0:
            target_register = x0;
            break;
          case ARM64_REG_X1:
            target_register = x1;
            break;
          case ARM64_REG_X2:
            target_register = x2;
            break;
          case ARM64_REG_X3:
            target_register = x3;
            break;
          case ARM64_REG_X4:
            target_register = x4;
            break;
          case ARM64_REG_X5:
            target_register = x5;
            break;
          case ARM64_REG_X6:
            target_register = x6;
            break;
          case ARM64_REG_X7:
            target_register = x7;
            break;
          case ARM64_REG_X8:
            target_register = x8;
            break;
          case ARM64_REG_X9:
            target_register = x9;
            break;
          case ARM64_REG_X10:
            target_register = x10;
            break;
          case ARM64_REG_X11:
            target_register = x11;
            break;
          case ARM64_REG_X12:
            target_register = x12;
            break;
          case ARM64_REG_X13:
            target_register = x13;
            break;
          case ARM64_REG_X14:
            target_register = x14;
            break;
          case ARM64_REG_X15:
            target_register = x15;
            break;
          case ARM64_REG_X16:
            target_register = x16;
            break;
          case ARM64_REG_X17:
            target_register = x17;
            break;
          case ARM64_REG_X18:
            target_register = x18;
            break;
          case ARM64_REG_X19:
            target_register = x19;
            break;
          case ARM64_REG_X20:
            target_register = x20;
            break;
          case ARM64_REG_X21:
            target_register = x21;
            break;
          case ARM64_REG_X22:
            target_register = x22;
            break;
          case ARM64_REG_X23:
            target_register = x23;
            break;
          case ARM64_REG_X24:
            target_register = x24;
            break;
          case ARM64_REG_X25:
            target_register = x25;
            break;
          case ARM64_REG_X26:
            target_register = x26;
            break;
          case ARM64_REG_X27:
            target_register = x27;
            break;
          case ARM64_REG_X28:
            target_register = x28;
            break;
          default:
            assert(false && "unsupported register");
          }
          // assembler.adr(target_register, relocation_data);
          //
          assembler.sub(sp, sp, 16);
          // assembler.str(x20, Mem{sp, 0});
          assembler.ldr(target_register, Mem{relocation_data, 0});
          // assembler.mov(target_register, x20);
          // assembler.ldr(x20, Mem{sp, 0});
          assembler.add(sp, sp, 16);
        }};

const static relocation_meta branch_relocator = {
    .size = sizeof(uintptr_t),
    .gen_relo_data =
        [](auto target, auto &relo, auto data_label, Assembler &assembler,
           auto &) {
          const auto label_error = assembler.bind(data_label);
          ASMJIT_ASSERT(label_error == kErrorOk);
          SPUD_UNUSED(label_error);
          auto has_group = [&](uint8_t group) {
            for (size_t i = 0; i < relo.instruction.detail->groups_count; i++) {
              if (relo.instruction.detail->groups[i] == group) {
                return true;
              }
            }
            return false;
          };
          const auto &detail = relo.detail;
          intptr_t result = 0;
          if (has_group(ARM64_GRP_BRANCH_RELATIVE) ||
              has_group(ARM64_GRP_JUMP) || has_group(ARM64_GRP_CALL)) {
            result = detail.operands[detail.op_count - 1].imm;
          }
          const auto target_start = reinterpret_cast<intptr_t>(target.data());
          Label L1 = assembler.newLabel();
          assembler.ldr(x16, ptr(L1));
          assembler.br(x16);
          assembler.bind(L1);
          assembler.embed(&result, sizeof(result));
        },
    .gen_relo_code = [](std::span<uint8_t>, const relocation_entry &relo,
                        const relocation_info &, asmjit::Label relocation_data,
                        Assembler &assembler) {},
    .copy_instruction = true};

const relocation_meta &
get_relocator_for_instruction(const cs_insn &instruction) {
  auto has_group = [&](uint8_t group) {
    for (size_t i = 0; i < instruction.detail->groups_count; i++) {
      if (instruction.detail->groups[i] == group) {
        return true;
      }
    }
    return false;
  };
  if (has_group(ARM64_GRP_BRANCH_RELATIVE) || has_group(ARM64_GRP_JUMP) ||
      has_group(ARM64_GRP_CALL)) {
    return branch_relocator;
  }
  return generic_relocator;
}

} // namespace spud::detail::arm64
