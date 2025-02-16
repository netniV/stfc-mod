#include <Zydis/Utils.h>
#include <Zydis/Zydis.h>
#include <asmjit/asmjit.h>

namespace spud {
asmjit::x86::Gp zydis_reg_to_asmjit(ZydisRegister reg);
asmjit::x86::Xmm zydis_xmm_reg_to_asmjit(ZydisRegister reg);
asmjit::x86::Ymm zydis_ymm_reg_to_asmjit(ZydisRegister reg);
asmjit::x86::Zmm zydis_zmm_reg_to_asmjit(ZydisRegister reg);
} // namespace spud
