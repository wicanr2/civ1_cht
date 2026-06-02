// VCPU.h — 16-bit x86 virtual CPU core for the OpenCiv1 C++/SDL2 port.
//
// Faithful port of OpenCiv1's IRB.VirtualCPU (VCPU.cs / VCPURegister.cs /
// VCPUFlags.cs). Method names, register views and flag semantics mirror the C#
// 1:1 so the ~80k lines of transliterated-asm CodeObjects can be ported nearly
// mechanically. Pure integer arithmetic — fully testable headless (--selftest).
//
// Register getters are no-arg; setters take an argument:
//     AX.u16()        // read  (C#: AX.UInt16)
//     AX.u16(0x1234)  // write (C#: AX.UInt16 = 0x1234)
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace oc1 {

// 32-bit-backed register with 8/16/32-bit views (matches VCPURegister.cs).
struct Register {
    uint32_t value = 0;

    Register() = default;
    explicit Register(uint32_t v) : value(v) {}

    uint8_t lo() const { return uint8_t(value & 0xFF); }                 // LowUInt8
    void    lo(uint8_t v) { value = (value & 0xFFFFFF00u) | v; }
    int8_t  loS() const { return int8_t(value & 0xFF); }                 // LowInt8

    uint8_t hi() const { return uint8_t((value & 0xFF00) >> 8); }        // HighUInt8
    void    hi(uint8_t v) { value = (value & 0xFFFF00FFu) | (uint32_t(v) << 8); }

    uint16_t u16() const { return uint16_t(value & 0xFFFF); }            // UInt16
    void     u16(uint16_t v) { value = (value & 0xFFFF0000u) | v; }
    int16_t  s16() const { return int16_t(uint16_t(value & 0xFFFF)); }   // Int16
    void     s16(int16_t v) { value = (value & 0xFFFF0000u) | uint16_t(v); }

    uint16_t hiU16() const { return uint16_t((value & 0xFFFF0000u) >> 16); }
    void     hiU16(uint16_t v) { value = (value & 0xFFFFu) | (uint32_t(v) << 16); }

    uint32_t u32() const { return value; }                              // UInt32
    void     u32(uint32_t v) { value = v; }
    int32_t  s32() const { return int32_t(value); }
    void     s32(int32_t v) { value = uint32_t(v); }

    void incU16() { u16(uint16_t(u16() + 1)); }
    void decU16() { u16(uint16_t(u16() - 1)); }
};

// Status flags (matches VCPUFlags.cs): only C, Z, S, D, O are modelled; the
// signed/unsigned condition accessors are what the ported asm reads.
struct Flags {
    bool C = false; // carry
    bool Z = false; // zero (== E)
    bool S = false; // sign
    bool D = false; // direction
    bool O = false; // overflow

    bool NC() const { return !C; }
    bool E()  const { return Z; }
    bool NZ() const { return !Z; }
    bool NE() const { return !Z; }
    bool A()  const { return !C && !Z; }
    bool AE() const { return !C; }
    bool B()  const { return C; }
    bool BE() const { return C || Z; }
    bool G()  const { return !((S ^ O) | Z); }
    bool GE() const { return !(S ^ O); }
    bool L()  const { return S ^ O; }
    bool LE() const { return (S ^ O) | Z; }
    bool NO() const { return !O; }
    bool NS() const { return !S; }
};

// No-op block logger so CodeObject ports that call CPU.Log.EnterBlock(...) /
// ExitBlock() compile unchanged. Swap for a real tracer when debugging.
struct BlockLog {
    void EnterBlock(const std::string&) {}
    void ExitBlock() {}
};

class VCPU {
public:
    Register AX, BX, CX, DX, SI, DI, BP, SP;
    Register CS, DS, ES, SS, Temp;
    Flags    flags;
    BlockLog Log;

    VCPU();

    static uint32_t ToLinearAddress(uint16_t seg, uint16_t off) {
        return (uint32_t(seg) << 4) + off;
    }

    // --- memory (1 MiB real-mode, segment:offset) ---
    uint8_t  ReadUInt8 (uint16_t seg, uint16_t off) const;
    int8_t   ReadInt8  (uint16_t seg, uint16_t off) const;
    uint16_t ReadUInt16(uint16_t seg, uint16_t off) const;
    int16_t  ReadInt16 (uint16_t seg, uint16_t off) const;
    uint32_t ReadUInt32(uint16_t seg, uint16_t off) const;
    int32_t  ReadInt32 (uint16_t seg, uint16_t off) const;
    void WriteUInt8 (uint16_t seg, uint16_t off, uint8_t v);
    void WriteInt8  (uint16_t seg, uint16_t off, int8_t v);
    void WriteUInt16(uint16_t seg, uint16_t off, uint16_t v);
    void WriteInt16 (uint16_t seg, uint16_t off, int16_t v);
    void WriteUInt32(uint16_t seg, uint16_t off, uint32_t v);
    void WriteInt32 (uint16_t seg, uint16_t off, int32_t v);

    uint8_t  memU8 (uint32_t addr) const { return mem_[addr & 0xFFFFF]; }
    void     memU8 (uint32_t addr, uint8_t v) { mem_[addr & 0xFFFFF] = v; }
    uint16_t memU16(uint32_t addr) const;
    void     memU16(uint32_t addr, uint16_t v);
    uint8_t*       raw()       { return mem_.data(); }
    const uint8_t* raw() const { return mem_.data(); }

    std::string ReadString(uint16_t seg, uint16_t off) const { return ReadString(ToLinearAddress(seg, off)); }
    std::string ReadString(uint32_t addr) const;
    void WriteString(uint32_t addr, const std::string& s);
    void WriteString(uint32_t addr, const std::string& s, int maxLength);

    // --- stack ---
    void     PUSH_UInt16(uint16_t v);
    uint16_t POP_UInt16();
    void     PUSH_UInt32(uint32_t v);
    uint32_t POP_UInt32();
    void     PUSHA_UInt16();
    void     POPA_UInt16();

    // --- helpers ---
    void UInt32ToTwoUInt16(Register& lo, Register& hi, uint32_t value);
    uint32_t TwoUInt16ToUInt32(uint16_t lo, uint16_t hi);

    // --- ALU / instructions (names + flag semantics match VCPU.cs) ---
    uint8_t  ADC_UInt8 (uint8_t a, uint8_t b);
    uint16_t ADC_UInt16(uint16_t a, uint16_t b);
    uint8_t  ADD_UInt8 (uint8_t a, uint8_t b);
    uint16_t ADD_UInt16(uint16_t a, uint16_t b);
    uint8_t  AND_UInt8 (uint8_t a, uint8_t b);
    uint16_t AND_UInt16(uint16_t a, uint16_t b);
    void     CMP_UInt8 (uint8_t a, uint8_t b);
    void     CMP_UInt16(uint16_t a, uint16_t b);
    void     CBW(Register& regAX);
    void     CWD(Register& regAX, Register& regDX);
    void     DAS_UInt8();
    uint8_t  DEC_UInt8 (uint8_t a);
    uint16_t DEC_UInt16(uint16_t a);
    void     DIV_UInt16 (Register& regAX, Register& regDX, uint16_t value);
    void     IDIV_UInt8 (Register& regAX, uint8_t value);
    void     IDIV_UInt16(Register& regAX, Register& regDX, uint16_t value);
    void     IMUL_UInt8 (Register& regAX, uint8_t v1);
    void     IMUL_UInt16(Register& regAX, Register& regDX, uint16_t v1);
    uint16_t IMUL1_UInt16(uint16_t v1, uint16_t v2);
    uint8_t  INC_UInt8 (uint8_t a);
    uint16_t INC_UInt16(uint16_t a);
    bool     Loop_UInt16(Register& regCX);
    void     LODS_UInt8();
    void     LODS_UInt16();
    uint16_t MOVSX_UInt16(uint16_t v);
    uint32_t MOVSX_UInt32(uint32_t v);
    uint16_t MOVZX_UInt16(uint16_t v);
    uint32_t MOVZX_UInt32(uint32_t v);
    void     MOVS_UInt8 (Register& sReg, Register& regSI, Register& regES, Register& regDI);
    void     REPE_MOVS_UInt8 (Register& sReg, Register& regSI, Register& regES, Register& regDI, Register& regCX);
    void     MOVS_UInt16(Register& sReg, Register& regSI, Register& regES, Register& regDI);
    void     REPE_MOVS_UInt16(Register& sReg, Register& regSI, Register& regES, Register& regDI, Register& regCX);
    void     MUL_UInt8 (Register& regAX, uint8_t value);
    void     MUL_UInt16(Register& regDX, Register& regAX, uint16_t value);
    uint8_t  NEG_UInt8 (uint8_t a);
    uint16_t NEG_UInt16(uint16_t a);
    uint8_t  NOT_UInt8 (uint8_t a);
    uint16_t NOT_UInt16(uint16_t a);
    uint8_t  OR_UInt8 (uint8_t a, uint8_t b);
    uint16_t OR_UInt16(uint16_t a, uint16_t b);
    uint8_t  RCL_UInt8 (uint8_t a, uint8_t cnt);
    uint16_t RCL_UInt16(uint16_t a, uint8_t cnt);
    uint8_t  RCR_UInt8 (uint8_t a, uint8_t cnt);
    uint16_t RCR_UInt16(uint16_t a, uint16_t cnt);
    uint8_t  ROL_UInt8 (uint8_t a, uint8_t cnt);
    uint16_t ROL_UInt16(uint16_t a, uint8_t cnt);
    uint8_t  ROR_UInt8 (uint8_t a, uint8_t cnt);
    uint16_t ROR_UInt16(uint16_t a, uint8_t cnt);
    uint8_t  SAR_UInt8 (uint8_t a, uint8_t cnt);
    uint16_t SAR_UInt16(uint16_t a, uint8_t cnt);
    uint8_t  SBB_UInt8 (uint8_t a, uint8_t b);
    uint16_t SBB_UInt16(uint16_t a, uint16_t b);
    void     SCAS_UInt8();
    void     REPNE_SCAS_UInt8();
    void     CMPS_UInt8(Register& regES, Register& regDI, Register& sReg, Register& regSI);
    void     REPE_CMPS_UInt8(Register& regES, Register& regDI, Register& sReg, Register& regSI);
    uint8_t  SHL_UInt8 (uint8_t a, uint8_t cnt);
    uint16_t SHL_UInt16(uint16_t a, uint8_t cnt);
    uint16_t SHLD1_UInt16(uint16_t v1, uint16_t v2, uint8_t cnt);
    uint8_t  SHR_UInt8 (uint8_t a, uint8_t cnt);
    uint16_t SHR_UInt16(uint16_t a, uint8_t cnt);
    void     STOS_UInt8();
    void     REPE_STOS_UInt8();
    void     STOS_UInt16();
    void     REPE_STOS_UInt16();
    uint8_t  SUB_UInt8 (uint8_t a, uint8_t b);
    uint16_t SUB_UInt16(uint16_t a, uint16_t b);
    void     TEST_UInt8 (uint8_t a, uint8_t b);
    void     TEST_UInt16(uint16_t a, uint16_t b);
    uint8_t  XLAT_UInt8(Register& sReg);
    uint8_t  XOR_UInt8 (uint8_t a, uint8_t b);
    uint16_t XOR_UInt16(uint16_t a, uint16_t b);

private:
    std::vector<uint8_t> mem_;
};

} // namespace oc1
