// VCPU.cpp — instruction implementations, ported 1:1 from OpenCiv1 VCPU.cs.
// Flag formulas are copied deliberately verbatim (incl. the model's choice to
// track only C/Z/S/D/O) so behaviour matches the reference engine exactly.
#include "VCPU.h"
#include <stdexcept>

namespace oc1 {

VCPU::VCPU() : mem_(0x100000, 0) {
    SS.u16(0x8000);
    SP.u16(0xFFFE);
}

// ---------------- memory ----------------
uint16_t VCPU::memU16(uint32_t addr) const {
    return uint16_t(mem_[addr & 0xFFFFF]) | (uint16_t(mem_[(addr + 1) & 0xFFFFF]) << 8);
}
void VCPU::memU16(uint32_t addr, uint16_t v) {
    mem_[addr & 0xFFFFF] = uint8_t(v & 0xFF);
    mem_[(addr + 1) & 0xFFFFF] = uint8_t(v >> 8);
}

uint8_t  VCPU::ReadUInt8 (uint16_t s, uint16_t o) const { return memU8(ToLinearAddress(s, o)); }
int8_t   VCPU::ReadInt8  (uint16_t s, uint16_t o) const { return int8_t(memU8(ToLinearAddress(s, o))); }
uint16_t VCPU::ReadUInt16(uint16_t s, uint16_t o) const { return memU16(ToLinearAddress(s, o)); }
int16_t  VCPU::ReadInt16 (uint16_t s, uint16_t o) const { return int16_t(memU16(ToLinearAddress(s, o))); }
uint32_t VCPU::ReadUInt32(uint16_t s, uint16_t o) const {
    uint32_t a = ToLinearAddress(s, o);
    return uint32_t(memU16(a)) | (uint32_t(memU16(a + 2)) << 16);
}
int32_t  VCPU::ReadInt32 (uint16_t s, uint16_t o) const { return int32_t(ReadUInt32(s, o)); }

void VCPU::WriteUInt8 (uint16_t s, uint16_t o, uint8_t v)  { memU8(ToLinearAddress(s, o), v); }
void VCPU::WriteInt8  (uint16_t s, uint16_t o, int8_t v)   { memU8(ToLinearAddress(s, o), uint8_t(v)); }
void VCPU::WriteUInt16(uint16_t s, uint16_t o, uint16_t v) { memU16(ToLinearAddress(s, o), v); }
void VCPU::WriteInt16 (uint16_t s, uint16_t o, int16_t v)  { memU16(ToLinearAddress(s, o), uint16_t(v)); }
void VCPU::WriteUInt32(uint16_t s, uint16_t o, uint32_t v) {
    uint32_t a = ToLinearAddress(s, o);
    memU16(a, uint16_t(v & 0xFFFF));
    memU16(a + 2, uint16_t(v >> 16));
}
void VCPU::WriteInt32 (uint16_t s, uint16_t o, int32_t v)  { WriteUInt32(s, o, uint32_t(v)); }

std::string VCPU::ReadString(uint32_t addr) const {
    std::string out;
    for (;;) {
        uint8_t b = mem_[addr & 0xFFFFF];
        if (b == 0) break;
        out.push_back(char(b));
        ++addr;
    }
    return out;
}
void VCPU::WriteString(uint32_t addr, const std::string& s) {
    for (char c : s) mem_[addr++ & 0xFFFFF] = uint8_t(c);
    mem_[addr & 0xFFFFF] = 0;
}
void VCPU::WriteString(uint32_t addr, const std::string& s, int maxLength) {
    int i = 0;
    for (char c : s) { if (i >= maxLength) break; mem_[addr++ & 0xFFFFF] = uint8_t(c); ++i; }
    mem_[addr & 0xFFFFF] = 0;
}

// ---------------- stack ----------------
void VCPU::PUSH_UInt16(uint16_t v) {
    SP.u16(uint16_t(SP.u16() - 2));
    memU16(ToLinearAddress(SS.u16(), SP.u16()), v);
}
uint16_t VCPU::POP_UInt16() {
    uint16_t v = memU16(ToLinearAddress(SS.u16(), SP.u16()));
    SP.u16(uint16_t(SP.u16() + 2));
    return v;
}
void VCPU::PUSH_UInt32(uint32_t v) {
    PUSH_UInt16(uint16_t(v >> 16));
    PUSH_UInt16(uint16_t(v & 0xFFFF));
}
uint32_t VCPU::POP_UInt32() {
    uint16_t lo = POP_UInt16();
    uint16_t hi = POP_UInt16();
    return uint32_t(lo) | (uint32_t(hi) << 16);
}
void VCPU::PUSHA_UInt16() {
    uint16_t sp = SP.u16();
    PUSH_UInt16(AX.u16()); PUSH_UInt16(CX.u16()); PUSH_UInt16(DX.u16()); PUSH_UInt16(BX.u16());
    PUSH_UInt16(sp);       PUSH_UInt16(BP.u16()); PUSH_UInt16(SI.u16()); PUSH_UInt16(DI.u16());
}
void VCPU::POPA_UInt16() {
    DI.u16(POP_UInt16()); SI.u16(POP_UInt16()); BP.u16(POP_UInt16()); (void)POP_UInt16();
    BX.u16(POP_UInt16()); DX.u16(POP_UInt16()); CX.u16(POP_UInt16()); AX.u16(POP_UInt16());
}

void VCPU::UInt32ToTwoUInt16(Register& lo, Register& hi, uint32_t value) {
    lo.u16(uint16_t(value & 0xFFFF));
    hi.u16(uint16_t((value >> 16) & 0xFFFF));
}
uint32_t VCPU::TwoUInt16ToUInt32(uint16_t lo, uint16_t hi) {
    return uint32_t(lo) | (uint32_t(hi) << 16);
}

// ---------------- ALU ----------------
uint8_t VCPU::ADC_UInt8(uint8_t v1, uint8_t v2) {
    uint8_t cf = flags.C ? 1 : 0;
    uint8_t res = uint8_t(v1 + v2 + cf);
    flags.C = (res < v1) || (cf != 0 && res == v1);
    flags.O = (((v1 ^ v2 ^ 0x80) & (res ^ v2)) & 0x80) != 0;
    flags.S = (res & 0x80) != 0;
    flags.Z = (res == 0);
    return res;
}
uint16_t VCPU::ADC_UInt16(uint16_t v1, uint16_t v2) {
    uint16_t cf = flags.C ? 1 : 0;
    uint16_t res = uint16_t(v1 + v2 + cf);
    flags.C = (res < v1) || (cf != 0 && res == v1);
    flags.O = (((v1 ^ v2 ^ 0x8000) & (res ^ v2)) & 0x8000) != 0;
    flags.S = (res & 0x8000) != 0;
    flags.Z = (res == 0);
    return res;
}
uint8_t VCPU::ADD_UInt8(uint8_t v1, uint8_t v2) {
    uint8_t res = uint8_t(v1 + v2);
    flags.C = (res < v1);
    flags.O = (((v1 ^ v2 ^ 0x80) & (res ^ v2)) & 0x80) != 0;
    flags.S = (res & 0x80) != 0;
    flags.Z = (res == 0);
    return res;
}
uint16_t VCPU::ADD_UInt16(uint16_t v1, uint16_t v2) {
    uint16_t res = uint16_t(v1 + v2);
    flags.C = (res < v1);
    flags.O = (((v1 ^ v2 ^ 0x8000) & (res ^ v2)) & 0x8000) != 0;
    flags.S = (res & 0x8000) != 0;
    flags.Z = (res == 0);
    return res;
}
uint8_t VCPU::AND_UInt8(uint8_t v1, uint8_t v2) {
    uint8_t res = uint8_t(v1 & v2);
    flags.C = false; flags.O = false;
    flags.S = (res & 0x80) != 0; flags.Z = (res == 0);
    return res;
}
uint16_t VCPU::AND_UInt16(uint16_t v1, uint16_t v2) {
    uint16_t res = uint16_t(v1 & v2);
    flags.C = false; flags.O = false;
    flags.S = (res & 0x8000) != 0; flags.Z = (res == 0);
    return res;
}
void VCPU::CMP_UInt8(uint8_t v1, uint8_t v2) {
    uint8_t res = uint8_t(v1 - v2);
    flags.C = (v1 < v2);
    flags.O = (((v1 ^ v2) & (v1 ^ res)) & 0x80) != 0;
    flags.S = (res & 0x80) != 0; flags.Z = (res == 0);
}
void VCPU::CMP_UInt16(uint16_t v1, uint16_t v2) {
    uint16_t res = uint16_t(v1 - v2);
    flags.C = (v1 < v2);
    flags.O = (((v1 ^ v2) & (v1 ^ res)) & 0x8000) != 0;
    flags.S = (res & 0x8000) != 0; flags.Z = (res == 0);
}
void VCPU::CBW(Register& regAX) { regAX.hi((regAX.lo() & 0x80) ? 0xFF : 0x00); }
void VCPU::CWD(Register& regAX, Register& regDX) { regDX.u16((regAX.u16() & 0x8000) ? 0xFFFF : 0x0000); }

void VCPU::DAS_UInt8() {
    uint8_t osigned = uint8_t(AX.lo() & 0x80);
    if ((AX.lo() & 0x0F) > 9) {
        if (AX.lo() > 0x99 || flags.C) { AX.lo(uint8_t(AX.lo() - 0x60)); flags.C = true; }
        else { flags.C = AX.lo() <= 0x05; }
        AX.lo(uint8_t(AX.lo() - 6));
    } else {
        if (AX.lo() > 0x99 || flags.C) { AX.lo(uint8_t(AX.lo() - 0x60)); flags.C = true; }
        else { flags.C = false; }
    }
    flags.O = osigned != 0 && (AX.lo() & 0x80) == 0;
    flags.S = (AX.lo() & 0x80) != 0;
    flags.Z = AX.lo() == 0;
}

uint8_t VCPU::DEC_UInt8(uint8_t v1) {
    uint8_t res = uint8_t(v1 - 1);
    flags.O = (res == 0x7F); flags.S = (res & 0x80) != 0; flags.Z = (res == 0);
    return res;
}
uint16_t VCPU::DEC_UInt16(uint16_t v1) {
    uint16_t res = uint16_t(v1 - 1);
    flags.O = (res == 0x7FFF); flags.S = (res & 0x8000) != 0; flags.Z = (res == 0);
    return res;
}

void VCPU::DIV_UInt16(Register& regAX, Register& regDX, uint16_t value) {
    if (value == 0) throw std::runtime_error("Division by zero");
    uint32_t num = (uint32_t(regDX.u16()) << 16) | uint32_t(regAX.u16());
    uint32_t quo = num / value;
    uint16_t rem = uint16_t(num % value);
    uint16_t quo16 = uint16_t(quo & 0xFFFF);
    if (quo != quo16) throw std::runtime_error("Division error");
    regDX.u16(rem); regAX.u16(quo16);
}
void VCPU::IDIV_UInt8(Register& regAX, uint8_t value) {
    if (value == 0) throw std::runtime_error("Division by zero");
    int16_t valuea = int8_t(value);
    int16_t num = int16_t(regAX.u16());
    int16_t quo = int16_t(num / valuea);
    int8_t rem = int8_t(num % valuea);
    int8_t quo8 = int8_t(uint16_t(quo) & 0xFF);
    if (quo != quo8) throw std::runtime_error("Division error");
    regAX.hi(uint8_t(rem)); regAX.lo(uint8_t(quo8));
}
void VCPU::IDIV_UInt16(Register& regAX, Register& regDX, uint16_t value) {
    if (value == 0) throw std::runtime_error("Division by zero");
    int32_t valuea = int16_t(value);
    int32_t num = int32_t((uint32_t(regDX.u16()) << 16) | uint32_t(regAX.u16()));
    int32_t quo = num / valuea;
    int16_t rem = int16_t(num % valuea);
    int16_t quo16 = int16_t(uint32_t(quo) & 0xFFFF);
    if (quo != quo16) throw std::runtime_error("Division error");
    regDX.u16(uint16_t(rem)); regAX.u16(uint16_t(quo16));
}
void VCPU::IMUL_UInt8(Register& regAX, uint8_t v1) {
    uint16_t res = uint16_t(int16_t(int8_t(regAX.lo())) * int16_t(int8_t(v1)));
    regAX.u16(res);
    if ((res & 0xFF80) == 0xFF80 || (res & 0xFF80) == 0x0) { flags.C = false; flags.O = false; }
    else { flags.C = true; flags.O = true; }
}
void VCPU::IMUL_UInt16(Register& regAX, Register& regDX, uint16_t v1) {
    uint32_t res = uint32_t(int32_t(int16_t(regAX.u16())) * int32_t(int16_t(v1)));
    regAX.u16(uint16_t(res & 0xFFFF));
    regDX.u16(uint16_t((res & 0xFFFF0000u) >> 16));
    if ((res & 0xFFFF8000u) == 0xFFFF8000u || (res & 0xFFFF8000u) == 0x0) { flags.C = false; flags.O = false; }
    else { flags.C = true; flags.O = true; }
}
uint16_t VCPU::IMUL1_UInt16(uint16_t v1, uint16_t v2) {
    uint32_t res = uint32_t(v1) * uint32_t(v2);
    if ((res & 0xFFFF8000u) == 0xFFFF8000u || (res & 0xFFFF8000u) == 0x0) { flags.C = false; flags.O = false; }
    else { flags.C = true; flags.O = true; }
    return uint16_t(res & 0xFFFF);
}
uint8_t VCPU::INC_UInt8(uint8_t v1) {
    uint8_t res = uint8_t(v1 + 1);
    flags.O = (res == 0x80); flags.S = (res & 0x80) != 0; flags.Z = (res == 0);
    return res;
}
uint16_t VCPU::INC_UInt16(uint16_t v1) {
    uint16_t res = uint16_t(v1 + 1);
    flags.O = (res == 0x8000); flags.S = (res & 0x8000) != 0; flags.Z = (res == 0);
    return res;
}
bool VCPU::Loop_UInt16(Register& regCX) { regCX.decU16(); return regCX.u16() != 0; }

void VCPU::LODS_UInt8() {
    AX.lo(ReadUInt8(DS.u16(), SI.u16()));
    if (flags.D) SI.u16(uint16_t(SI.u16() - 1)); else SI.u16(uint16_t(SI.u16() + 1));
}
void VCPU::LODS_UInt16() {
    AX.u16(ReadUInt16(DS.u16(), SI.u16()));
    if (flags.D) SI.u16(uint16_t(SI.u16() - 2)); else SI.u16(uint16_t(SI.u16() + 2));
}

uint16_t VCPU::MOVSX_UInt16(uint16_t v1) { v1 &= 0xFF; return (v1 & 0x80) ? uint16_t(0xFF00 | v1) : v1; }
uint32_t VCPU::MOVSX_UInt32(uint32_t v1) { v1 &= 0xFFFF; return (v1 & 0x8000) ? (0xFFFF0000u | v1) : v1; }
uint16_t VCPU::MOVZX_UInt16(uint16_t v1) { return uint16_t(v1 & 0xFF); }
uint32_t VCPU::MOVZX_UInt32(uint32_t v1) { return v1 & 0xFFFF; }

void VCPU::MOVS_UInt8(Register& sReg, Register& regSI, Register& regES, Register& regDI) {
    WriteUInt8(regES.u16(), regDI.u16(), ReadUInt8(sReg.u16(), regSI.u16()));
    if (flags.D) { regSI.decU16(); regDI.decU16(); } else { regSI.incU16(); regDI.incU16(); }
}
void VCPU::REPE_MOVS_UInt8(Register& sReg, Register& regSI, Register& regES, Register& regDI, Register& regCX) {
    while (regCX.u16() != 0) { MOVS_UInt8(sReg, regSI, regES, regDI); regCX.decU16(); }
}
void VCPU::MOVS_UInt16(Register& sReg, Register& regSI, Register& regES, Register& regDI) {
    WriteUInt16(regES.u16(), regDI.u16(), ReadUInt16(sReg.u16(), regSI.u16()));
    if (flags.D) { regSI.u16(uint16_t(regSI.u16() - 2)); regDI.u16(uint16_t(regDI.u16() - 2)); }
    else        { regSI.u16(uint16_t(regSI.u16() + 2)); regDI.u16(uint16_t(regDI.u16() + 2)); }
}
void VCPU::REPE_MOVS_UInt16(Register& sReg, Register& regSI, Register& regES, Register& regDI, Register& regCX) {
    while (regCX.u16() != 0) { MOVS_UInt16(sReg, regSI, regES, regDI); regCX.decU16(); }
}

void VCPU::MUL_UInt8(Register& regAX, uint8_t value) {
    regAX.u16(uint16_t(uint16_t(regAX.lo()) * uint16_t(value)));
    flags.Z = regAX.lo() == 0;
    if (regAX.hi() != 0) { flags.C = true; flags.O = true; } else { flags.C = false; flags.O = false; }
}
void VCPU::MUL_UInt16(Register& regDX, Register& regAX, uint16_t value) {
    uint32_t t = uint32_t(regAX.u16()) * uint32_t(value);
    regAX.u16(uint16_t(t & 0xFFFFu));
    regDX.u16(uint16_t((t & 0xFFFF0000u) >> 16));
    flags.Z = regAX.u16() == 0;
    if (regDX.u16() != 0) { flags.C = true; flags.O = true; } else { flags.C = false; flags.O = false; }
}

uint8_t VCPU::NEG_UInt8(uint8_t v1) {
    uint8_t res = uint8_t((0x100 - uint16_t(v1)) & 0xFF);
    flags.C = v1 != 0; flags.Z = res == 0; flags.S = (v1 & 0x80) != 0; flags.O = (v1 == 0x80);
    return res;
}
uint16_t VCPU::NEG_UInt16(uint16_t v1) {
    uint16_t res = uint16_t((0x10000u - uint32_t(v1)) & 0xFFFF);
    flags.C = v1 != 0; flags.Z = res == 0; flags.S = (v1 & 0x8000) != 0; flags.O = (v1 == 0x8000);
    return res;
}
uint8_t  VCPU::NOT_UInt8(uint8_t v1)  { return uint8_t(~v1); }
uint16_t VCPU::NOT_UInt16(uint16_t v1) { return uint16_t(~v1); }

uint8_t VCPU::OR_UInt8(uint8_t v1, uint8_t v2) {
    uint8_t res = uint8_t(v1 | v2);
    flags.C = false; flags.O = false; flags.S = (res & 0x80) != 0; flags.Z = (res == 0);
    return res;
}
uint16_t VCPU::OR_UInt16(uint16_t v1, uint16_t v2) {
    uint16_t res = uint16_t(v1 | v2);
    flags.C = false; flags.O = false; flags.S = (res & 0x8000) != 0; flags.Z = (res == 0);
    return res;
}

uint8_t VCPU::RCL_UInt8(uint8_t v1, uint8_t v2) {
    if ((v2 % 9) == 0) return v1;
    v2 %= 9;
    uint8_t cf = flags.C ? 1 : 0;
    uint8_t res = uint8_t((v1 << v2) | (cf << (v2 - 1)) | (v1 >> (9 - v2)));
    flags.O = (((flags.C ? 1 : 0) ^ (res >> 7)) != 0);
    flags.C = (((v1 >> (8 - v2)) & 1) != 0);
    return res;
}
uint16_t VCPU::RCL_UInt16(uint16_t v1, uint8_t v2) {
    if ((v2 % 17) == 0) return v1;
    v2 %= 17;
    uint16_t cf = flags.C ? 1 : 0;
    uint16_t res = uint16_t((v1 << v2) | (cf << (v2 - 1)) | (v1 >> (17 - v2)));
    flags.O = (((flags.C ? 1 : 0) ^ (res >> 15)) != 0);
    flags.C = (((v1 >> (16 - v2)) & 1) != 0);
    return res;
}
uint8_t VCPU::RCR_UInt8(uint8_t v1, uint8_t v2) {
    if ((v2 % 9) == 0) return v1;
    v2 %= 9;
    uint8_t cf = flags.C ? 1 : 0;
    uint8_t res = uint8_t((v1 >> v2) | (cf << (8 - v2)) | (v1 << (9 - v2)));
    flags.C = (((v1 >> (v2 - 1)) & 1) != 0);
    flags.O = (((res ^ (res << 1)) & 0x80) != 0);
    return res;
}
uint16_t VCPU::RCR_UInt16(uint16_t v1, uint16_t v2) {
    if ((v2 % 17) == 0) return v1;
    v2 %= 17;
    uint16_t cf = flags.C ? 1 : 0;
    uint16_t res = uint16_t((v1 >> v2) | (cf << (16 - v2)) | (v1 << (17 - v2)));
    flags.C = (((v1 >> (v2 - 1)) & 1) != 0);
    flags.O = (((res ^ (res << 1)) & 0x8000) != 0);
    return res;
}
uint8_t VCPU::ROL_UInt8(uint8_t v1, uint8_t v2) {
    if ((v2 & 0x7) == 0) {
        if ((v2 & 0x18) != 0) { flags.C = (v1 & 1) != 0; flags.O = (((v1 & 1) ^ (v1 >> 7)) != 0); }
        return v1;
    }
    v2 &= 0x7;
    uint8_t res = uint8_t((v1 << v2) | (v1 >> (8 - v2)));
    flags.C = (res & 1) != 0; flags.O = (((res & 1) ^ (res >> 7)) != 0);
    return res;
}
uint16_t VCPU::ROL_UInt16(uint16_t v1, uint8_t v2) {
    if ((v2 & 0xF) == 0) {
        if ((v2 & 0x10) != 0) { flags.C = (v1 & 1) != 0; flags.O = (((v1 & 1) ^ (v1 >> 15)) != 0); }
        return v1;
    }
    v2 &= 0xF;
    uint16_t res = uint16_t((v1 << v2) | (v1 >> (16 - v2)));
    flags.C = (res & 1) != 0; flags.O = (((res & 1) ^ (res >> 15)) != 0);
    return res;
}
uint8_t VCPU::ROR_UInt8(uint8_t v1, uint8_t v2) {
    if ((v2 & 0x7) == 0) {
        if ((v2 & 0x18) != 0) { flags.C = (v1 >> 7) != 0; flags.O = (((v1 >> 7) ^ ((v1 >> 6) & 1)) != 0); }
        return v1;
    }
    v2 &= 0x7;
    uint8_t res = uint8_t((v1 >> v2) | (v1 << (8 - v2)));
    flags.C = (res & 0x80) != 0; flags.O = (((res ^ (res << 1)) & 0x80) != 0);
    return res;
}
uint16_t VCPU::ROR_UInt16(uint16_t v1, uint8_t v2) {
    if ((v2 & 0xF) == 0) {
        if ((v2 & 0x10) != 0) { flags.C = (v1 >> 15) != 0; flags.O = (((v1 >> 15) ^ ((v1 >> 14) & 1)) != 0); }
        return v1;
    }
    v2 &= 0xF;
    uint16_t res = uint16_t((v1 >> v2) | (v1 << (16 - v2)));
    flags.C = (res & 0x8000) != 0; flags.O = (((res ^ (res << 1)) & 0x8000) != 0);
    return res;
}
uint8_t VCPU::SAR_UInt8(uint8_t v1, uint8_t v2) {
    if (v2 == 0) return v1;
    if (v2 > 8) v2 = 8;
    uint8_t res = (v1 & 0x80) ? uint8_t((v1 >> v2) | (0xFF << (8 - v2))) : uint8_t(v1 >> v2);
    flags.C = (((v1) >> (v2 - 1)) & 1) != 0;
    flags.Z = (res == 0); flags.S = (res & 0x80) != 0; flags.O = false;
    return res;
}
uint16_t VCPU::SAR_UInt16(uint16_t v1, uint8_t v2) {
    if (v2 == 0) return v1;
    if (v2 > 16) v2 = 16;
    uint16_t res = (v1 & 0x8000) ? uint16_t((v1 >> v2) | (0xFFFF << (16 - v2))) : uint16_t(v1 >> v2);
    flags.C = (((v1) >> (v2 - 1)) & 1) != 0;
    flags.Z = (res == 0); flags.S = (res & 0x8000) != 0; flags.O = false;
    return res;
}
uint8_t VCPU::SBB_UInt8(uint8_t v1, uint8_t v2) {
    uint8_t cf = flags.C ? 1 : 0;
    uint8_t res = uint8_t(v1 - (v2 + cf));
    flags.C = (v1 < res) || (flags.C && v2 == 0xFF);
    flags.O = (((v1 ^ v2) & (v1 ^ res)) & 0x80) != 0;
    flags.S = (res & 0x80) != 0; flags.Z = (res == 0);
    return res;
}
uint16_t VCPU::SBB_UInt16(uint16_t v1, uint16_t v2) {
    uint16_t cf = flags.C ? 1 : 0;
    uint16_t res = uint16_t(v1 - (v2 + cf));
    flags.C = (v1 < res) || (flags.C && v2 == 0xFFFF);
    flags.O = (((v1 ^ v2) & (v1 ^ res)) & 0x8000) != 0;
    flags.S = (res & 0x8000) != 0; flags.Z = (res == 0);
    return res;
}
void VCPU::SCAS_UInt8() {
    CMP_UInt8(AX.lo(), ReadUInt8(ES.u16(), DI.u16()));
    if (flags.D) DI.decU16(); else DI.incU16();
}
void VCPU::REPNE_SCAS_UInt8() {
    while (CX.u16() != 0) {
        CMP_UInt8(AX.lo(), ReadUInt8(ES.u16(), DI.u16()));
        if (flags.D) DI.decU16(); else DI.incU16();
        CX.decU16();
        if (!flags.Z) break;
    }
}
void VCPU::CMPS_UInt8(Register& regES, Register& regDI, Register& sReg, Register& regSI) {
    CMP_UInt8(ReadUInt8(regES.u16(), regDI.u16()), ReadUInt8(sReg.u16(), regSI.u16()));
    if (flags.D) { SI.decU16(); DI.decU16(); } else { SI.incU16(); DI.incU16(); }
}
void VCPU::REPE_CMPS_UInt8(Register& regES, Register& regDI, Register& sReg, Register& regSI) {
    while (CX.u16() != 0) {
        CMPS_UInt8(regES, regDI, sReg, regSI);
        CX.decU16();
        if (flags.Z) break;
    }
}
uint8_t VCPU::SHL_UInt8(uint8_t v1, uint8_t v2) {
    if (v2 == 0) return v1;
    uint8_t res = uint8_t(v1 << v2);
    flags.C = (v2 > 8) ? false : (((v1 >> (8 - v2)) & 1) != 0);
    flags.O = ((res ^ v1) & 0x80) != 0;
    flags.S = (res & 0x80) != 0; flags.Z = (res == 0);
    return res;
}
uint16_t VCPU::SHL_UInt16(uint16_t v1, uint8_t v2) {
    if (v2 == 0) return v1;
    uint16_t res = uint16_t(v1 << v2);
    flags.C = (v2 > 16) ? false : (((v1 >> (16 - v2)) & 1) != 0);
    flags.O = ((res ^ v1) & 0x8000) != 0;
    flags.S = (res & 0x8000) != 0; flags.Z = (res == 0);
    return res;
}
uint16_t VCPU::SHLD1_UInt16(uint16_t v1, uint16_t v2, uint8_t v3) {
    if (v3 == 0) return v1;
    uint32_t res = (uint32_t(v1) << v3) | uint32_t(v2 & ((1u << v3) - 1));
    flags.C = (res & 0x10000) != 0;
    flags.S = (res & 0x8000) != 0;
    flags.Z = (res & 0xFFFF) == 0;
    return uint16_t(res & 0xFFFF);
}
uint8_t VCPU::SHR_UInt8(uint8_t v1, uint8_t v2) {
    if (v2 == 0) return v1;
    uint8_t res = uint8_t(v1 >> v2);
    flags.C = (v2 > 8) ? false : (((v1 >> (v2 - 1)) & 1) != 0);
    flags.O = ((v2 & 0x1F) == 1) ? (v1 > 0x80) : false;
    flags.S = (res & 0x80) != 0; flags.Z = (res == 0);
    return res;
}
uint16_t VCPU::SHR_UInt16(uint16_t v1, uint8_t v2) {
    if (v2 == 0) return v1;
    uint16_t res = uint16_t(v1 >> v2);
    flags.C = (v2 > 16) ? false : (((v1 >> (v2 - 1)) & 1) != 0);
    flags.O = ((v2 & 0x1F) == 1) ? (v1 > 0x8000) : false;
    flags.S = (res & 0x8000) != 0; flags.Z = (res == 0);
    return res;
}
void VCPU::STOS_UInt8() {
    WriteUInt8(ES.u16(), DI.u16(), AX.lo());
    if (flags.D) DI.decU16(); else DI.incU16();
}
void VCPU::REPE_STOS_UInt8() { while (CX.u16() != 0) { STOS_UInt8(); CX.decU16(); } }
void VCPU::STOS_UInt16() {
    WriteUInt16(ES.u16(), DI.u16(), AX.u16());
    if (flags.D) DI.u16(uint16_t(DI.u16() - 2)); else DI.u16(uint16_t(DI.u16() + 2));
}
void VCPU::REPE_STOS_UInt16() { while (CX.u16() != 0) { STOS_UInt16(); CX.decU16(); } }

uint8_t VCPU::SUB_UInt8(uint8_t v1, uint8_t v2) {
    uint8_t res = uint8_t(v1 - v2);
    flags.C = (v1 < v2);
    flags.O = (((v1 ^ v2) & (v1 ^ res)) & 0x80) != 0;
    flags.S = (res & 0x80) != 0; flags.Z = (res == 0);
    return res;
}
uint16_t VCPU::SUB_UInt16(uint16_t v1, uint16_t v2) {
    uint16_t res = uint16_t(v1 - v2);
    flags.C = (v1 < v2);
    flags.O = (((v1 ^ v2) & (v1 ^ res)) & 0x8000) != 0;
    flags.S = (res & 0x8000) != 0; flags.Z = (res == 0);
    return res;
}
void VCPU::TEST_UInt8(uint8_t v1, uint8_t v2) {
    uint8_t res = uint8_t(v1 & v2);
    flags.C = false; flags.O = false; flags.S = (res & 0x80) != 0; flags.Z = (res == 0);
}
void VCPU::TEST_UInt16(uint16_t v1, uint16_t v2) {
    uint16_t res = uint16_t(v1 & v2);
    flags.C = false; flags.O = false; flags.S = (res & 0x8000) != 0; flags.Z = (res == 0);
}
uint8_t VCPU::XLAT_UInt8(Register& sReg) {
    return ReadUInt8(sReg.u16(), uint16_t(BX.u16() + AX.lo()));
}
uint8_t VCPU::XOR_UInt8(uint8_t v1, uint8_t v2) {
    uint8_t res = uint8_t(v1 ^ v2);
    flags.C = false; flags.O = false; flags.S = (res & 0x80) != 0; flags.Z = (res == 0);
    return res;
}
uint16_t VCPU::XOR_UInt16(uint16_t v1, uint16_t v2) {
    uint16_t res = uint16_t(v1 ^ v2);
    flags.C = false; flags.O = false; flags.S = (res & 0x8000) != 0; flags.Z = (res == 0);
    return res;
}

} // namespace oc1
