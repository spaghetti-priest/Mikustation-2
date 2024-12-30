/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include <assert.h>

#include "fmt-10.2.1/include/fmt/format-inl.h"
#include "fmt-10.2.1/include/fmt/format.h"
#include "fmt-10.2.1/src/format.cc"

#include "../include/common.h"
#include "../include/ps2.h"
#include "../include/ee/r5900Interpreter.h"
#include "../include/ee/cop0.h"
#include "../include/ee/cop1.h"
#include "../include/kernel.h"

// @Cleanup
#include <cstdint>
#include <fstream>

//std::array<TLB_Entry, 48> TLBS;

FILE *dis = fopen("disasm.txt", "w+");
std::ofstream console("disasm.txt", std::ios::out);


static u8 *_icache_         = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));
static u8 *_dcache_         = (u8 *)malloc(sizeof(u8) * KILOBYTES(8));
static u8 *_scratchpad_     = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));

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

Range SCRATCHPAD = Range(0x70000000, KILOBYTES(16));


void
dump_all_ee_registers(R5900_Core *ee)
{
    for (int i = 0; i < 32; ++i) {
        printf("EE Register [%d] contains [%08x]\n", i, ee->reg.r[i].UD[0]);
    }
}

/*******************************************
 * Load Functions
*******************************************/
static inline u8 
ee_core_load_8 (u32 address) 
{
    if (SCRATCHPAD.contains(address))
        return *(uint8_t*)&_scratchpad_[address & 0x3FFF];
    if (address >= 0x30100000 && address < 0x31FFFFFF) // uncached and accelerated ram
        address -= 0x10000000; // move to unaccelerated ram

    // mask from virtual memory to physical
    address &= 0x1FFFFFFF;
    
    return ee_load_8(address);
}

static inline u16 
ee_core_load_16 (u32 address) 
{
    uint32_t r;
    if (SCRATCHPAD.contains(address))
        return *(uint16_t*)&_scratchpad_[address & 0x3FFF];
    if (address >= 0x30100000 && address < 0x31FFFFFF) // uncached and accelerated ram
        address -= 0x10000000;// move to unaccelerated ram

    address &= 0x1FFFFFFF;
    
    return ee_load_16(address);
}

static inline u32 
ee_core_load_32 (u32 address) 
{
    uint32_t r;
    if (SCRATCHPAD.contains(address))
        return *(uint32_t*)&_scratchpad_[address & 0x3FFF];
    if (address >= 0x30100000 && address < 0x31FFFFFF) // uncached and accelerated ram
        address -= 0x10000000; // move to unaccelerated ram

    address &= 0x1FFFFFFF;
    
    return ee_load_32(address);
}

static inline u64 
ee_core_load_64 (u32 address) 
{
    uint32_t r;
    if (SCRATCHPAD.contains(address))
        return *(uint64_t*)&_scratchpad_[address & 0x3FFF];
    if (address >= 0x30100000 && address < 0x31FFFFFF) // uncached and accelerated
        address -= 0x10000000; // move to unaccelerated ram

    address &= 0x1FFFFFFF;
    
    return ee_load_64(address);
}

static inline void ee_core_load_128(u32 address) {return;}

/*******************************************
 * Store Functions
*******************************************/
static inline void 
ee_core_store_8 (u32 address, u8 value) 
{
    if (SCRATCHPAD.contains(address)) {
        *(uint8_t*)&_scratchpad_[address & 0x3FFF] = value;
        return;
    }

    address &= 0x1FFFFFFF;
    ee_store_8(address, value);
}

static inline void 
ee_core_store_16 (u32 address, u16 value) 
{
    if (SCRATCHPAD.contains(address)) {
        *(uint16_t*)&_scratchpad_[address & 0x3FFF] = value;
        return;
    }

    address &= 0x1FFFFFFF;
    ee_store_16(address, value);
}

static inline void 
ee_core_store_32 (u32 address, u32 value) 
{
    if (SCRATCHPAD.contains(address)) {
        *(uint32_t*)&_scratchpad_[address & 0x3FFF] = value;
        return;
    }

    address &= 0x1FFFFFFF;
    ee_store_32(address, value);
}

static inline void 
ee_core_store_64 (u32 address, u64 value) 
{
    if (SCRATCHPAD.contains(address)) {
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

enum Type {
    _HALF,
    _WORD,
    _DOUBLE,
    _QUAD,
};

static inline void check_address_error_exception (R5900_Core *ee, Type type, u32 vaddr)
{
    u32 low_bits = 0;
    // If the exception system becomes a problem to debug just add more types to the type enum 
    // to have it point to the location of the functoin caller or something else more efficient 
    char *location = 0;
    switch(type)
    {
        case _HALF:     low_bits = vaddr & 0x1; break;
        case _WORD:     low_bits = vaddr & 0x3; break;
        case _DOUBLE:   low_bits = vaddr & 0x7; break;
        case _QUAD:     low_bits = vaddr & 0xF; break;
    };

    if (low_bits != 0) {
        errlog("[ERROR] Load/Store Address is not properly aligned {:#08x}\n", vaddr);
        Exception exc = get_exception(V_COMMON, __ADDRESS_ERROR);
        handle_exception_level_1(ee, &exc);
    }
}

static void 
LUI (R5900_Core *ee, u32 instruction) 
{
    s64 imm = (s64)(s32)((instruction & 0xFFFF) << 16);
    u32 rt  = instruction >> 16 & 0x1F;
    ee->reg.r[rt].UD[0] = imm;

    intlog("LUI [{:d}] [{:#x}]\n", rt, imm);
}

static void 
LB (R5900_Core *ee, u32 instruction) 
{
    u32 base        = instruction >> 21 & 0x1f;
    u32 rt          = instruction >> 16 & 0x1f;
    s32 offset      = (s16)(instruction & 0xFFFF);
    u32 vaddr       = ee->reg.r[base].UW[0] + offset;

    ee->reg.r[rt].SD[0] = (s64)ee_core_load_8(vaddr);
    intlog("LB [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);  
}

static void 
LBU (R5900_Core *ee, u32 instruction) 
{
    u32 base        = instruction >> 21 & 0x1f;
    u32 rt          = instruction >> 16 & 0x1f;
    s16 offset      = (s16)(instruction & 0xFFFF);
    u32 vaddr       = ee->reg.r[base].UW[0] + offset;
    
    ee->reg.r[rt].UD[0] = (u64)ee_core_load_8(vaddr);
    intlog("LBU [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);    
}

static void
LH (R5900_Core *ee, u32 instruction) 
{
    s32 offset  = (s16)(instruction & 0xFFFF);
    u32 base    = instruction >> 21 & 0x1F;
    u32 rt      = instruction >> 16 & 0x1F;
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    
    check_address_error_exception(ee, _HALF, vaddr);

    ee->reg.r[rt].SD[0] = (s64)ee_core_load_16(vaddr);
    intlog("LH [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);    
}

static void
LHU (R5900_Core *ee, u32 instruction)
{
    s32 offset  = (s16)(instruction & 0xFFFF);
    u32 base    = (instruction >> 21) & 0x1F;
    u32 rt      = (instruction >> 16) & 0x1F;
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;

    check_address_error_exception(ee, _HALF, vaddr);

    ee->reg.r[rt].UD[0] = (u64)ee_core_load_16(vaddr);
    intlog("LHU [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
}

static void 
LW (R5900_Core *ee, u32 instruction) 
{
    s32 offset  = (s16)(instruction & 0xFFFF);
    u32 base    = instruction >> 21 & 0x1F;
    u32 rt      = instruction >> 16 & 0x1F;
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    
    check_address_error_exception(ee, _WORD, vaddr);

    ee->reg.r[rt].SD[0] = (s64)ee_core_load_32(vaddr);
    intlog("LW [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
}

static void 
LWC1 (R5900_Core *ee, u32 instruction) 
{
    s32 offset  = (s16)(instruction & 0xFFFF);
    u32 base    = instruction >> 21 & 0x1F;
    u32 ft      = instruction >> 16 & 0x1F;
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    
    check_address_error_exception(ee, _WORD, vaddr);

    u32 data = (u32)ee_core_load_32(vaddr);

    cop1_setFPR(ft, data);
    intlog("LWC1 [{:d}] [{:#x}] [{:d}] \n", ft, offset, base);
}

static void 
LWU (R5900_Core *ee, u32 instruction) 
{
    s32 offset  = (s16)(instruction & 0xFFFF);
    u32 base    = instruction >> 21 & 0x1F;
    u32 rt      = instruction >> 16 & 0x1F;
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;

    check_address_error_exception(ee, _WORD, vaddr);
   
    ee->reg.r[rt].SD[0] = (u64)ee_core_load_32(vaddr);
    intlog("LWU [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
}

static void 
LD (R5900_Core *ee, u32 instruction) 
{
    u32 base    = instruction >> 21 & 0x1f;
    u32 rt      = instruction >> 16 & 0x1f;
    s16 offset  = (s16)(instruction & 0xFFFF);
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    
    check_address_error_exception(ee, _DOUBLE, vaddr);

    ee->reg.r[rt].UD[0] = ee_core_load_64(vaddr);
    intlog("LD [{:d}] [{:#x}] [{:d}]\n", rt, offset, base);
}

static void 
LDL (R5900_Core *ee, u32 instruction) 
{
    /* This is an interesting and clear implemtation from Dobiestation */
    const u64 LDL_MASK[8] =
    {   0x00ffffffffffffffULL, 0x0000ffffffffffffULL, 0x000000ffffffffffULL, 0x00000000ffffffffULL,
        0x0000000000ffffffULL, 0x000000000000ffffULL, 0x00000000000000ffULL, 0x0000000000000000ULL
    };

    const u8 LDL_SHIFT[8] = {56, 48, 40, 32, 24, 16, 8, 0};
    
    u32 base    = instruction >> 21 & 0x1f;
    u32 rt      = instruction >> 16 & 0x1f;
    s16 offset  = (s16)(instruction & 0xFFFF);
    
    u32 vaddr           = ee->reg.r[base].UW[0] + offset;
    u32 aligned_vaddr   = vaddr & ~0x7;
    u32 shift           = vaddr & 0x7;
    
    u64 aligned_dword   = ee_core_load_64(aligned_vaddr);
    u64 result          = (ee->reg.r[rt].UD[0] & LDL_MASK[shift] | aligned_vaddr << LDL_SHIFT[shift]);
    ee->reg.r[rt].UD[0] = result;
    //intlog("LDL [{:d}] [{:#x}] [{:d}]\n", rt, offset, base);
    printf("LDL [{%d}] [{%#x}] [{%d}]\n", rt, offset, base);
}

static void 
LDR (R5900_Core *ee, u32 instruction) 
{
    /* This is an interesting and clear implemtation from Dobiestation */
    const u64 LDR_MASK[8] = 
    {
        0x0000000000000000ULL, 0xff00000000000000ULL, 0xffff000000000000ULL, 0xffffff0000000000ULL, 
        0xffffffff00000000ULL, 0xffffffffff000000ULL, 0xffffffffffff0000ULL, 0xffffffffffffff00ULL      
    };

    const u8 LDR_SHIFT[8] = {0, 8, 16, 24, 32, 40, 48, 56};
    
    u32 base    = instruction >> 21 & 0x1f;
    u32 rt      = instruction >> 16 & 0x1f;
    s16 offset  = (s16)(instruction & 0xFFFF);
    
    u32 vaddr           = ee->reg.r[base].UW[0] + offset;
    u32 aligned_vaddr   = vaddr & ~0x7;
    u32 shift           = vaddr & 0x7;
    
    u64 aligned_dword   = ee_core_load_64(aligned_vaddr);
    u64 result          = (ee->reg.r[rt].UD[0] & LDR_MASK[shift] | aligned_vaddr << LDR_SHIFT[shift]);
    ee->reg.r[rt].UD[0] = result;
   // intlog("LDR [{:d}] [{:#x}] [{:d}]\n", rt, offset, base);
    printf("LDR [{%d}] [{%#x}] [{%d}]\n", rt, offset, base);
}

static void 
LQ (R5900_Core *ee, u32 instruction) 
{
    u32 base    = instruction >> 21 & 0x1f;
    u32 rt      = instruction >> 16 & 0x1f;
    s16 offset  = (s16)(instruction & 0xFFFF);
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    
    check_address_error_exception(ee, _QUAD, vaddr);

    ee->reg.r[rt].UD[0] = ee_core_load_64(vaddr);
    ee->reg.r[rt].UD[1] = ee_core_load_64(vaddr + 8);
    intlog("LQ [{:d}] [{:#x}] [{:d}]\n", rt, offset, base);
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
    intlog("SB [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
}

static void 
SH (R5900_Core *ee, u32 instruction) 
{
    s32 offset      = (s16)(instruction & 0xFFFF);
    u32 rt          = (instruction >> 16) & 0x1F;
    u32 base        = (instruction >> 21) & 0x1F;
    u32 vaddr       = ee->reg.r[base].UW[0] + offset;
    s16 value       = ee->reg.r[rt].SH[0];

    check_address_error_exception(ee, _HALF, vaddr);

    ee_core_store_16(vaddr, value);
    intlog("SH [{:d}] [{:#x}] [{:d}] \n", rt, offset, base); 
}

static void 
SW (R5900_Core *ee, u32 instruction) 
{
    s16 offset  = (s16)(instruction & 0xFFFF);
    u32 base    = instruction >> 21 & 0x1F;
    u32 rt      = instruction >> 16 & 0x1F;
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    s32 value   = ee->reg.r[rt].SW[0];
    
    check_address_error_exception(ee, _WORD, vaddr);

    ee_core_store_32(vaddr, value);
    intlog("SW [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
}

static void 
SWC1 (R5900_Core *ee, u32 instruction) 
{
    u32 base    = instruction >> 21 & 0x1f;
    u32 ft      = instruction >> 16 & 0x1f;
    s16 offset  = (s16)(instruction & 0xFFFF);
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    
    check_address_error_exception(ee, _WORD, vaddr);

    ee_core_store_32(vaddr, cop1_getFPR(ft));
    intlog("SWC1 [{:d}] [{:#x}] [{:d}] \n", ft, offset, base); 
}

static void 
SD (R5900_Core *ee, u32 instruction) 
{
    s16 offset  = (s16)(instruction & 0xFFFF);
    u32 rt      = instruction >> 16 & 0x1F;
    u32 base    = instruction >> 21 & 0x1f;
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    u64 value   = ee->reg.r[rt].UD[0];
  
    check_address_error_exception(ee, _DOUBLE, vaddr);

    ee_core_store_64(vaddr, value);   
    intlog("SD [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
}

static void 
SDL (R5900_Core *ee, u32 instruction) 
{
    /* This is an interesting and clear implemtation from Dobiestation */
    const u64 SDL_MASK[8] =
    {   0x00ffffffffffffffULL, 0x0000ffffffffffffULL, 0x000000ffffffffffULL, 0x00000000ffffffffULL,
        0x0000000000ffffffULL, 0x000000000000ffffULL, 0x00000000000000ffULL, 0x0000000000000000ULL
    };

    const u8 SDL_SHIFT[8] = {56, 48, 40, 32, 24, 16, 8, 0};
    
    u32 base    = instruction >> 21 & 0x1f;
    u32 rt      = instruction >> 16 & 0x1f;
    s16 offset  = (s16)(instruction & 0xFFFF);
    
    u32 vaddr           = ee->reg.r[base].UW[0] + offset;
    u32 aligned_vaddr   = vaddr & ~0x7;
    u32 shift           = vaddr & 0x7;
    
    u64 aligned_dword   = ee_core_load_64(aligned_vaddr);
    u64 result          = (ee->reg.r[rt].UD[0] & SDL_MASK[shift] | aligned_vaddr << SDL_SHIFT[shift]);
    ee_core_store_64(aligned_vaddr, result);
    //intlog("SDL [{:d}] [{:#x}] [{:d}]\n", rt, offset, base);
    printf("SDL [{%d}] [{%#x}] [{%d}]\n", rt, offset, base);
}

static void 
SDR (R5900_Core *ee, u32 instruction) 
{
    /* This is an interesting and clear implemtation from Dobiestation */
    const u64 SDR_MASK[8] = 
    {
        0x0000000000000000ULL, 0xff00000000000000ULL, 0xffff000000000000ULL, 0xffffff0000000000ULL, 
        0xffffffff00000000ULL, 0xffffffffff000000ULL, 0xffffffffffff0000ULL, 0xffffffffffffff00ULL      
    };

    const u8 SDR_SHIFT[8] = {0, 8, 16, 24, 32, 40, 48, 56};
    
    u32 base    = instruction >> 21 & 0x1f;
    u32 rt      = instruction >> 16 & 0x1f;
    s16 offset  = (s16)(instruction & 0xFFFF);
    
    u32 vaddr           = ee->reg.r[base].UW[0] + offset;
    u32 aligned_vaddr   = vaddr & ~0x7;
    u32 shift           = vaddr & 0x7;
    
    u64 aligned_dword   = ee_core_load_64(aligned_vaddr);
    u64 result          = (ee->reg.r[rt].UD[0] & SDR_MASK[shift] | aligned_vaddr << SDR_SHIFT[shift]);
    ee_core_store_64(aligned_vaddr, result);
    //intlog("SDR [{:d}] [{:#x}] [{:d}]\n", rt, offset, base);
    printf("SDR [{%d}] [{%#x}] [{%d}]\n", rt, offset, base);
}

//@@Incomplete: Change this to using uint128 data structure
static void 
SQ (R5900_Core *ee, u32 instruction) 
{
    s16 offset  = (s16)(instruction & 0xFFFF);
    u32 rt      = instruction >> 16 & 0x1F;
    u32 base    = instruction >> 21 & 0x1f;
    u32 vaddr   = ee->reg.r[base].UW[0] + offset;
    u64 lov     = ee->reg.r[rt].UD[0];
    u64 hiv     = ee->reg.r[rt].UD[1];
   
    check_address_error_exception(ee, _QUAD, vaddr);

    ee_core_store_64(vaddr, lov);       
    ee_core_store_64(vaddr + 8, hiv);       
    intlog("SQ [{:d}] [{:#x}] [{:d}] \n", rt, offset, base);
}

/*******************************************
* Computational ALU Instructions
* Arithmetic
• Logical
• Shift
• Multiply
• Divide
*******************************************/
static void add_overflow() {return;}
static void add_overflow64() {return;}
static void sub_overflow() {return;}
static void sub_overflow64() {return;}

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
    intlog("ADD [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);   
}

static void 
ADDU (R5900_Core *ee, u32 instruction) 
{
    u32 rd = (instruction >> 11) & 0x1F;
    u32 rt = (instruction >> 16) & 0x1F;
    u32 rs = (instruction >> 21) & 0x1F;

    int32_t result = ee->reg.r[rs].SW[0] + ee->reg.r[rt].SW[0];
    ee->reg.r[rd].UD[0] = result;
    intlog("ADDU [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);
}

static void 
ADDI (R5900_Core *ee, u32 instruction) 
{
    s16 imm = (s16)(instruction & 0xFFFF);
    u32 rs  = instruction >> 21 & 0x1f;
    u32 rt  = instruction >> 16 & 0x1f;
    s32 result = ee->reg.r[rs].SD[0] + imm;
    ee->reg.r[rt].SD[0] = result;
    /* @@Incomplete: No 2 complement Arithmetic Overflow error implementation*/
    intlog("ADDI: [{:d}] [{:d}] [{:#x}] \n", rt, rs, imm);
}

static void 
ADDIU (R5900_Core *ee, u32 instruction) 
{
    s16 imm = (s16)(instruction & 0xFFFF);
    u32 rs  = instruction >> 21 & 0x1f;
    u32 rt  = instruction >> 16 & 0x1f;
    s32 result = ee->reg.r[rs].SD[0] + imm;
    ee->reg.r[rt].SD[0] = result;
    intlog("ADDIU: [{:d}] [{:d}] [{:#x}] \n", rt, rs, imm);
}

static void 
DADDU (R5900_Core *ee, u32 instruction) 
{
    u32 rs = instruction >> 21 & 0x1f;
    u32 rt = instruction >> 16 & 0x1f;
    u32 rd = instruction >> 11 & 0x1f;
    ee->reg.r[rd].UD[0] = ee->reg.r[rs].SD[0] + ee->reg.r[rt].SD[0];

    intlog("DADDU [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);  
}

static void
DADDIU (R5900_Core *ee, u32 instruction)
{
    s16 imm = (s16)(instruction & 0xFFFF);
    u32 rs  = instruction >> 21 & 0x1F;
    u32 rt  = instruction >> 16 & 0x1F;

    ee->reg.r[rt].UD[0] = ee->reg.r[rs].SD[0] + imm;
    intlog("DADDIU [{:d}] [{:d}] [{:#x}]\n", rt, rs, imm);  
}

static void 
SUB (R5900_Core *ee, u32 instruction) 
{
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    u32 rs = instruction >> 21 & 0x1F;
    s32 result = ee->reg.r[rs].SW[0] - ee->reg.r[rt].SW[0];
    ee->reg.r[rd].UD[0] = (s64)result;

    /* @@Incomplete: No 2 complement Arithmetic Overflow error implementation*/
    intlog("SUB [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);    
}

static void 
SUBU (R5900_Core *ee, u32 instruction) 
{
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    u32 rs = instruction >> 21 & 0x1F;
    s32 result = ee->reg.r[rs].SW[0] - ee->reg.r[rt].SW[0];
    ee->reg.r[rd].UD[0] = (u64)result;
    intlog("SUBU [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);    
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

    intlog("MULT [{:d}] [{:d}] [{:d}]\n", rs, rt, rd);
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

    intlog("MULT1 [{:d}] [{:d}] [{:d}]\n", rs, rt, rd);
}

static void 
MADDU (R5900_Core *ee, u32 instruction)
{
    errlog("LOL\n");
    intlog("MADDU\n");
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
    intlog("DIV [{:d}] [{:d}]\n", rs, rt);
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

    intlog("DIVU [{:d}] [{:d}]\n", rs, rt);
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

    intlog("DIVU_1 [{:d}] [{:d}]\n", rs, rt);
}

static void 
AND (R5900_Core *ee, u32 instruction) 
{
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    u32 rs = instruction >> 21 & 0x1F;
    ee->reg.r[rd].UD[0] = ee->reg.r[rs].UD[0] & ee->reg.r[rt].UD[0];
    intlog("AND [{:d}] [{:d}] [{:d}] \n", rd, rs, rt);
}

static void 
ANDI (R5900_Core *ee, u32 instruction) 
{
    u64 imm = (u16)(instruction & 0xFFFF);
    u32 rs = instruction >> 21 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    ee->reg.r[rt].UD[0] = ee->reg.r[rs].UD[0] & imm; 

    intlog("ANDI: [{:d}] [{:d}] [{:#x}] \n", rt, rs, imm);
}

static void 
OR (R5900_Core *ee, u32 instruction) 
{
    u32 rs = instruction >> 21 & 0x1f;
    u32 rt = instruction >> 16 & 0x1f;
    u32 rd = instruction >> 11 & 0x1f;
    ee->reg.r[rd].SD[0] = ee->reg.r[rs].SD[0] | ee->reg.r[rt].SD[0];

    intlog("OR [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);
}

static void 
ORI (R5900_Core *ee, u32 instruction) 
{
    u64 imm = (instruction & 0xFFFF); 
    u32 rs  = instruction >> 21 & 0x1F;
    u32 rt  = instruction >> 16 & 0x1F;          
    ee->reg.r[rt].UD[0] = ee->reg.r[rs].UD[0] | imm; 

    intlog("ORI: [{:d}] [{:d}] [{:#x}] \n", rt, rs, imm);
}

/* @Temporary: No XOR instruction? */
static void
XORI (R5900_Core *ee, u32 instruction)
{
    u64 imm = (u16)(instruction & 0xFFFF);
    u32 rs  = instruction >> 21 & 0x1F;
    u32 rt  = instruction >> 16 & 0x1F;

    ee->reg.r[rt].UD[0] = ee->reg.r[rs].UD[0] ^ imm;
    intlog("XORI [{:d}] [{:d}] [{:#x}] \n", rt, rs, imm);
}

static void 
NOR (R5900_Core *ee, u32 instruction) 
{
    u32 rs = instruction >> 21 & 0x1f;
    u32 rt = instruction >> 16 & 0x1f;
    u32 rd = instruction >> 11 & 0x1f;
    ee->reg.r[rd].SD[0] = ~(ee->reg.r[rs].SD[0] | ee->reg.r[rt].SD[0]);
    assert(1);
    intlog("NOR [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);
}

static void 
SLL (R5900_Core *ee, u32 instruction) 
{
    u32 sa = instruction >> 6 & 0x1F;
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    ee->reg.r[rd].SD[0] = (u64)((s32)ee->reg.r[rt].UW[0] << sa);

    intlog("SLL source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, sa);
}

static void 
SLLV (R5900_Core *ee, u32 instruction) 
{
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    u32 rs = instruction >> 21 & 0x1F;
    u32 sa = ee->reg.r[rs].SW[0] & 0x1F;

    ee->reg.r[rd].SD[0] = (s64)(ee->reg.r[rt].SW[0] << sa);

    intlog("SLLV source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, sa);
}

static void 
DSLL (R5900_Core *ee, u32 instruction) 
{
    u32 sa = instruction >> 6 & 0x1F;
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    ee->reg.r[rd].UD[0] = ee->reg.r[rt].UD[0] << sa;
    intlog("DSLL source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, sa);
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
    intlog("DSLLV [{:d}] [{:d}] [{:d}]\n", rd, rt, rs); 
}

static void 
SRL (R5900_Core *ee, u32 instruction) 
{
    u32 sa = instruction >> 6 & 0x1F;
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    u32 result = (ee->reg.r[rt].UW[0] >> sa);
    ee->reg.r[rd].SD[0] = (s32)result;

    intlog("SRL source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, sa);
}

static void 
SRLV (R5900_Core *ee, u32 instruction) 
{
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    u32 rs = instruction >> 21 & 0x1F;
    u32 sa = ee->reg.r[rs].SW[0] & 0x1F;

    ee->reg.r[rd].SD[0] = (s64)(ee->reg.r[rt].SW[0] >> sa);

    intlog("SRLV source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, sa);
}

static void 
SRA (R5900_Core *ee, u32 instruction) 
{
    s32 sa      = instruction >> 6 & 0x1F;
    u32 rd      = instruction >> 11 & 0x1F;
    u32 rt      = instruction >> 16 & 0x1F;
    s32 result  = (ee->reg.r[rt].SW[0]) >> sa;
    ee->reg.r[rd].SD[0] = result;

    intlog("SRA source: [{:d}] dest: [{:d}] [{:#x}]\n", rd, rt, sa);
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

    intlog("SRAV [{:d}] [{:d}] [{:d}]\n", rd, rt, rs);
}

static void 
DSRAV (R5900_Core *ee, u32 instruction)
{
    u32 rs = (instruction >> 21) & 0x1F;
    u32 rt = (instruction >> 16) & 0x1F;
    u32 rd = (instruction >> 11) & 0x1F;

    u32 sa = ee->reg.r[rs].UW[0] & 0x3F;
    ee->reg.r[rd].SD[0] = ee->reg.r[rt].SD[0] >> sa;

    intlog("DSRAV [{:d}] [{:d}] [{:d}]\n", rd, rt, rs);
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

    intlog("DSRA32 source: [{:d}] dest: [{:d}] s: [{:#x}] \n", rd, rt, s);
}

static void 
DSLL32 (R5900_Core *ee, u32 instruction) 
{
    u32 sa  = instruction >> 6 & 0x1F;
    u32 rd  = instruction >> 11 & 0x1F;
    u32 rt  = instruction >> 16 & 0x1F;
    u32 s   = sa + 32;

    ee->reg.r[rd].UD[0] = ee->reg.r[rt].UD[0] << s;

    intlog("DSLL32 source: [{:d}] dest: [{:d}] s: [{:#x}] \n", rd, rt, s);
}

static void 
DSRL (R5900_Core *ee, u32 instruction) 
{
    u32 sa  = instruction >> 6 & 0x1F;
    u32 rd  = instruction >> 11 & 0x1F;
    u32 rt  = instruction >> 16 & 0x1F;

    ee->reg.r[rd].UD[0] = ee->reg.r[rt].UD[0] >> sa;

    intlog("DSRL source: [{:d}] dest: [{:d}] s: [{:#x}] \n", rd, rt, sa);
}

static void 
DSRL32 (R5900_Core *ee, u32 instruction) 
{
    u32 sa  = instruction >> 6 & 0x1F;
    u32 rd  = instruction >> 11 & 0x1F;
    u32 rt  = instruction >> 16 & 0x1F;
    u32 s   = sa + 32;

    ee->reg.r[rd].UD[0] = ee->reg.r[rt].UD[0] >> s;

    intlog("DSRL32 source: [{:d}] dest: [{:d}] s: [{:#x}] \n", rd, rt, s);
}

static void 
SLT (R5900_Core *ee, u32 instruction) 
{
    u32 rd = instruction >> 11 & 0x1F;
    u32 rt = instruction >> 16 & 0x1F;
    u32 rs = instruction >> 21 & 0x1F;

    int result = ee->reg.r[rs].SD[0] < ee->reg.r[rt].SD[0] ? 1 : 0;
    ee->reg.r[rd].SD[0] = result;
    intlog("SLT [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);
}

static void 
SLTU (R5900_Core *ee, u32 instruction) 
{
    u32 rs = (instruction >> 21) & 0x1F;
    u32 rt = (instruction >> 16) & 0x1F;
    u32 rd = (instruction >> 11) & 0x1F;
    uint32_t result = (ee->reg.r[rs].UD[0] < ee->reg.r[rt].UD[0]) ? 1 : 0;
    ee->reg.r[rd].UD[0] = result;
    
    intlog("SLTU [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);
}

static void 
SLTI (R5900_Core *ee, u32 instruction) 
{
    s16 imm = (s16)(instruction & 0xFFFF);
    u32 rt  = instruction >> 16 & 0x1F;
    u32 rs  = instruction >> 21 & 0x1F;

    int result = (ee->reg.r[rs].SD[0] < (s32)imm) ? 1 : 0;
    ee->reg.r[rt].SD[0] = result;
    
    intlog("SLTI [{:d}] [{:d}], [{:#x}]\n", rt, rs, imm);
}

static void 
SLTIU (R5900_Core *ee, u32 instruction) 
{
    u64 imm = (s16)(instruction & 0xFFFF);
    u32 rs = instruction >> 21 & 0x1f;
    u32 rt = instruction >> 16 & 0x1f;

    u32 result = (ee->reg.r[rs].UD[0] < imm) ? 1 : 0;
    ee->reg.r[rt].UD[0] = result;
    
    intlog("SLTIU [{:d}] [{:d}] [{:#x}]\n", rt, rs, imm);
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
    intlog("J [{:#x}]\n", offset);
}

static void 
JR(R5900_Core *ee, u32 instruction) 
{
    u32 source = instruction >> 21 & 0x1F;
    //@Temporary: check the 2 lsb if 0
    jump_to(ee, ee->reg.r[source].UW[0]);

    intlog("JR source: [{:d}] pc_dest: [{:#x}]\n", source, ee->reg.r[source].UW[0]);
}

static void 
JAL(R5900_Core *ee, u32 instruction) 
{
    u32 instr_index     = (instruction & 0x3FFFFFF);
    u32 target_address  = ((ee->pc + 4) & 0xF0000000) + (instr_index << 2);
    jump_to(ee, target_address); 
    ee->reg.r[31].UD[0]  = ee->pc + 8;

    intlog("JAL [{:#x}] \n", target_address);   
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

    intlog("JALR [{:d}]\n", rs);
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

    intlog("BNE [{:d}] [{:d}], [{:#x}]\n", rt, rs, imm);
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

    intlog("BEQ [{:d}] [{:d}] [{:#x}] \n", rs, rt, imm);
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

    intlog("BEQL [{:d}] [{:d}] [{:#x}]\n", rs, rt, imm);    
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

    intlog("BNEL [{:d}] [{:d}] [{:#x}]\n", rs, rt, imm);
}

static void 
BLEZ (R5900_Core *ee, u32 instruction) 
{
    s16 offset  = (s16)((instruction & 0xFFFF) << 2);
    u32 rs      = instruction >> 21 & 0x1F;

    bool condition = ee->reg.r[rs].SD[0] <= 0;
    branch(ee, condition, offset);
    intlog("BLEZ [{:d}] [{:#x}]\n", rs, offset); 
}

static void 
BLEZL (R5900_Core *ee, u32 instruction) 
{
    s16 offset  = (s16)((instruction & 0xFFFF) << 2);
    u32 rs      = instruction >> 21 & 0x1F;

    bool condition = ee->reg.r[rs].SD[0] <= 0;
    branch_likely(ee, condition, offset);
    intlog("BLEZ [{:d}] [{:#x}]\n", rs, offset); 
}

static void 
BLTZ (R5900_Core *ee, u32 instruction) 
{
    s32 offset  = (s16)(instruction & 0xFFFF) << 2;
    u32 rs      = instruction >> 21 & 0x1f;

    bool condition = ee->reg.r[rs].SD[0] < 0;
    branch(ee, condition, offset);
    intlog("BLTZ [{:d}] [{:#x}] \n", rs, offset);
}

static void
BGTZ (R5900_Core *ee, u32 instruction)
{
    s32 offset  = (s16)(instruction & 0xFFFF) << 2;
    u32 rs      = instruction >> 21 & 0x1F;

    bool condition = ee->reg.r[rs].SD[0] > 0;
    branch(ee, condition, offset);
    intlog("BGTZ [{:d}] [{:#x}] \n", rs, offset);
}

static void 
BGEZ (R5900_Core *ee, u32 instruction) 
{
    s32 offset  = (s16)(instruction & 0xFFFF) << 2;
    u32 rs      = instruction >> 21 & 0x1f;

    bool condition = ee->reg.r[rs].SD[0] >= 0;
    branch(ee, condition, offset);
    intlog("BGEZ [{:d}] [{:#x}] \n", rs, offset);    
}

/*******************************************
 * Miscellaneous Instructions
*******************************************/
// @@Implementation @@Debug: We have no debugger yet so theres no need to implement this now
static void 
BREAKPOINT(R5900_Core *ee, u32 instruction) 
{
    intlog("BREAKPOINT\n");
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
    intlog("SYNC\n");
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

    intlog("MFC0 GPR: [{:d}] COP0: [{:d}]\n", gpr, cop0);
}

static void 
MTC0 (R5900_Core *ee, u32 instruction) 
{
    u32 cop0            = instruction >> 11 & 0x1F;
    u32 gpr             = instruction >> 16 & 0x1F;
    ee->cop0.regs[cop0]  = ee->reg.r[gpr].SW[0];

    intlog("MTC0 GPR: [{:d}] COP0: [{:d}]\n", gpr, cop0);
}

static void
ERET (R5900_Core *ee, u32 instruction) 
{
    //@@Removal @@Implementation: This could be shortened down a fair bit with just an mfc0 instruction
    ee->pc = ee->cop0.EPC;
}

// @@Implemtation: implement TLB
static void 
TLBWI (R5900_Core *ee, u32 instruction, int index) 
{
    //TLB_Entry current = TLBS.at(index);
    intlog("TLBWI not yet implemented\n");
}

static void 
CACHE_IXIN (R5900_Core *ee, u32 instruction) 
{
    intlog("Invalidate instruction cache");
}

#define BIOS_HLE
#ifndef BIOS_HLE
static void
SYSCALL (R5900_Core *ee, u32 instruction)
{
    printf("SYSCALL [%#02x]  ", ee->reg.r[3].UB[0]);
    Exception exc = get_exception(V_COMMON, __SYSCALL);
    handle_exception_level_1(ee, &exc);
}

#else

/* 
From:   https://www.psdevwiki.com/ps2/Syscalls 
        https://psi-rockin.github.io/ps2tek/#bioseesyscalls
*/
static void
SYSCALL (R5900_Core *ee, u32 instruction)
{
    //printf("SYSCALL [%#02x]  ", ee->reg.r[3].UB[0]);
    u32 syscall = ee->reg.r[3].UB[0];
    void *return_   = (void *)ee->reg.r[2].UD[0];
    void *param0    = (void *)ee->reg.r[4].UD[0];
    void *param1    = (void *)ee->reg.r[5].UD[0];
    void *param2    = (void *)ee->reg.r[6].UD[0];
    void *param3    = (void *)ee->reg.r[7].UD[0];
    void *param4    = (void *)ee->reg.r[8].UD[0];

    switch(syscall)
    {
        case 0x02: SetGsCrt((bool)param0, (s32)param1, (bool)param2);                                       break;
        case 0x3C: InitMainThread((u32)param0, param1, (s32)param2, (char*)param3, (s32)param4, return_);   break;
        case 0x3D: InitHeap(param0, (s32)param1, return_);                                                  break;
        case 0x64: FlushCache();                                                                            break;
        case 0x71: GsPutIMR((u64)param0);                                                                   break;
        default:
        {
            errlog("Unknown Syscall: [%#x]\n", syscall);
            return;
        }
    }
}

#endif
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
    intlog("MOVN [{:d}] [{:d}] [{:d}]\n", rd, rs, rt);
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
    intlog("MOVZ [{:d}] [{:d}] [{:d}]\n",rd, rs, rt);
}

static void 
MFLO (R5900_Core *ee, u32 instruction) 
{
    u32 rd = instruction >> 11 & 0x1f;
    ee->reg.r[rd].UD[0] = ee->LO;

    intlog("MFLO [{:d}] \n", rd);
}

static void 
MFLO1 (R5900_Core *ee, u32 instruction)
{
    u32 rd = (instruction >> 11) & 0x1F;
    ee->reg.r[rd].UD[0] = ee->LO1;
    intlog("MFLO_1 [{:d}]\n", rd);
}

static void 
MFHI (R5900_Core *ee, u32 instruction) 
{
    u32 rd = instruction >> 11 & 0x1F;
    ee->reg.r[rd].UD[0] = ee->HI;
    intlog("MFHI [{:d}]\n", rd);
}

static void
MTLO (R5900_Core *ee, u32 instruction) 
{
    u32 rs = instruction >> 21 & 0x1F;
    ee->LO = ee->reg.r[rs].UD[0];
    intlog("MTLO [{:d}]\n", rs);
}

static void
MTHI (R5900_Core *ee, u32 instruction) 
{
    u32 rs = instruction >> 21 & 0x1F;
    ee->HI = ee->reg.r[rs].UD[0];
    intlog("MTHI [{:d}]\n", rs);
}

void 
decode_and_execute (R5900_Core *ee, u32 instruction)
{
    if (instruction == 0x00000000) return;
    int opcode = instruction >> 26;
    switch (opcode) {
        case COP0:
        {
            int fmt = instruction >> 21 & 0x3F;
            switch (fmt) 
            {
                case 0x000000: MFC0(ee, instruction);  break;
                case 0x000004: MTC0(ee, instruction);  break;
                case 0x10000:
                {
                    case 0x000010: 
                    {
                        int index = ee->cop0.regs[0];
                        TLBWI(ee, instruction, index);
                    } break;

                    case 0x011000:
                    {
                        ERET(ee, instruction);
                    } break;
                } break;

                default:
                {
                    errlog("[ERROR]: Could not interpret COP0 instruction format opcode [{:#09x}]\n", fmt);
                } break;
            }
        } break;

        case COP1: { cop1_decode_and_execute(ee, instruction); } break;

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
                case 0x0000003A:    DSRL(ee, instruction); break;
                case 0x0000003E:    DSRL32(ee, instruction); break;
                case 0x00000024:    AND(ee, instruction);    break;
                case 0x00000002:    SRL(ee, instruction);    break;
                case 0x00000003:    SRA(ee, instruction);    break;
                case 0x000000A:     MOVZ(ee, instruction);   break;
                case 0x0000022:     SUB(ee, instruction);    break;
                case 0x0000023:     SUBU(ee, instruction);   break;
                case 0x0000010:     MFHI(ee, instruction);   break;
                case 0x0000020:     ADD(ee, instruction);    break;
                case 0x000002A:     SLT(ee, instruction);    break;
                case 0x0000004:     SLLV(ee, instruction);   break;
                case 0x0000006:     SRLV(ee, instruction);   break;
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
                    errlog("[ERROR]: Could not interpret special instruction: [{:#09x}]\n", instruction);
                } break;
            }
        } break;

        case 0b001010:  SLTI(ee, instruction);   break;
        case 0b000101:  BNE(ee, instruction);    break;
        case 0b000010:  J(ee, instruction);      break;
        case 0b001111:  LUI(ee, instruction);    break;
        case 0b001100:  ANDI(ee, instruction);   break;
        case 0b001101:  ORI(ee, instruction);    break;
        case 0b001000:  ADDI(ee, instruction);   break;
        case 0b001001:  ADDIU(ee, instruction);  break;
        case 0b101011:  SW(ee, instruction);     break;
        case 0b111111:  SD(ee, instruction);     break;
        case 0b101100:  SDL(ee, instruction);    break;
        case 0b101101:  SDR(ee, instruction);    break;
        case 0b000011:  JAL(ee, instruction);    break;
        case 0b000100:  BEQ(ee, instruction);    break;
        case 0b010100:  BEQL(ee, instruction);   break;
        case 0b001011:  SLTIU(ee, instruction);  break;
        case 0b010101:  BNEL(ee, instruction);   break;
        case 0b100011:  LW(ee, instruction);     break;
        case 0b110001:  LWC1(ee, instruction);   break;
        case 0b100111:  LWU(ee, instruction);    break;
        case 0b100000:  LB(ee, instruction);     break;
        case 0b111001:  SWC1(ee, instruction);   break;
        case 0b100100:  LBU(ee, instruction);    break;
        case 0b000110:  BLEZ(ee, instruction);   break;
        case 0b010110:  BLEZL(ee, instruction);  break;
        case 0b110111:  LD(ee, instruction);     break;
        case 0b011010:  LDL(ee, instruction);    break;
        case 0b011011:  LDR(ee, instruction);    break;
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
                    intlog("Dunno what this is\n");
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
            errlog("[ERROR]: Could not interpret instructino.. opcode: [{:#09x}]\n", instruction);
        } break;
    }
}

void
ee_reset(R5900_Core *ee)
{
    printf("Resetting Emotion Engine Core\n");
    memset(ee, 0, sizeof(R5900_Core));
    ee->pc            = 0xbfc00000;
    ee->current_cycle = 0;
    ee->cop0.regs[15] = 0x2e20;

    // _scratchpad_        = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));
    // _icache_            = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));
    // _dcache_            = (u8 *)malloc(sizeof(u8) * KILOBYTES(8));
}

//#endif
inline void 
set_kernel_mode (COP0_Registers *cop0)
{
    u8 mode = cop0->status.KSU;
    u32 ERL = cop0->status.ERL;
    u32 EXL = cop0->status.EXL;
   
    if (ERL || EXL) {
        cop0->status.KSU = __KERNEL_MODE;
        return;
    }
   
    switch (mode) {
        case __KERNEL_MODE:         cop0->status.KSU = __KERNEL_MODE; break;
        case __SUPERVISOR_MODE:     cop0->status.KSU = __SUPERVISOR_MODE; break;
        case __USER_MODE:           cop0->status.KSU = __USER_MODE; break;
    };
}

inline void 
cop0_status_write (COP0_Status *s, u32 data) 
{
    s->IE       = (data >> 0) & 0x1;
    s->EXL      = (data >> 1) & 0x1;
    s->ERL      = (data >> 2) & 0x1;
    s->KSU      = (data >> 3) & 0x1;
    s->IM_2     = (data >> 10) & 0x1;
    s->IM_3     = (data >> 11) & 0x1;
    s->BEM      = (data >> 12) & 0x1;
    s->IM_7     = (data >> 15) & 0x1;
    s->EIE      = (data >> 16) & 0x1;
    s->EDI      = (data >> 17) & 0x1;
    s->CH       = (data >> 18) & 0x1;
    s->BEV      = (data >> 22) & 0x1;
    s->DEV      = (data >> 23) & 0x1;
    s->CU       = (data >> 28) & 0xF;
    s->value    = data;
}

inline void 
cop0_cause_write (COP0_Cause *c, u32 data) 
{
    c->ex_code          = (data >> 2)  & 0x1F; 
    c->int0_pending     = (data >> 10) & 0x1;
    c->int1_pending     = (data >> 11) & 0x1;
    c->timer_pending    = (data >> 15) & 0x1;
    c->EXC2             = (data >> 16) & 0x3;
    c->CE               = (data >> 28) & 0x3;
    c->BD2              = (data >> 30) & 0x1;
    c->BD               = (data >> 31) & 0x1;
    c->value            = data;
}

inline void
cop0_timer_compare_check (R5900_Core *ee)
{
    bool are_equal = ee->cop0.regs[9] == ee->cop0.regs[11];
    if (are_equal) {
        ee->cop0.cause.timer_pending = 1;
        ee->cop0.status.IM_7 = 1;
    }
}

void 
r5900_cycle(R5900_Core *ee) 
{
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

    ee->pc              += 4;
    ee->cop0.regs[9]    += 1;
    ee->reg.r[0].SD[0]  = 0;

    cop0_status_write(&ee->cop0.status, ee->cop0.regs[12]);
    cop0_cause_write(&ee->cop0.cause, ee->cop0.regs[13]);
    set_kernel_mode(&ee->cop0);
}

void 
r5900_shutdown()
{
    free(_scratchpad_);
    fclose(dis);
    console.close();
}