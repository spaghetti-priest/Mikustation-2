/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>
#include "fmt-10.2.1/include/fmt/core.h"
#include "fmt-10.2.1/include/fmt/format-inl.h"
#include "fmt-10.2.1/include/fmt/format.h"
#include "fmt-10.2.1/src/format.cc"

#include "../include/ps2.h"
#include "../include/r5900Interpreter.h"
#include "../include/cop0.h"

std::array<TLB_Entry, 48> TLBS;

#define NDISASM 1
#ifdef NDISASM
#define syslog(fmt, ...) (void)0
#else
#define syslog(...) fmt::print(__VA_ARGS__ )
#endif

//@@Incomplete: Make this bold red text to indicate an error
#define errlog(...) fmt::print(__VA_ARGS__)

FILE *dis = fopen("disasm.txt", "w+");

//static u8 *_scratchpad_; 
static u8 *_icache_;
static u8 *_dcache_;

static u8 *_scratchpad_ = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));

// @@Refactor: Replace all of the instruction decoding in each function with these macros
#if 0
#define RD              (ee->current_instruction >> 11) & 0x1F
#define RT              (ee->current_instruction >> 16) & 0x1F
#define RS              (ee->current_instruction >> 21) & 0x1F
#define BASE            (ee->current_instruction >> 21) & 0x1F

#define IMM             (ee->current_instruction & 0xFFFF)
#define SIGN_IMM   (s16)(ee->current_instruction & 0xFFFF)
#define OFFSET          (ee->current_instruction & 0xFFFF)
#define SIGN_OFFSET (s16)(ee->current_instruction & 0xFFFF)
#endif

/*******************************************
 * Load Functions
*******************************************/
static inline u8 
ee_core_load_8 (u32 address) {
    uint32_t r;
    if (address >= 0x70000000 && address < 0x70004000)
        return *(uint8_t*)&_scratchpad_[address & 0x3FFF];
    if (address >= 0x30100000 && address < 0x31FFFFFF) // uncached and accelerated
        address -= 0x10000000;

    address &= 0x1FFFFFFF;
    
    return ee_load_8(address);
}

static inline u16 
ee_core_load_16 (u32 address) {
    uint32_t r;
    if (address >= 0x70000000 && address < 0x70004000)
        return *(uint16_t*)&_scratchpad_[address & 0x3FFF];
    if (address >= 0x30100000 && address < 0x31FFFFFF) // uncached and accelerated
        address -= 0x10000000;

    address &= 0x1FFFFFFF;
    
    return ee_load_16(address);

    return r;
}

static inline u32 
ee_core_load_32 (u32 address) {
    uint32_t r;
    if (address >= 0x70000000 && address < 0x70004000)
        return *(uint32_t*)&_scratchpad_[address & 0x3FFF];
    if (address >= 0x30100000 && address < 0x31FFFFFF) // uncached and accelerated
        address -= 0x10000000;

    address &= 0x1FFFFFFF;
    
    return ee_load_32(address);
}

static inline u64 
ee_core_load_64 (u32 address) {
       uint32_t r;
    if (address >= 0x70000000 && address < 0x70004000)
        return *(uint64_t*)&_scratchpad_[address & 0x3FFF];
    if (address >= 0x30100000 && address < 0x31FFFFFF) // uncached and accelerated
        address -= 0x10000000;

    address &= 0x1FFFFFFF;
    
    return ee_load_64(address);
}

static inline void ee_core_load_128() {return;}

/*******************************************
 * Store Functions
*******************************************/
static inline void 
ee_core_store_8 (u32 address, u8 value) {
      if (address >= 0x70000000 && address < 0x70004000) {
        *(uint8_t*)&_scratchpad_[address & 0x3FFF] = value;
        return;
    }

    address &= 0x1FFFFFFF;
    ee_store_8(address, value);
}

static inline void 
ee_core_store_16 (u32 address, u16 value) {
      if (address >= 0x70000000 && address < 0x70004000) {
        *(uint16_t*)&_scratchpad_[address & 0x3FFF] = value;
        return;
    }

    address &= 0x1FFFFFFF;
    ee_store_16(address, value);
}

static inline void 
ee_core_store_32 (u32 address, u32 value) {
      if (address >= 0x70000000 && address < 0x70004000) {
        *(uint32_t*)&_scratchpad_[address & 0x3FFF] = value;
        return;
    }

    address &= 0x1FFFFFFF;
    ee_store_32(address, value);
}

static inline void 
ee_core_store_64 (u32 address, u64 value) {
    if (address >= 0x70000000 && address < 0x70004000) {
        *(uint64_t*)&_scratchpad_[address & 0x3FFF] = value;
        return;
    }

    address &= 0x1FFFFFFF;
    ee_store_64(address, value);
}

static inline void ee_core_store_128() {return;}

/*******************************************
 * Load and Store Instructions
*******************************************/
// @@Note: Fuck everything I said previously. On the r5900load delay slots are optional
static void load_delay() {}

static void 
LUI (R5900_Core *ee, u32 instruction) 
{
    s64 imm = (s64)(s32)((instruction & 0xFFFF) << 16);
    u32 rt  = instruction >> 16 & 0x1F;
    ee->reg.r[rt].UD[0] = imm;

    syslog("LUI [{:d}] [{:#x}]\n", rt, imm); 
}

static void 
LB (R5900_Core *ee, u32 instruction) 
{
    u32 base        = instruction >> 21 & 0x1f;
    u32 rt          = instruction >> 16 & 0x1f;
    s32 offset      = (s16)(instruction & 0xFFFF);
    u32 vaddr       = ee->reg.r[base].UW[0] + offset;

    //ee->reg.r[rt].SD[0] = (s64)ee_core_load_memory<s8>(vaddr);
    ee->reg.r[rt].SD[0] = (s64)ee_core_load_8(vaddr);
    syslog("LB [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);  
}

static void 
LBU (R5900_Core *ee, u32 instruction) 
{
    u32 base        = instruction >> 21 & 0x1f;
    u32 rt          = instruction >> 16 & 0x1f;
    s16 offset      = (s16)(instruction & 0xFFFF);
    u32 vaddr       = ee->reg.r[base].UW[0] + offset;
    s32 low_bits    = vaddr & 0x3;
    //ee->reg.r[rt].UD[0] = (u64)ee_core_load_memory<u8>(vaddr);
    ee->reg.r[rt].UD[0] = (u64)ee_core_load_8(vaddr);
    
    syslog("LBU [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);    
}

static void
LH (R5900_Core *ee, u32 instruction) 
{
    s32 offset  = (s16)(instruction & 0xFFFF);
    u32 base    = instruction >> 21 & 0x1F;
    u32 rt      = instruction >> 16 & 0x1F;
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    u32 low_bit = vaddr & 0b1;
    if (low_bit != 0) {
        errlog("[ERROR] Vaddr is not properly aligned %#x \n", vaddr);
        Exception exc = get_exception(V_COMMON, __ADDRESS_ERROR);
        handle_exception_level_1(ee, &exc);
    }
    ee->reg.r[rt].SD[0] = (s64)ee_core_load_16(vaddr);
    syslog("LH [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);    
}

static void
LHU (R5900_Core *ee, u32 instruction)
{
    s32 offset  = (s16)(instruction & 0xFFFF);
    u32 rt      = (instruction >> 16) & 0x1F;
    u32 base    = (instruction >> 21) & 0x1F;
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    u32 low_bit = vaddr & 0x1;

    if (low_bit != 0) {
        errlog("[ERROR] Vaddr is not properly aligned %#x \n", vaddr);
        Exception exc = get_exception(V_COMMON, __ADDRESS_ERROR);
        handle_exception_level_1(ee, &exc);
    }
    ee->reg.r[rt].UD[0] = (u64)ee_core_load_16(vaddr);
    syslog("LHU [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
}

static void 
LW (R5900_Core *ee, u32 instruction) 
{
    s32 offset  = (s16)(instruction & 0xFFFF);
    u32 base    = instruction >> 21 & 0x1F;
    u32 rt      = instruction >> 16 & 0x1F;
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    u32 low_bits = vaddr & 0x3;
    if (low_bits != 0) {
        errlog("[ERROR] Vaddr is not properly aligned %#x \n", vaddr);
        Exception exc = get_exception(V_COMMON, __ADDRESS_ERROR);
        handle_exception_level_1(ee, &exc);
    }
    ee->reg.r[rt].SD[0] = (s64)ee_core_load_32(vaddr);
    syslog("LW [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
}

static void 
LD (R5900_Core *ee, u32 instruction) 
{
    u32 base    = instruction >> 21 & 0x1f;
    u32 rt      = instruction >> 16 & 0x1f;
    s16 offset  = (s16)(instruction & 0xFFFF);
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    u32 low_bit = vaddr & 0x3;
    if (low_bit != 0) {
        syslog("LD [{:d}] [{:#x}] [{:d}]\n", rt, offset, base);
        errlog("[ERROR] Vaddr is not properly aligned %#x \n", vaddr);
        Exception exc = get_exception(V_COMMON, __ADDRESS_ERROR);
        handle_exception_level_1(ee, &exc);
        //return;
    }
    ee->reg.r[rt].UD[0] = ee_core_load_64(vaddr);
    syslog("LD [{:d}] [{:#x}] [{:d}]\n", rt, offset, base);
}

static void 
LQ (R5900_Core *ee, u32 instruction) 
{
    u32 base    = instruction >> 21 & 0x1f;
    u32 rt      = instruction >> 16 & 0x1f;
    s16 offset  = (s16)(instruction & 0xFFFF);
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    u32 low_bit = vaddr & 0x3;
    if (low_bit != 0) {
        syslog("LQ [{:d}] [{:#x}] [{:d}]\n", rt, offset, base);
        errlog("[ERROR] Vaddr is not properly aligned %#x \n", vaddr);
        Exception exc = get_exception(V_COMMON, __ADDRESS_ERROR);
        handle_exception_level_1(ee, &exc);
    }
    ee->reg.r[rt].UD[0] = ee_core_load_64(vaddr);
    ee->reg.r[rt].UD[1] = ee_core_load_64(vaddr + 8);
    syslog("LQ [{:d}] [{:#x}] [{:d}]\n", rt, offset, base);
}

static void 
SB (R5900_Core *ee, u32 instruction) 
{
    s32 offset  = (s16)(instruction & 0xFFFF);
    u32 rt      = (instruction >> 16) & 0x1F;
    u32 base    = (instruction >> 21) & 0x1F;
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    s8 value    = ee->reg.r[rt].SB[0];

    ee_core_store_8(vaddr, value);
    syslog("SB [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
}

static void 
SH (R5900_Core *ee, u32 instruction) 
{
    s32 offset      = (s16)(instruction & 0xFFFF);
    u32 rt          = (instruction >> 16) & 0x1F;
    u32 base        = (instruction >> 21) & 0x1F;
    u32 vaddr       = ee->reg.r[base].UW[0] + offset;
    u32 low_bits    = vaddr & 0x1;
    s16 value       = ee->reg.r[rt].SH[0];

    if ( low_bits != 0 ) {
        syslog("SH [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
        errlog("[ERROR]: Vaddr is not properly halfword aligned%#x \n", vaddr);
        Exception exc = get_exception(V_COMMON, __ADDRESS_ERROR);
        handle_exception_level_1(ee, &exc);
    }

    ee_core_store_16(vaddr, value);
    syslog("SH [{:d}] [{:#x}] [{:d}] \n", rt, offset, base); 
}

static void 
SW (R5900_Core *ee, u32 instruction) 
{
    s16 offset  = (s16)(instruction & 0xFFFF);
    u32 base    = instruction >> 21 & 0x1F;
    u32 rt      = instruction >> 16 & 0x1F;
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    s32 value   = ee->reg.r[rt].SW[0];
    u32 low_bits = vaddr & 0x3;
    if (low_bits != 0) {
        syslog("SW [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
        errlog("[ERROR] Vaddr is not properly aligned %#x \n", vaddr);
        Exception exc = get_exception(V_COMMON, __ADDRESS_ERROR);
        handle_exception_level_1(ee, &exc);
    }

    ee_core_store_32(vaddr, value);
    syslog("SW [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
}

static void 
SD (R5900_Core *ee, u32 instruction) 
{
    s16 offset  = (s16)(instruction & 0xFFFF);
    u32 rt      = instruction >> 16 & 0x1F;
    u32 base    = instruction >> 21 & 0x1f;
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    u64 value   = ee->reg.r[rt].UD[0];
    u32 low_bits = vaddr & 0x3;
    if (low_bits != 0) {
        syslog("SD [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
        errlog("[ERROR] Vaddr is not properly aligned %#x \n", vaddr);
        Exception exc = get_exception(V_COMMON, __ADDRESS_ERROR);
        handle_exception_level_1(ee, &exc);
    }

    ee_core_store_64(vaddr, value);   
    syslog("SD [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
}

static void 
SQ (R5900_Core *ee, u32 instruction) 
{
    s16 offset  = (s16)(instruction & 0xFFFF);
    u32 rt      = instruction >> 16 & 0x1F;
    u32 base    = instruction >> 21 & 0x1f;
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    u64 lov     = ee->reg.r[rt].UD[0];
    u64 hiv     = ee->reg.r[rt].UD[1];
    u32 low_bits = vaddr & 0x3;
    if (low_bits != 0) {
        syslog("SQ [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
        errlog("[ERROR] Vaddr is not properly aligned %#x \n", vaddr);
        Exception exc = get_exception(V_COMMON, __ADDRESS_ERROR);
        handle_exception_level_1(ee, &exc);
    }

    ee_core_store_64(vaddr, lov);       
    ee_core_store_64(vaddr + 8, hiv);       
    syslog("SQ [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
}

static void 
SWC1 (R5900_Core *ee, u32 instruction) 
{
    u32 base    = instruction >> 21 & 0x1f;
    u32 ft      = instruction >> 16 & 0x1f;
    s16 offset  = (s16)(instruction & 0xFFFF);
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    u32 low_bits = vaddr & 0x3;
    if (low_bits != 0) {
        syslog("SWC1 [{:d}] [{:#x}] [{:d}] \n", ft, offset, base);
        errlog("[ERROR] Vaddr is not properly aligned %#x \n", vaddr);
        Exception exc = get_exception(V_COMMON, __ADDRESS_ERROR);
        handle_exception_level_1(ee, &exc);
    }

    ee_core_store_32(vaddr, ee->cop1.fprs[ft]);
    syslog("SWC1 [{:d}] [{:#x}] [{:d}] \n", ft, offset, base); 
}

/*******************************************
* Computational Instructions
* Arithmetic
• Logical
• Shift
• Multiply
• Divide
*******************************************/
static void add_overflow() {return;}
static void add_overflow64() {return;}

static void 
ADD (R5900_Core *ee, u32 instruction) 
{
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    u32 rs = instruction >> 21 & 0x1F;

    // @@Incomplete: The add instruction must signal an exception on overflow
    // but we have no overflow detection for now
    int temp = ee->reg.r[rs].SW[0] + ee->reg.r[rt].SW[0];
    ee->reg.r[rd].SD[0] = (s64)temp;
    syslog("ADD [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);   
}

static void 
ADDU (R5900_Core *ee, u32 instruction) 
{
    u32 rd = (instruction >> 11) & 0x1F;
    u32 rt = (instruction >> 16) & 0x1F;
    u32 rs = (instruction >> 21) & 0x1F;

    int32_t result = ee->reg.r[rs].SW[0] + ee->reg.r[rt].SW[0];
    ee->reg.r[rd].UD[0] = result;
    syslog("ADDU [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);
}

static void 
ADDIU (R5900_Core *ee, u32 instruction) 
{
    s16 imm = (s16)(instruction & 0xFFFF);
    u32 rs  = instruction >> 21 & 0x1f;
    u32 rt  = instruction >> 16 & 0x1f;
    s32 result = ee->reg.r[rs].SD[0] + imm;
    ee->reg.r[rt].SD[0] = result;
    syslog("ADDIU: [{:d}] [{:d}] [{:#x}] \n", rt, rs, imm);
}

static void 
DADDU (R5900_Core *ee, u32 instruction) 
{
    u32 rs = instruction >> 21 & 0x1f;
    u32 rt = instruction >> 16 & 0x1f;
    u32 rd = instruction >> 11 & 0x1f;
    ee->reg.r[rd].UD[0] = ee->reg.r[rs].SD[0] + ee->reg.r[rt].SD[0];

    syslog("DADDU [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);  
}

static void
DADDIU (R5900_Core *ee, u32 instruction)
{
    s16 imm = (s16)(instruction & 0xFFFF);
    u32 rs  = instruction >> 21 & 0x1F;
    u32 rt  = instruction >> 16 & 0x1F;

    //ee->reg.r[rt].UD[0] = ee->reg.r[rs].SD[0] + imm;
    ee->reg.r[rt].UD[0] = ee->reg.r[rs].SD[0] + imm;
    syslog("DADDIU [{:d}] [{:d}] [{:#x}]\n", rt, rs, imm);  
}

static void 
SUBU (R5900_Core *ee, u32 instruction) 
{
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    u32 rs = instruction >> 21 & 0x1F;
    s32 result = ee->reg.r[rs].SW[0] - ee->reg.r[rt].SW[0];
    ee->reg.r[rd].UD[0] = (u64)result;
    syslog("SUBU [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);    
}

static void 
MULT (R5900_Core *ee, u32 instruction) 
{
    u32 rs = instruction >> 21 & 0x1f;
    u32 rt = instruction >> 16 & 0x1f;
    u32 rd = instruction >> 11 & 0x1f;

    s64 w1 = (s64)ee->reg.r[rs].SW[0];
    s64 w2 = (s64)ee->reg.r[rt].SW[0];
    s64 prod = (w1 * w2);
    
    ee->LO = (s32)(prod & 0xFFFFFFFF);
    ee->HI = (s32)(prod >> 32);
    // @@Note: C790 apparently states that the chip does not always call mflo everytime
    // during a 3 operand mult
    // But we could always use mflo because of the pipeline interlock 
    ee->reg.r[rd].SD[0] = ee->LO;

    syslog("MULT [{:d}] [{:d}] [{:d}]\n", rs, rt, rd);
}

static void
MULT1 (R5900_Core *ee, u32 instruction)
{
    u32 rs = instruction >> 21 & 0x1f;
    u32 rt = instruction >> 16 & 0x1f;
    u32 rd = instruction >> 11 & 0x1f;

    s32 w1 = (s32)ee->reg.r[rs].SW[0];
    s32 w2 = (s32)ee->reg.r[rt].SW[0];
    s64 prod = (s64)(w1 * w2);
    
    ee->LO1 = (s32)(prod & 0xFFFFFFFF);
    ee->HI1 = (s32)(prod >> 32);
    // @@Note: C790 apparently states that the chip does not always call mflo everytime
    // during a 3-operand mult
    // But we could always use mflo because of the pipeline interlock 
    ee->reg.r[rd].SD[0] = ee->LO1;

    syslog("MULT1 [{:d}] [{:d}] [{:d}]\n", rs, rt, rd);
}

static void 
MADDU (R5900_Core *ee, u32 instruction)
{
    errlog("LOL\n");
    syslog("MADDU\n");
    printf("lol");
}

static void
DIV (R5900_Core *ee, u32 instruction)
{
    u32 rs = (instruction >> 21) & 0x1F;
    u32 rt = (instruction >> 16) & 0x1F;

    if (ee->reg.r[rt].UD == 0) {
        ee->LO = (int)0xffffffff;
        ee->HI = ee->reg.r[rs].UD[0];
        errlog("[ERROR]: Tried to Divide by zero\n");
        //return;
    }
    s32 d = ee->reg.r[rs].SW[0] / ee->reg.r[rt].SW[0];
    s32 q = ee->reg.r[rs].SW[0] % ee->reg.r[rt].SW[0];

    ee->LO = (s64)d;
    ee->HI = (s64)q;
    syslog("DIV [{:d}] [{:d}]\n", rs, rt);
}

static void 
DIVU (R5900_Core *ee, u32 instruction) 
{
    u32 rs = instruction >> 21 & 0x1f;
    u32 rt = instruction >> 16 & 0x1f;
    s64 w1 = (s64)ee->reg.r[rs].SW[0];
    s64 w2 = (s64)ee->reg.r[rt].SW[0];

    if (w2 == 0) {
        errlog("[ERROR] tried to divide by 0, result is Undefined\n");
        //return;
    } 
    // @@Note: Sign extend by 64?
    s64 q = (s32)(w1 / w2);
    ee->LO = q;
    s64 r = (s32)(w1 % w2);
    ee->HI = r;

    syslog("DIVU [{:d}] [{:d}]\n", rs, rt);
}

static void
DIVU1 (R5900_Core *ee, u32 instruction)
{
    u32 rs = instruction >> 21 & 0x1f;
    u32 rt = instruction >> 16 & 0x1f;
    s32 w1 = ee->reg.r[rs].SW[0];
    s32 w2 = ee->reg.r[rt].SW[0];

    if (w2 == 0) {
        ee->LO1 = (int)0xffffffff;
        ee->HI1 = ee->reg.r[rs].UD[0];
        errlog("[ERROR] tried to divide by 0, result is Undefined\n");
        //exit(1);
        //return;
    } 

    s64 q  = (w1 / w2);
    ee->LO1 = q;
    s64 r  = (w1 % w2);
    ee->HI1 = r;

    syslog("DIVU_1 [{:d}] [{:d}]\n", rs, rt);
}

static void 
AND (R5900_Core *ee, u32 instruction) 
{
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    u32 rs = instruction >> 21 & 0x1F;
    ee->reg.r[rd].UD[0] = ee->reg.r[rs].UD[0] & ee->reg.r[rt].UD[0];
    syslog("AND [{:d}] [{:d}] [{:d}] \n", rd, rs, rt);
}

static void 
ANDI (R5900_Core *ee, u32 instruction) 
{
    u64 imm = (u16)(instruction & 0xFFFF);
    u32 rs = instruction >> 21 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    ee->reg.r[rt].UD[0] = ee->reg.r[rs].UD[0] & imm; 

    syslog("ANDI: [{:d}] [{:d}] [{:#x}] \n", rt, rs, imm);
}

static void 
OR (R5900_Core *ee, u32 instruction) 
{
    u32 rs = instruction >> 21 & 0x1f;
    u32 rt = instruction >> 16 & 0x1f;
    u32 rd = instruction >> 11 & 0x1f;
    ee->reg.r[rd].SD[0] = ee->reg.r[rs].SD[0] | ee->reg.r[rt].SD[0];

    syslog("OR [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);
}

static void 
ORI (R5900_Core *ee, u32 instruction) 
{
    u64 imm = (instruction & 0xFFFF); 
    u32 rs  = instruction >> 21 & 0x1F;
    u32 rt  = instruction >> 16 & 0x1F;          
    ee->reg.r[rt].UD[0] = ee->reg.r[rs].UD[0] | imm; 

    syslog("ORI: [{:d}] [{:d}] [{:#x}] \n", rt, rs, imm);
}

static void
XORI (R5900_Core *ee, u32 instruction)
{
    u64 imm = (u16)(instruction & 0xFFFF);
    u32 rs  = instruction >> 21 & 0x1F;
    u32 rt  = instruction >> 16 & 0x1F;

    ee->reg.r[rt].UD[0] = ee->reg.r[rs].UD[0] ^ imm;
    syslog("XORI [{:d}] [{:d}] [{:#x}] \n", rt, rs, imm);
}

static void 
NOR (R5900_Core *ee, u32 instruction) 
{
    u32 rs = instruction >> 21 & 0x1f;
    u32 rt = instruction >> 16 & 0x1f;
    u32 rd = instruction >> 11 & 0x1f;
    ee->reg.r[rd].SD[0] = ~(ee->reg.r[rs].SD[0] | ee->reg.r[rt].SD[0]);
    assert(1);
    syslog("NOR [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);
}

static void 
SLL (R5900_Core *ee, u32 instruction) 
{
    u32 sa = instruction >> 6 & 0x1F;
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    ee->reg.r[rd].SD[0] = (u64)((s32)ee->reg.r[rt].UW[0] << sa);

    syslog("SLL source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, sa);
}

static void 
SLLV (R5900_Core *ee, u32 instruction) 
{
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    u32 rs = instruction >> 21 & 0x1F;
    u32 sa = ee->reg.r[rs].SW[0] & 0x1F;

    ee->reg.r[rd].SD[0] = (s64)(ee->reg.r[rt].SW[0] << sa);

    syslog("SLLV source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, sa);
}

static void 
DSLL (R5900_Core *ee, u32 instruction) 
{
    u32 sa = instruction >> 6 & 0x1F;
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    ee->reg.r[rd].UD[0] = ee->reg.r[rt].UD[0] << sa;
    syslog("DSLL source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, sa);
}

static void
DSLLV (R5900_Core *ee, u32 instruction)
{
    u32 rs = (instruction >> 21) & 0x1F;
    u32 rt = (instruction >> 16) & 0x1F;
    u32 rd = (instruction >> 11) & 0x1F;

    //s32 sa = ee->reg.r[rs].UW[0] & 0x3F;
    u32 sa = ee->reg.r[rs].UW[0] & 0x3F;
    ee->reg.r[rd].UD[0] = ee->reg.r[rt].UD[0] << sa;
    syslog("DSLLV [{:d}] [{:d}] [{:d}]\n", rd, rt, rs); 
}

static void 
SRL (R5900_Core *ee, u32 instruction) 
{
    u32 sa = instruction >> 6 & 0x1F;
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    u32 result = (ee->reg.r[rt].UW[0] >> sa);
    ee->reg.r[rd].SD[0] = (s32)result;

    syslog("SRL source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, sa);
}

static void 
SRA (R5900_Core *ee, u32 instruction) 
{
    s32 sa      = instruction >> 6 & 0x1F;
    u32 rd      = instruction >> 11 & 0x1F;
    u32 rt      = instruction >> 16 & 0x1F;
    s32 result  = (ee->reg.r[rt].SW[0]) >> sa;
    ee->reg.r[rd].SD[0] = result;

    syslog("SRA source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, sa);
}

static void
SRAV (R5900_Core *ee, u32 instruction)
{
    u32 rs = (instruction >> 21) & 0x1F;
    u32 rt = (instruction >> 16) & 0x1F;
    u32 rd = (instruction >> 11) & 0x1F;
    u32 sa = ee->reg.r[rs].UB[0] & 0x1F;
    s32 result = ee->reg.r[rt].SW[0] >> sa;
    ee->reg.r[rd].SD[0] = (s64)result;

    syslog("SRAV [{:d}] [{:d}] [{:d}]\n", rd, rt, rs);
}

static void 
DSRAV (R5900_Core *ee, u32 instruction)
{
    u32 rs = (instruction >> 21) & 0x1F;
    u32 rt = (instruction >> 16) & 0x1F;
    u32 rd = (instruction >> 11) & 0x1F;

    u32 sa = ee->reg.r[rs].UW[0] & 0x3F;
    ee->reg.r[rd].SD[0] = ee->reg.r[rt].SD[0] >> sa;
    syslog("DSRAV [{:d}] [{:d}] [{:d}]\n", rd, rt, rs);
}

static void 
DSRA32 (R5900_Core *ee, u32 instruction) 
{
    u32 sa  = instruction >> 6 & 0x1F;
    u32 rd  = instruction >> 11 & 0x1F;
    u32 rt  = instruction >> 16 & 0x1F;
    u32 s   = sa + 32;

    int64_t result = ee->reg.r[rt].SD[0] >> s;
    ee->reg.r[rd].SD[0] = result;
    syslog("DSRA32 source: [{:d}] dest: [{:d}] s: [{:#x}] \n", rd, rt, s);
}

static void 
DSLL32 (R5900_Core *ee, u32 instruction) 
{
    u32 sa  = instruction >> 6 & 0x1F;
    u32 rd  = instruction >> 11 & 0x1F;
    u32 rt  = instruction >> 16 & 0x1F;
    u32 s   = sa + 32;

    ee->reg.r[rd].UD[0] = ee->reg.r[rt].UD[0] << s;

    syslog("DSLL32 source: [{:d}] dest: [{:d}] s: [{:#x}] \n", rd, rt, s);
}

static void 
SLT (R5900_Core *ee, u32 instruction) 
{
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    u32 rs = instruction >> 21 & 0x1F;

    int result = ee->reg.r[rs].SD[0] < ee->reg.r[rt].SD[0] ? 1 : 0;
    ee->reg.r[rd].SD[0] = result;
    syslog("SLT [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);
}

static void 
SLTU (R5900_Core *ee, u32 instruction) 
{
    u32 rs = (instruction >> 21) & 0x1F;
    u32 rt = (instruction >> 16) & 0x1F;
    u32 rd = (instruction >> 11) & 0x1F;
    uint32_t result = (ee->reg.r[rs].UD[0] < ee->reg.r[rt].UD[0]) ? 1 : 0;
    ee->reg.r[rd].UD[0] = result;
    syslog("SLTU [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);
}

static void 
SLTI (R5900_Core *ee, u32 instruction) 
{
    s16 imm = (s16)(instruction & 0xFFFF);
    u32 rt  = instruction >> 16 & 0x1F;
    u32 rs  = instruction >> 21 & 0x1F;

    int result = (ee->reg.r[rs].SD[0] < (s32)imm) ? 1 : 0;
    ee->reg.r[rt].SD[0] = result;
    syslog("SLTI [{:d}] [{:d}], [{:#x}]\n", rt, rs, imm);
}

static void 
SLTIU (R5900_Core *ee, u32 instruction) 
{
    u64 imm = (s16)(instruction & 0xFFFF);
    u32 rs = instruction >> 21 & 0x1f;
    u32 rt = instruction >> 16 & 0x1f;

    u32 result = (ee->reg.r[rs].UD[0] < imm) ? 1 : 0;
    ee->reg.r[rt].UD[0] = result;
    syslog("SLTIU [{:d}] [{:d}] [{:#x}]\n", rt, rs, imm);
}

/*******************************************
 * Jump and Branch Instructions
*******************************************/
static inline void 
jump_to (R5900_Core *ee, s32 address) 
{
    ee->branch_pc    = address;
    ee->is_branching = true;
    ee->delay_slot   = 1;
}

static inline void 
branch (R5900_Core *ee, bool is_branching, u32 offset) 
{
    if (is_branching) {
        ee->is_branching = true;
        ee->branch_pc    = ee->pc + 4 + offset;
        ee->delay_slot   = 1;
    }
}

static inline void 
branch_likely (R5900_Core *ee, bool is_branching, u32 offset) 
{
    if (is_branching) {
        ee->is_branching = true;
        ee->branch_pc    = ee->pc + 4 + offset;
        ee->delay_slot   = 1;
    } else {
        ee->pc = ee->pc + 4;
    }
}

static void 
J (R5900_Core *ee, u32 instruction) 
{
    u32 instr_index = (instruction & 0x3FFFFFF);
    u32 offset      = ((ee->pc + 4) & 0xF0000000) + (instr_index << 2);
    jump_to(ee, offset);
    syslog("J [{:#x}]\n", offset);
}

static void 
JR(R5900_Core *ee, u32 instruction) 
{
    u32 source = instruction >> 21 & 0x1F;
    //@Temporary: check the 2 lsb if 0
    jump_to(ee, ee->reg.r[source].UW[0]);

    syslog("JR source: [{:d}] pc_dest: [{:#x}]\n", source, ee->reg.r[source].UW[0]);
}

static void 
JAL(R5900_Core *ee, u32 instruction) 
{
    u32 instr_index     = (instruction & 0x3FFFFFF);
    u32 target_address  = ((ee->pc + 4) & 0xF0000000) + (instr_index << 2);
    jump_to(ee, target_address); 
    ee->reg.r[31].UD[0]  = ee->pc + 8;

    syslog("JAL [{:#x}] \n", target_address);   
}

static void 
JALR(R5900_Core *ee, u32 instruction) 
{
    u32 rs = instruction >> 21 & 0x1f;
    u32 rd = instruction >> 11 & 0x1f;
    u32 return_addr = ee->pc + 8;
    if (rd != 0) {
        ee->reg.r[rd].UD[0] = return_addr;
    } else {
        ee->reg.r[31].UD[0] = return_addr;
    }
    jump_to(ee, ee->reg.r[rs].UW[0]);
    syslog("JALR [{:d}]\n", rs);
}

static void 
BNE(R5900_Core *ee, u32 instruction) 
{
    s32 imm     = (s16)(instruction & 0xFFFF);
    imm         = (imm << 2);
    u32 rs      = instruction >> 21 & 0x1F;
    u32 rt      = instruction >> 16 & 0x1F;

    bool condition = ee->reg.r[rs].SD[0] != ee->reg.r[rt].SD[0];
    branch(ee, condition, imm);

    syslog("BNE [{:d}] [{:d}], [{:#x}]\n", rt, rs, imm);
}

static void 
BEQ(R5900_Core *ee, u32 instruction) 
{
    s16 imm = (s16)(instruction & 0xFFFF);
    imm     = imm << 2;
    u32 rs  = instruction >> 21 & 0x1f;
    u32 rt  = instruction >> 16 & 0x1f;

    bool condition = ee->reg.r[rs].SD[0] == ee->reg.r[rt].SD[0];
    branch(ee, condition, imm);

    syslog("BEQ [{:d}] [{:d}] [{:#x}] \n", rs, rt, imm);
}

static void 
BEQL(R5900_Core *ee, u32 instruction) 
{
    u32 rs  = instruction >> 21 & 0x1f;
    u32 rt  = instruction >> 16 & 0x1f;
    int imm = (s16)(instruction & 0xFFFF);
    imm     = imm << 2;

    bool condition = ee->reg.r[rs].SD[0] == ee->reg.r[rt].SD[0];
    branch_likely(ee, condition, imm);

    syslog("BEQL [{:d}] [{:d}] [{:#x}]\n", rs, rt, imm);    
}

static void 
BNEL(R5900_Core *ee, u32 instruction) 
{
    u32 rs  = instruction >> 21 & 0x1f;
    u32 rt  = instruction >> 16 & 0x1f;
    int imm = (s16)(instruction & 0xFFFF);
    imm     = imm << 2;

    bool condition = ee->reg.r[rs].SD[0] != ee->reg.r[rt].SD[0];
    branch_likely(ee, condition, imm);

    syslog("BNEL [{:d}] [{:d}] [{:#x}]\n", rs, rt, imm);
}

static void 
BLEZ (R5900_Core *ee, u32 instruction) 
{
    s16 offset  = (s16)((instruction & 0xFFFF) << 2);
    u32 rs      = instruction >> 21 & 0x1F;

    bool condition = ee->reg.r[rs].SD[0] <= 0;
    branch(ee, condition, offset);
    syslog("BLEZ [{:d}] [{:#x}]\n", rs, offset); 
}

static void 
BLTZ (R5900_Core *ee, u32 instruction) 
{
    s32 offset  = (s16)(instruction & 0xFFFF) << 2;
    u32 rs      = instruction >> 21 & 0x1f;

    bool condition = ee->reg.r[rs].SD[0] < 0;
    branch(ee, condition, offset);
    syslog("BLTZ [{:d}] [{:#x}] \n", rs, offset);
}

static void
BGTZ (R5900_Core *ee, u32 instruction)
{
    s32 offset  = (s16)(instruction & 0xFFFF) << 2;
    u32 rs      = instruction >> 21 & 0x1F;

    bool condition = ee->reg.r[rs].SD[0] > 0;
    branch(ee, condition, offset);
    syslog("BGTZ [{:d}] [{:#x}] \n", rs, offset);
}

static void 
BGEZ (R5900_Core *ee, u32 instruction) 
{
    s32 offset  = (s16)(instruction & 0xFFFF) << 2;
    u32 rs      = instruction >> 21 & 0x1f;

    bool condition = ee->reg.r[rs].SD[0] >= 0;
    branch(ee, condition, offset);
    syslog("BGEZ [{:d}] [{:#x}] \n", rs, offset);    
}

/*******************************************
 * Miscellaneous Instructions
*******************************************/
// @@Implementation @@Debug: We have no debugger yet so theres no need to implement this now
static void 
BREAKPOINT(R5900_Core *ee, u32 instruction) 
{
    syslog("BREAKPOINT\n");
}

/*******************************************
 * SYNCHRONOUS Instructions
*******************************************/
// @@Note: Issued after every MTC0 write The PS2 possibly executes millions of memory loads / stores per second, 
// which is entirely feasible in hardware. But a complete pain in software as that would completely kill performance
// May be cool to implement this for accuracy since that is the purpose of the emulator
static void 
SYNC(R5900_Core *ee, u32 instruction) 
{
    syslog("SYNC\n");
}

/*******************************************
 * COP0 Instructions
*******************************************/
static void 
MFC0 (R5900_Core *ee, u32 instruction) 
{
    u32 cop0            = instruction >> 11 & 0x1F;
    u32 gpr             = instruction >> 16 & 0x1F;
    ee->reg.r[gpr].SD[0] = ee->cop0.regs[cop0];

    syslog("MFC0 GPR: [{:d}] COP0: [{:d}]\n", gpr, cop0);
}

static void 
MTC0 (R5900_Core *ee, u32 instruction) 
{
    u32 cop0            = instruction >> 11 & 0x1F;
    u32 gpr             = instruction >> 16 & 0x1F;
    ee->cop0.regs[cop0]  = ee->reg.r[gpr].SW[0];

    syslog("MTC0 GPR: [{:d}] COP0: [{:d}]\n", gpr, cop0);
}

// @@Implemtation: implement TLB
static void 
TLBWI (R5900_Core *ee, u32 instruction, int index) 
{
    //TLB_Entry current = TLBS.at(index);
    syslog("TLBWI not yet implemented\n");
}

static void 
CACHE_IXIN (R5900_Core *ee, u32 instruction) 
{
    syslog("Invalidate instruction cache");
}

static void
SYSCALL (R5900_Core *ee, u32 instruction)
{
    printf("Hello we call syscall here lol\n");
    Exception exc = get_exception(V_COMMON, __SYSCALL);
    handle_exception_level_1(ee, &exc);
}

/*******************************************
 * COP1 Instructions
*******************************************/
/*******************************************
 * EE Instructions
*******************************************/
static void 
MOVN (R5900_Core *ee, u32 instruction)
{
    u32 rs = (instruction >> 21) & 0x1F;
    u32 rt = (instruction >> 16) & 0x1F;
    u32 rd = (instruction >> 11) & 0x1F;

    if (ee->reg.r[rt].UD[0] != 0) {
        ee->reg.r[rd].UD[0] = ee->reg.r[rs].UD[0];
    }
    syslog("MOVN [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);
}

static void 
MOVZ (R5900_Core *ee, u32 instruction) 
{
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    u32 rs = instruction >> 21 & 0x1F;
    if (ee->reg.r[rt].SD[0] == 0) {
        ee->reg.r[rd].SD[0] = ee->reg.r[rs].SD[0];
    }
    syslog("MOVZ [{:d}] [{:d}] [{:d}]\n",rd, rs, rt);
}

static void 
MFLO (R5900_Core *ee, u32 instruction) 
{
    u32 rd = instruction >> 11 & 0x1f;
    ee->reg.r[rd].UD[0] = ee->LO;

    syslog("MFLO [{:d}] \n", rd);
}

static void 
MFLO1 (R5900_Core *ee, u32 instruction)
{
    u32 rd = (instruction >> 11) & 0x1F;
    ee->reg.r[rd].UD[0] = ee->LO1;
    syslog("MFLO_1 [{:d}]\n", rd);
}

static void 
MFHI (R5900_Core *ee, u32 instruction) 
{
    u32 rd = instruction >> 11 & 0x1F;
    ee->reg.r[rd].UD[0] = ee->HI;
    syslog("MFHI [{:d}]\n", rd);
}

static void
MTLO (R5900_Core *ee, u32 instruction) 
{
    u32 rs = instruction >> 21 & 0x1F;
    ee->LO = ee->reg.r[rs].UD[0];
    syslog("MTLO [{:d}]\n", rs);
}

static void
MTHI (R5900_Core *ee, u32 instruction) 
{
    u32 rs = instruction >> 21 & 0x1F;
    ee->HI = ee->reg.r[rs].UD[0];
    syslog("MTHI [{:d}]\n", rs);
}

void 
decode_and_execute (R5900_Core *ee, u32 instruction)
{
    if (instruction == 0x00000000) return;
    int opcode = instruction >> 26;
    //syslog("instruction: {:#09x} || pc: {:#09x}\n", instruction, ee.pc);
    switch (opcode) {
        case COP0:
        {
            int fmt = instruction >> 21 & 0x3F;
            switch (fmt) 
            {
                case 0x000000: MFC0(ee, instruction);  break;
                case 0x000004: MTC0(ee, instruction);  break;
                case 0x000010: 
                {
                    int index = ee->cop0.regs[0];
                    TLBWI(ee, instruction, index);
                } break;

                default:
                {
                    errlog("[ERROR]: Could not interpret COP0 instruction format opcode [{:#09x}]\n", fmt);
                } break;
            }
        } break;

        case SPECIAL:
        {
            u32 special = instruction & 0x3F;
            switch(special) 
            {
                case 0x00000000:    SLL(ee, instruction);    break;
                case 0x0000003f:    DSRA32(ee, instruction); break;
                case 0x00000008:    JR(ee, instruction);     break;
                case 0x0000000f:    SYNC(ee, instruction);   break;
                case 0x00000009:    JALR(ee, instruction);   break;
                case 0x0000002d:    DADDU(ee, instruction);  break;
                case 0x00000025:    OR(ee, instruction);     break;
                case 0x00000018:    MULT(ee, instruction);   break;
                case 0x0000001B:    DIVU(ee, instruction);   break;
                case 0x0000000D:    BREAKPOINT(ee, instruction);break;
                case 0x00000012:    MFLO(ee, instruction);   break;
                case 0x0000002B:    SLTU(ee, instruction);   break;
                case 0x00000038:    DSLL(ee, instruction);   break;
                case 0x0000003C:    DSLL32(ee, instruction); break;
                case 0x00000024:    AND(ee, instruction);    break;
                case 0x00000002:    SRL(ee, instruction);    break;
                case 0x00000003:    SRA(ee, instruction);    break;
                case 0x000000A:     MOVZ(ee, instruction);   break;
                case 0x0000023:     SUBU(ee, instruction);   break;
                case 0x0000010:     MFHI(ee, instruction);   break;
                case 0x0000020:     ADD(ee, instruction);    break;
                case 0x000002A:     SLT(ee, instruction);    break;
                case 0x0000004:     SLLV(ee, instruction);   break;
                case 0x0000027:     NOR(ee, instruction);    break;
                case 0x0000021:     ADDU(ee, instruction);   break;
                case 0x000000B:     MOVN(ee, instruction);   break;
                case 0x000001A:     DIV(ee, instruction);    break;
                case 0x0000007:     SRAV(ee, instruction);   break;
                case 0x0000017:     DSRAV(ee, instruction);  break;
                case 0x0000014:     DSLLV(ee, instruction); break;
                case 0x000000C:     SYSCALL(ee, instruction); break;
                case 0x0000011:     MTHI(ee, instruction); break;
                case 0x0000013:     MTLO(ee, instruction); break;

                default: 
                {
                    errlog("[ERROR]: Could not interpret special instruction: [{:#09x}]\n", special);
                } break;
            }
        } break;

        case 0b001010:  SLTI(ee, instruction);   break;
        case 0b000101:  BNE(ee, instruction);    break;
        case 0b000010:  J(ee, instruction);      break;
        case 0b001111:  LUI(ee, instruction);    break;
        case 0b001100:  ANDI(ee, instruction);   break;
        case 0b001101:  ORI(ee, instruction);    break;
        case 0b001001:  ADDIU(ee, instruction);  break;
        case 0b101011:  SW(ee, instruction);     break;
        case 0b111111:  SD(ee, instruction);     break;
        case 0b000011:  JAL(ee, instruction);    break;
        case 0b000100:  BEQ(ee, instruction);    break;
        case 0b010100:  BEQL(ee, instruction);   break;
        case 0b001011:  SLTIU(ee, instruction);  break;
        case 0b010101:  BNEL(ee, instruction);   break;
        case 0b100011:  LW(ee, instruction);     break;
        case 0b100000:  LB(ee, instruction);     break;
        case 0b111001:  SWC1(ee, instruction);   break;
        case 0b100100:  LBU(ee, instruction);    break;
        case 0b000110:  BLEZ(ee, instruction);   break;
        case 0b110111:  LD(ee, instruction);     break;
        case 0b101000:  SB(ee, instruction);     break;
        case 0b101001:  SH(ee, instruction);     break;
        case 0b100001:  LH(ee, instruction);     break;
        case 0b100101:  LHU(ee, instruction);    break;
        case 0b000111:  BGTZ(ee, instruction);   break;
        case 0b001110:  XORI(ee, instruction);   break;
        case 0b011001:  DADDIU(ee, instruction); break;
        case 0b101111:  CACHE_IXIN(ee, instruction); break;
        case 0b011111:  SQ(ee, instruction);     break;
        case 0b011110:  LQ(ee, instruction);     break;
        case MMI:
        {
            int fmt = instruction & 0x3f;
            switch(fmt) 
            {
                case 0x00000001: MADDU(ee, instruction); break;
                case 0x00000012: MFLO1(ee, instruction); break;
                case 0x0000001B: DIVU1(ee, instruction); break;
                case 0x00000018: MULT1(ee, instruction); break;
                case 0x00000029:
                {
                    syslog("Dunno what this is\n");
                } break;

                default:
                {
                    errlog("[ERROR]: Could not interpret MMI instruction [{:#09x}]\n", fmt);
                } break;
            }

        } break;

        /***************************
        *       REGIMM
        ***************************/
        case REGIMM: 
        {
            int function = instruction >> 16 & 0x1f;
            switch(function) 
            {
                case 0b00000: BLTZ(ee, instruction); break;
                case 0b00001: BGEZ(ee, instruction); break;
                default:
                {
                    errlog("[ERROR]: Could not interpret REGIMM instruction [{:#09x}]\n", function);
                } break;
            }
        } break;


        default: 
        {
            errlog("[ERROR]: Could not interpret instructino.. opcode: [{:#09x}]\n", opcode);
        } break;
    }
}
// Uhhh haha..
#if 0
void
r5900_reset(R5900_Core ee)
{
    printf("Resetting Emotion Engine Core\n");
    memset(&ee, 0, sizeof(R5900_Core));
    ee = {
        .pc            = 0xbfc00000,
        .cop1.fcr0     = 0x2e30,
        .current_cycle = 0,
    };
    ee.cop0.regs[15]    = 0x2e20;
    _scratchpad_        = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));
    _icache_            = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));
    _dcache_            = (u8 *)malloc(sizeof(u8) * KILOBYTES(8));
/*
    // @@Remove: move these to particular files
    _vu0_code_memory_    = (u8 *)malloc(sizeof(u8) * KILOBYTES(4));
    _vu0_data_memory_    = (u8 *)malloc(sizeof(u8) * KILOBYTES(4));
    _vu1_code_memory_    = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));
    _vu1_data_memory_    = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));
    */
}
#endif
void 
r5900_cycle(R5900_Core *ee) 
{
 //   while(ee->pc) {
        // @@Implemetation: Figure out the amount of CPU cycles per instruction
        if (ee->delay_slot > 0) ee->delay_slot -= 1;
        if (ee->is_branching) {
            ee->is_branching     = false;
            ee->next_instruction = ee_core_load_32(ee->pc);
            decode_and_execute(ee, ee->next_instruction);
            ee->pc = ee->branch_pc;
        }
        ee->current_instruction  = ee_core_load_32(ee->pc);
        ee->next_instruction     = ee_core_load_32(ee->pc + 4);
        decode_and_execute(ee, ee->current_instruction);
        ee->pc += 4;
        ee->cop0.regs[9] += 1;
        ee->reg.r[0].SD[0] = 0;
        set_cop0_status(&ee->cop0.status, ee->cop0.regs[12]);
        set_cop0_cause(&ee->cop0.cause, ee->cop0.regs[13]);
   // }
}

void 
r5900_shutdown()
{
    free(_scratchpad_);
    fclose(dis);
}