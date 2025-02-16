#include <Zydis/Utils.h>
#include <Zydis/Zydis.h>

#include <asmjit/asmjit.h>

namespace spud {

asmjit::x86::Gp zydis_reg_to_asmjit(ZydisRegister reg) {
  using namespace asmjit;
  using namespace asmjit::x86;

  switch (reg) {
  case ZYDIS_REGISTER_AL:
    return al;
  case ZYDIS_REGISTER_CL:
    return cl;
  case ZYDIS_REGISTER_DL:
    return dl;
  case ZYDIS_REGISTER_BL:
    return bl;
  case ZYDIS_REGISTER_AH:
    return ah;
  case ZYDIS_REGISTER_CH:
    return ch;
  case ZYDIS_REGISTER_DH:
    return dh;
  case ZYDIS_REGISTER_BH:
    return bh;
  case ZYDIS_REGISTER_SPL:
    return spl;
  case ZYDIS_REGISTER_BPL:
    return bpl;
  case ZYDIS_REGISTER_SIL:
    return sil;
  case ZYDIS_REGISTER_DIL:
    return dil;
  case ZYDIS_REGISTER_R8B:
    return r8b;
  case ZYDIS_REGISTER_R9B:
    return r9b;
  case ZYDIS_REGISTER_R10B:
    return r10b;
  case ZYDIS_REGISTER_R11B:
    return r11b;
  case ZYDIS_REGISTER_R12B:
    return r12b;
  case ZYDIS_REGISTER_R13B:
    return r13b;
  case ZYDIS_REGISTER_R14B:
    return r14b;
  case ZYDIS_REGISTER_R15B:
    return r15b;

  case ZYDIS_REGISTER_AX:
    return ax;
  case ZYDIS_REGISTER_CX:
    return cx;
  case ZYDIS_REGISTER_DX:
    return dx;
  case ZYDIS_REGISTER_BX:
    return bx;
  case ZYDIS_REGISTER_SP:
    return sp;
  case ZYDIS_REGISTER_BP:
    return bp;
  case ZYDIS_REGISTER_SI:
    return si;
  case ZYDIS_REGISTER_DI:
    return di;
  case ZYDIS_REGISTER_R8W:
    return r8w;
  case ZYDIS_REGISTER_R9W:
    return r9w;
  case ZYDIS_REGISTER_R10W:
    return r10w;
  case ZYDIS_REGISTER_R11W:
    return r11w;
  case ZYDIS_REGISTER_R12W:
    return r12w;
  case ZYDIS_REGISTER_R13W:
    return r13w;
  case ZYDIS_REGISTER_R14W:
    return r14w;
  case ZYDIS_REGISTER_R15W:
    return r15w;

  case ZYDIS_REGISTER_EAX:
    return eax;
  case ZYDIS_REGISTER_ECX:
    return ecx;
  case ZYDIS_REGISTER_EDX:
    return edx;
  case ZYDIS_REGISTER_EBX:
    return ebx;
  case ZYDIS_REGISTER_ESP:
    return esp;
  case ZYDIS_REGISTER_EBP:
    return ebp;
  case ZYDIS_REGISTER_ESI:
    return esi;
  case ZYDIS_REGISTER_EDI:
    return edi;
  case ZYDIS_REGISTER_R8D:
    return r8d;
  case ZYDIS_REGISTER_R9D:
    return r9d;
  case ZYDIS_REGISTER_R10D:
    return r10d;
  case ZYDIS_REGISTER_R11D:
    return r11d;
  case ZYDIS_REGISTER_R12D:
    return r12d;
  case ZYDIS_REGISTER_R13D:
    return r13d;
  case ZYDIS_REGISTER_R14D:
    return r14d;
  case ZYDIS_REGISTER_R15D:
    return r15d;
  //
  case ZYDIS_REGISTER_RAX:
    return rax;
  case ZYDIS_REGISTER_RCX:
    return rcx;
  case ZYDIS_REGISTER_RDX:
    return rdx;
  case ZYDIS_REGISTER_RBX:
    return rbx;
  case ZYDIS_REGISTER_RSP:
    return rsp;
  case ZYDIS_REGISTER_RBP:
    return rbp;
  case ZYDIS_REGISTER_RSI:
    return rsi;
  case ZYDIS_REGISTER_RDI:
    return rdi;
  case ZYDIS_REGISTER_R8:
    return r8;
  case ZYDIS_REGISTER_R9:
    return r9;
  case ZYDIS_REGISTER_R10:
    return r10;
  case ZYDIS_REGISTER_R11:
    return r11;
  case ZYDIS_REGISTER_R12:
    return r12;
  case ZYDIS_REGISTER_R13:
    return r13;
  case ZYDIS_REGISTER_R14:
    return r14;
  case ZYDIS_REGISTER_R15:
    return r15;
  default:
    assert(false && "Unhandled register");
  }
  return rax;
}

asmjit::x86::Xmm zydis_xmm_reg_to_asmjit(ZydisRegister reg) {
  using namespace asmjit;
  using namespace asmjit::x86;
  switch (reg) {
  case ZYDIS_REGISTER_XMM0:
    return xmm0;
  case ZYDIS_REGISTER_XMM1:
    return xmm1;
  case ZYDIS_REGISTER_XMM2:
    return xmm2;
  case ZYDIS_REGISTER_XMM3:
    return xmm3;
  case ZYDIS_REGISTER_XMM4:
    return xmm4;
  case ZYDIS_REGISTER_XMM5:
    return xmm5;
  case ZYDIS_REGISTER_XMM6:
    return xmm6;
  case ZYDIS_REGISTER_XMM7:
    return xmm7;
  case ZYDIS_REGISTER_XMM8:
    return xmm8;
  case ZYDIS_REGISTER_XMM9:
    return xmm9;
  case ZYDIS_REGISTER_XMM10:
    return xmm10;
  case ZYDIS_REGISTER_XMM11:
    return xmm11;
  case ZYDIS_REGISTER_XMM12:
    return xmm12;
  case ZYDIS_REGISTER_XMM13:
    return xmm13;
  case ZYDIS_REGISTER_XMM14:
    return xmm14;
  case ZYDIS_REGISTER_XMM15:
    return xmm15;
  case ZYDIS_REGISTER_XMM16:
    return xmm16;
  case ZYDIS_REGISTER_XMM17:
    return xmm17;
  case ZYDIS_REGISTER_XMM18:
    return xmm18;
  case ZYDIS_REGISTER_XMM19:
    return xmm19;
  case ZYDIS_REGISTER_XMM20:
    return xmm20;
  case ZYDIS_REGISTER_XMM21:
    return xmm21;
  case ZYDIS_REGISTER_XMM22:
    return xmm22;
  case ZYDIS_REGISTER_XMM23:
    return xmm23;
  case ZYDIS_REGISTER_XMM24:
    return xmm24;
  case ZYDIS_REGISTER_XMM25:
    return xmm25;
  case ZYDIS_REGISTER_XMM26:
    return xmm26;
  case ZYDIS_REGISTER_XMM27:
    return xmm27;
  case ZYDIS_REGISTER_XMM28:
    return xmm28;
  case ZYDIS_REGISTER_XMM29:
    return xmm29;
  case ZYDIS_REGISTER_XMM30:
    return xmm30;
  case ZYDIS_REGISTER_XMM31:
    return xmm31;
  default:
    assert(false && "Unhandled register");
  }
  return xmm0;
}

auto zydis_ymm_reg_to_asmjit(ZydisRegister reg) {
  using namespace asmjit;
  using namespace asmjit::x86;
  switch (reg) {
  case ZYDIS_REGISTER_YMM0:
    return ymm0;
  case ZYDIS_REGISTER_YMM1:
    return ymm1;
  case ZYDIS_REGISTER_YMM2:
    return ymm2;
  case ZYDIS_REGISTER_YMM3:
    return ymm3;
  case ZYDIS_REGISTER_YMM4:
    return ymm4;
  case ZYDIS_REGISTER_YMM5:
    return ymm5;
  case ZYDIS_REGISTER_YMM6:
    return ymm6;
  case ZYDIS_REGISTER_YMM7:
    return ymm7;
  case ZYDIS_REGISTER_YMM8:
    return ymm8;
  case ZYDIS_REGISTER_YMM9:
    return ymm9;
  case ZYDIS_REGISTER_YMM10:
    return ymm10;
  case ZYDIS_REGISTER_YMM11:
    return ymm11;
  case ZYDIS_REGISTER_YMM12:
    return ymm12;
  case ZYDIS_REGISTER_YMM13:
    return ymm13;
  case ZYDIS_REGISTER_YMM14:
    return ymm14;
  case ZYDIS_REGISTER_YMM15:
    return ymm15;
  case ZYDIS_REGISTER_YMM16:
    return ymm16;
  case ZYDIS_REGISTER_YMM17:
    return ymm17;
  case ZYDIS_REGISTER_YMM18:
    return ymm18;
  case ZYDIS_REGISTER_YMM19:
    return ymm19;
  case ZYDIS_REGISTER_YMM20:
    return ymm20;
  case ZYDIS_REGISTER_YMM21:
    return ymm21;
  case ZYDIS_REGISTER_YMM22:
    return ymm22;
  case ZYDIS_REGISTER_YMM23:
    return ymm23;
  case ZYDIS_REGISTER_YMM24:
    return ymm24;
  case ZYDIS_REGISTER_YMM25:
    return ymm25;
  case ZYDIS_REGISTER_YMM26:
    return ymm26;
  case ZYDIS_REGISTER_YMM27:
    return ymm27;
  case ZYDIS_REGISTER_YMM28:
    return ymm28;
  case ZYDIS_REGISTER_YMM29:
    return ymm29;
  case ZYDIS_REGISTER_YMM30:
    return ymm30;
  case ZYDIS_REGISTER_YMM31:
    return ymm31;
  default:
    assert(false && "Unhandled register");
  }
  return ymm0;
}

auto zydis_zmm_reg_to_asmjit(ZydisRegister reg) {
  using namespace asmjit;
  using namespace asmjit::x86;
  switch (reg) {
  case ZYDIS_REGISTER_ZMM0:
    return zmm0;
  case ZYDIS_REGISTER_ZMM1:
    return zmm1;
  case ZYDIS_REGISTER_ZMM2:
    return zmm2;
  case ZYDIS_REGISTER_ZMM3:
    return zmm3;
  case ZYDIS_REGISTER_ZMM4:
    return zmm4;
  case ZYDIS_REGISTER_ZMM5:
    return zmm5;
  case ZYDIS_REGISTER_ZMM6:
    return zmm6;
  case ZYDIS_REGISTER_ZMM7:
    return zmm7;
  case ZYDIS_REGISTER_ZMM8:
    return zmm8;
  case ZYDIS_REGISTER_ZMM9:
    return zmm9;
  case ZYDIS_REGISTER_ZMM10:
    return zmm10;
  case ZYDIS_REGISTER_ZMM11:
    return zmm11;
  case ZYDIS_REGISTER_ZMM12:
    return zmm12;
  case ZYDIS_REGISTER_ZMM13:
    return zmm13;
  case ZYDIS_REGISTER_ZMM14:
    return zmm14;
  case ZYDIS_REGISTER_ZMM15:
    return zmm15;
  case ZYDIS_REGISTER_ZMM16:
    return zmm16;
  case ZYDIS_REGISTER_ZMM17:
    return zmm17;
  case ZYDIS_REGISTER_ZMM18:
    return zmm18;
  case ZYDIS_REGISTER_ZMM19:
    return zmm19;
  case ZYDIS_REGISTER_ZMM20:
    return zmm20;
  case ZYDIS_REGISTER_ZMM21:
    return zmm21;
  case ZYDIS_REGISTER_ZMM22:
    return zmm22;
  case ZYDIS_REGISTER_ZMM23:
    return zmm23;
  case ZYDIS_REGISTER_ZMM24:
    return zmm24;
  case ZYDIS_REGISTER_ZMM25:
    return zmm25;
  case ZYDIS_REGISTER_ZMM26:
    return zmm26;
  case ZYDIS_REGISTER_ZMM27:
    return zmm27;
  case ZYDIS_REGISTER_ZMM28:
    return zmm28;
  case ZYDIS_REGISTER_ZMM29:
    return zmm29;
  case ZYDIS_REGISTER_ZMM30:
    return zmm30;
  case ZYDIS_REGISTER_ZMM31:
    return zmm31;
  default:
    assert(false && "Unhandled register");
  }
  return zmm0;
}
} // namespace spud
