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

// @Incomplete: This is not verified to work but Im putting it here anyways
#define SignExtend(x)
#define ZeroExtend(x)

//std::array<TLB_Entry, 48> TLBS;
FILE* dis = fopen("disasm.txt", "w+");

std::ofstream console("disasm.txt", std::ios::out);

static u8 *_icache_         = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));
static u8 *_dcache_         = (u8 *)malloc(sizeof(u8) * KILOBYTES(8));
static u8 *_scratchpad_     = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));

 //Range SCRATCHPAD = Range(0x70000000, KILOBYTES(16));

typedef struct Instruction_t {
    u8 opcode;
    u8 rd;
    u8 rt;
    u8 rs;
    u8 base;
    u32 sa;

    u16 imm;
    s16 sign_imm;
    u16 offset;
    s16 sign_offset;
    u32 instr_index;
} Instruction;

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
    // if (SCRATCHPAD.contains(address))
    if (address >= 0x70000000 && address < 0x70004000)
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
    // if (SCRATCHPAD.contains(address))
    if (address >= 0x70000000 && address < 0x70004000)
        return *(uint16_t*)&_scratchpad_[address & 0x3FFF];
    if (address >= 0x30100000 && address < 0x31FFFFFF) // uncached and accelerated ram
        address -= 0x10000000;// move to unaccelerated ram

    address &= 0x1FFFFFFF;
    
    return ee_load_16(address);
}

static inline u32 
ee_core_load_32 (u32 address) 
{
    // if (SCRATCHPAD.contains(address))
    if (address >= 0x70000000 && address < 0x70004000)
        return *(uint32_t*)&_scratchpad_[address & 0x3FFF];
    if (address >= 0x30100000 && address < 0x31FFFFFF) // uncached and accelerated ram
        address -= 0x10000000; // move to unaccelerated ram

    address &= 0x1FFFFFFF;
    
    return ee_load_32(address);
}

static inline u64 
ee_core_load_64 (u32 address) 
{
    // if (SCRATCHPAD.contains(address))
    if (address >= 0x70000000 && address < 0x70004000)
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
    // if (SCRATCHPAD.contains(address)) {
    if (address >= 0x70000000 && address < 0x70004000) {
        *(uint8_t*)&_scratchpad_[address & 0x3FFF] = value;
        return;
    }

    address &= 0x1FFFFFFF;
    ee_store_8(address, value);
}

static inline void 
ee_core_store_16 (u32 address, u16 value) 
{
    // if (SCRATCHPAD.contains(address)) {
    if (address >= 0x70000000 && address < 0x70004000) {
        *(uint16_t*)&_scratchpad_[address & 0x3FFF] = value;
        return;
    }

    address &= 0x1FFFFFFF;
    ee_store_16(address, value);
}

static inline void 
ee_core_store_32 (u32 address, u32 value) 
{
    // if (SCRATCHPAD.contains(address)) {
    if (address >= 0x70000000 && address < 0x70004000) {
        *(uint32_t*)&_scratchpad_[address & 0x3FFF] = value;
        return;
    }

    address &= 0x1FFFFFFF;
    ee_store_32(address, value);
}

static inline void 
ee_core_store_64 (u32 address, u64 value) 
{
    // if (SCRATCHPAD.contains(address)) {
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
        ee->pc = handle_exception_level_1(&ee->cop0, &exc, ee->pc, ee->is_branching);
    }
}

static inline void 
LUI (R5900_Core *ee, Instruction *i) 
{
    s64 imm                 = (s64)(s32)((i->imm) << 16);
    ee->reg.r[i->rt].UD[0]  = imm;

    intlog("LUI [{:d}] [{:#x}]\n", i->rt, imm);
    // printf("LUI [{%d}] [{%#08x}]\n", rt, imm);
}

static inline void 
LB (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr = ee->reg.r[i->rs].UW[0] + (s32)i->sign_offset;

    ee->reg.r[i->rt].SD[0] = (s64)ee_core_load_8(vaddr);
    intlog("LB [{:d}] [{:#x}] [{:d}] \n", i->rt, (s32)i->sign_offset, i->rs);  
}

static inline void 
LBU (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr = ee->reg.r[i->rs].UW[0] + (s32)i->sign_offset;
    
    ee->reg.r[i->rt].UD[0] = (u64)ee_core_load_8(vaddr);
    intlog("LBU [{:d}] [{:#x}] [{:d}] \n", i->rt, (s32)i->sign_offset, i->rs);    
}

static inline void
LH (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr = ee->reg.r[i->rs].UW[0] + (s32)i->sign_offset;
    
    check_address_error_exception(ee, _HALF, vaddr);

    ee->reg.r[i->rt].SD[0] = (s64)ee_core_load_16(vaddr);
    intlog("LH [{:d}] [{:#x}] [{:d}] \n", i->rt, (s32)i->sign_offset, i->rs);    
}

static inline void
LHU (R5900_Core *ee, Instruction *i)
{
    u32 vaddr = ee->reg.r[i->rs].UW[0] + (s32)i->sign_offset;

    check_address_error_exception(ee, _HALF, vaddr);

    ee->reg.r[i->rt].UD[0] = (u64)ee_core_load_16(vaddr);
    intlog("LHU [{:d}] [{:#x}] [{:d}] \n", i->rt, (s32)i->sign_offset, i->rs);
}

static inline void 
LW (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr = ee->reg.r[i->rs].UW[0] + (s32)i->sign_offset;
    
    check_address_error_exception(ee, _WORD, vaddr);

    ee->reg.r[i->rt].SD[0] = (s64)ee_core_load_32(vaddr);
    intlog("LW [{:d}] [{:#x}] [{:d}] \n", i->rt, (s32)i->sign_offset, i->rs);
    // printf("LW [{%d}] [{%#08x}] [{%d}] \n", rt, (s32)i->sign_offset, base);
}

static inline void 
LWC1 (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr = ee->reg.r[i->rs].UW[0] + (s32)i->sign_offset;
    
    check_address_error_exception(ee, _WORD, vaddr);

    u32 data = (u32)ee_core_load_32(vaddr);

    cop1_setFPR(i->rt, data);
    intlog("LWC1 [{:d}] [{:#x}] [{:d}] \n", i->rt, (s32)i->sign_offset, i->rs);
}

static inline void 
LWU (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr = ee->reg.r[i->rs].UW[0] + (s32)i->sign_offset;

    check_address_error_exception(ee, _WORD, vaddr);
   
    ee->reg.r[i->rt].SD[0] = (u64)ee_core_load_32(vaddr);
    intlog("LWU [{:d}] [{:#x}] [{:d}] \n", i->rt, (s32)i->sign_offset, i->rs);
}

static inline void 
LD (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr = ee->reg.r[i->rs].UW[0] + (s32)i->sign_offset;
    
    check_address_error_exception(ee, _DOUBLE, vaddr);

    ee->reg.r[i->rt].UD[0] = ee_core_load_64(vaddr);
    intlog("LD [{:d}] [{:#x}] [{:d}]\n", i->rt, (s32)i->sign_offset, i->rs);
}

/* This is an interesting and clear implemtation from Dobiestation */
static const u64 LDL_MASK[8] =
{   0x00ffffffffffffffULL, 0x0000ffffffffffffULL, 0x000000ffffffffffULL, 0x00000000ffffffffULL,
    0x0000000000ffffffULL, 0x000000000000ffffULL, 0x00000000000000ffULL, 0x0000000000000000ULL
};

static const u8 LDL_SHIFT[8] = {56, 48, 40, 32, 24, 16, 8, 0};

static const u64 LDR_MASK[8] = 
{
    0x0000000000000000ULL, 0xff00000000000000ULL, 0xffff000000000000ULL, 0xffffff0000000000ULL, 
    0xffffffff00000000ULL, 0xffffffffff000000ULL, 0xffffffffffff0000ULL, 0xffffffffffffff00ULL      
};

static const u8 LDR_SHIFT[8] = {0, 8, 16, 24, 32, 40, 48, 56};

static inline void 
LDL (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr           = ee->reg.r[i->rs].UW[0] + (s32)i->sign_offset;
    u32 aligned_vaddr   = vaddr & ~0x7;
    u32 shift           = vaddr & 0x7;
    
    u64 aligned_dword   = ee_core_load_64(aligned_vaddr);
    u64 result          = (ee->reg.r[i->rt].UD[0] & LDL_MASK[shift] | aligned_vaddr << LDL_SHIFT[shift]);
    ee->reg.r[i->rt].UD[0] = result;
    intlog("LDL [{:d}] [{:#x}] [{:d}]\n", i->rt, (s32)i->sign_offset, i->rs);
}

static inline void 
LDR (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr           = ee->reg.r[i->rs].UW[0] + (s32)i->sign_offset;
    u32 aligned_vaddr   = vaddr & ~0x7;
    u32 shift           = vaddr & 0x7;
    
    u64 aligned_dword   = ee_core_load_64(aligned_vaddr);
    u64 result          = (ee->reg.r[i->rt].UD[0] & LDR_MASK[shift] | aligned_vaddr << LDR_SHIFT[shift]);
    ee->reg.r[i->rt].UD[0] = result;
    intlog("LDR [{:d}] [{:#x}] [{:d}]\n", i->rt, (s32)i->sign_offset, i->rs);
    printf("LDR [{%d}] [{%#x}] [{%d}]\n", i->rt, (s32)i->sign_offset, i->rs);
}

static inline void 
LQ (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr   = ee->reg.r[i->rs].UW[0] + (s32)i->sign_offset;
    
    check_address_error_exception(ee, _QUAD, vaddr);

    ee->reg.r[i->rt].UD[0] = ee_core_load_64(vaddr);
    ee->reg.r[i->rt].UD[1] = ee_core_load_64(vaddr + 8);
    // intlog("LQ [{:d}] [{:#x}] [{:d}]\n", i->rt, (s32)i->sign_offset, i->rs);
}

static inline void 
SB (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr   = ee->reg.r[i->rs].UW[0] + (s32)i->sign_offset;
    s8 value    = ee->reg.r[i->rt].SB[0];

    ee_core_store_8(vaddr, value);
    intlog("SB [{:d}] [{:#x}] [{:d}] \n", i->rt, (s32)i->sign_offset, i->rs);
}

static inline void 
SH (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr   = ee->reg.r[i->rs].UW[0] + (s32)i->sign_offset;
    s16 value   = ee->reg.r[i->rt].SH[0];

    check_address_error_exception(ee, _HALF, vaddr);

    ee_core_store_16(vaddr, value);
    intlog("SH [{:d}] [{:#x}] [{:d}] \n", i->rt, (s32)i->sign_offset, i->rs); 
}

static inline void 
SW (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr = ee->reg.r[i->rs].UW[0] + (s32)i->sign_offset;
    s32 value = ee->reg.r[i->rt].SW[0];
    
    check_address_error_exception(ee, _WORD, vaddr);

    ee_core_store_32(vaddr, value);
    intlog("SW [{:d}] [{:#x}] [{:d}] \n", i->rt, i->sign_offset, i->rs);
    // printf("SW [{%d}] [{%#08x}] [{%d}] \n", i->rt, (s32)i->sign_offset, i->rs);
}

static inline void 
SWC1 (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr = ee->reg.r[i->rs].UW[0] + (s32)i->sign_offset;
    
    check_address_error_exception(ee, _WORD, vaddr);

    ee_core_store_32(vaddr, cop1_getFPR(i->rt));
    intlog("SWC1 [{:d}] [{:#x}] [{:d}] \n", i->rt, (s32)i->sign_offset, i->rs); 
}

static inline void 
SD (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr   = ee->reg.r[i->rs].UW[0] + (s32)i->sign_offset;
    u64 value   = ee->reg.r[i->rt].UD[0];
  
    check_address_error_exception(ee, _DOUBLE, vaddr);

    ee_core_store_64(vaddr, value);   
 
    // intlog("SD [{:d}] [{:#x}] [{:d}] \n", i->rt, (s32)i->sign_offset, i->rs);
    // printf("SD [{%d}] [{%#08x}] [{%d}] \n", i->rt, (s32)i->sign_offset, i->rs);
}

/* This is an interesting and clear implemtation from Dobiestation */
static const u64 SDL_MASK[8] =
{   0x00ffffffffffffffULL, 0x0000ffffffffffffULL, 0x000000ffffffffffULL, 0x00000000ffffffffULL,
    0x0000000000ffffffULL, 0x000000000000ffffULL, 0x00000000000000ffULL, 0x0000000000000000ULL
};

static const u8 SDL_SHIFT[8] = {56, 48, 40, 32, 24, 16, 8, 0};

static const u64 SDR_MASK[8] = 
{
    0x0000000000000000ULL, 0xff00000000000000ULL, 0xffff000000000000ULL, 0xffffff0000000000ULL, 
    0xffffffff00000000ULL, 0xffffffffff000000ULL, 0xffffffffffff0000ULL, 0xffffffffffffff00ULL      
};

static const u8 SDR_SHIFT[8] = {0, 8, 16, 24, 32, 40, 48, 56};

static inline void 
SDL (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr           = ee->reg.r[i->rs].UW[0] + i->sign_offset;
    u32 aligned_vaddr   = vaddr & ~0x7;
    u32 shift           = vaddr & 0x7;
    
    u64 aligned_dword   = ee_core_load_64(aligned_vaddr);
    u64 result          = (ee->reg.r[i->rt].UD[0] & SDL_MASK[shift] | aligned_vaddr << SDL_SHIFT[shift]);
    ee_core_store_64(aligned_vaddr, result);
    intlog("SDL [{:d}] [{:#x}] [{:d}]\n", i->rt, i->sign_offset, i->rs);
    printf("SDL [{%d}] [{%#x}] [{%d}]\n", i->rt, i->sign_offset, i->rs);
}

static inline void 
SDR (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr           = ee->reg.r[i->rs].UW[0] + i->sign_offset;
    u32 aligned_vaddr   = vaddr & ~0x7;
    u32 shift           = vaddr & 0x7;
    
    u64 aligned_dword   = ee_core_load_64(aligned_vaddr);
    u64 result          = (ee->reg.r[i->rt].UD[0] & SDR_MASK[shift] | aligned_vaddr << SDR_SHIFT[shift]);
    ee_core_store_64(aligned_vaddr, result);
    intlog("SDR [{:d}] [{:#x}] [{:d}]\n", i->rt, i->sign_offset, i->rs);
    printf("SDR [{%d}] [{%#x}] [{%d}]\n", i->rt, i->sign_offset, i->rs);
}

//@@Incomplete: Change this to using uint128 data structure
static inline void 
SQ (R5900_Core *ee, Instruction *i) 
{
    u32 vaddr   = ee->reg.r[i->rs].UW[0] + i->sign_offset;
    u64 lov     = ee->reg.r[i->rt].UD[0];
    u64 hiv     = ee->reg.r[i->rt].UD[1];
   
    check_address_error_exception(ee, _QUAD, vaddr);

    ee_core_store_64(vaddr, lov);       
    ee_core_store_64(vaddr + 8, hiv);       
    // intlog("SQ [{:d}] [{:#x}] [{:d}] \n", i->rt, i->sign_offset, base);
}

/*******************************************
* Computational ALU Instructions
* Arithmetic
• Logical
• Shift
• Multiply
• Divide
*******************************************/
static inline void add_overflow() {return;}
static inline void add_overflow64() {return;}
static inline void sub_overflow() {return;}
static inline void sub_overflow64() {return;}

static inline void 
ADD (R5900_Core *ee, Instruction *i) 
{
    // @@Incomplete: The add instruction must signal an exception on overflow
    // but we have no overflow detection for now
    int temp                = ee->reg.r[i->rs].SW[0] + ee->reg.r[i->rt].SW[0];
    ee->reg.r[i->rd].SD[0]  = (s64)temp;
    intlog("ADD [{:d}] [{:d}] [{:d}]\n", i->rd, i->rs, i->rt);   
}

static inline void 
ADDU (R5900_Core *ee, Instruction *i) 
{
    int32_t result          = ee->reg.r[i->rs].SW[0] + ee->reg.r[i->rt].SW[0];
    ee->reg.r[i->rd].UD[0]  = result;
    intlog("ADDU [{:d}] [{:d}] [{:d}]\n", i->rd, i->rs, i->rt);
}

static inline void 
ADDI (R5900_Core *ee, Instruction *i) 
{
    s32 result              = ee->reg.r[i->rs].SD[0] + i->sign_imm;
    ee->reg.r[i->rt].SD[0]  = result;
    /* @@Incomplete: No 2 complement Arithmetic Overflow error implementation*/
    intlog("ADDI: [{:d}] [{:d}] [{:#x}] \n", i->rt, i->rs, i->sign_imm);
}

static inline void 
ADDIU (R5900_Core *ee, Instruction *i) 
{
    s32 result              = ee->reg.r[i->rs].SD[0] + i->sign_imm;
    ee->reg.r[i->rt].SD[0]  = result;
    intlog("ADDIU: [{:d}] [{:d}] [{:#x}] \n", i->rt, i->rs, i->sign_imm);
}

static inline void 
DADDU (R5900_Core *ee, Instruction *i) 
{
    ee->reg.r[i->rd].UD[0] = ee->reg.r[i->rs].SD[0] + ee->reg.r[i->rt].SD[0];
    intlog("DADDU [{:d}] [{:d}] [{:d}]\n", i->rd, i->rs, i->rt);  
}

static inline void
DADDIU (R5900_Core *ee, Instruction *i)
{
    ee->reg.r[i->rt].UD[0] = ee->reg.r[i->rs].SD[0] + i->sign_imm;
    intlog("DADDIU [{:d}] [{:d}] [{:#x}]\n", i->rt, i->rs, i->sign_imm);  
}

static inline void 
SUB (R5900_Core *ee, Instruction *i) 
{
    s32 result              = ee->reg.r[i->rs].SW[0] - ee->reg.r[i->rt].SW[0];
    ee->reg.r[i->rd].UD[0]  = (s64)result;

    /* @@Incomplete: No 2 complement Arithmetic Overflow error implementation*/
    intlog("SUB [{:d}] [{:d}] [{:d}]\n", i->rd, i->rs, i->rt);    
}

static inline void 
SUBU (R5900_Core *ee, Instruction *i) 
{
    s32 result              = ee->reg.r[i->rs].SW[0] - ee->reg.r[i->rt].SW[0];
    ee->reg.r[i->rd].UD[0]  = (u64)result;
    intlog("SUBU [{:d}] [{:d}] [{:d}]\n", i->rd, i->rs, i->rt);    
}

static inline void 
MULT (R5900_Core *ee, Instruction *i) 
{
    s64 w1 = (s64)ee->reg.r[i->rs].SW[0];
    s64 w2 = (s64)ee->reg.r[i->rt].SW[0];
    s64 prod = (w1 * w2);
    
    ee->LO = (s32)(prod & 0xFFFFFFFF);
    ee->HI = (s32)(prod >> 32);
    // @@Note: C790 apparently states that the chip does not always call mflo everytime
    // during a 3 operand mult
    // But we could always use mflo because of the pipeline interlock 
    ee->reg.r[i->rd].SD[0] = ee->LO;

    intlog("MULT [{:d}] [{:d}] [{:d}]\n", i->rs, i->rt, i->rd);
}

static inline void
MULT1 (R5900_Core *ee, Instruction *i)
{
    s32 w1 = (s32)ee->reg.r[i->rs].SW[0];
    s32 w2 = (s32)ee->reg.r[i->rt].SW[0];
    s64 prod = (s64)(w1 * w2);
    
    ee->LO1 = (s32)(prod & 0xFFFFFFFF);
    ee->HI1 = (s32)(prod >> 32);
    // @@Note: C790 apparently states that the chip does not always call mflo everytime
    // during a 3-operand mult
    // But we could always use mflo because of the pipeline interlock 
    ee->reg.r[i->rd].SD[0] = ee->LO1;

    intlog("MULT1 [{:d}] [{:d}] [{:d}]\n", i->rs, i->rt, i->rd);
}

static inline void 
MADDU (R5900_Core *ee, u32 instruction)
{
    errlog("LOL\n");
    // intlog("MADDU\n");
    printf("lol");
}

static inline void
DIV (R5900_Core *ee,Instruction *i)
{
    if (ee->reg.r[i->rt].UD == 0) {
        ee->LO = (int)0xffffffff;
        ee->HI = ee->reg.r[i->rs].UD[0];
        errlog("[ERROR]: Tried to Divide by zero\n");
        //return;
    }
    s32 d = ee->reg.r[i->rs].SW[0] / ee->reg.r[i->rt].SW[0];
    s32 q = ee->reg.r[i->rs].SW[0] % ee->reg.r[i->rt].SW[0];

    ee->LO = (s64)d;
    ee->HI = (s64)q;
    intlog("DIV [{:d}] [{:d}]\n", i->rs, i->rt);
}

static inline void 
DIVU (R5900_Core *ee, Instruction *i) 
{
    s64 w1 = (s64)ee->reg.r[i->rs].SW[0];
    s64 w2 = (s64)ee->reg.r[i->rt].SW[0];

    if (w2 == 0) {
        errlog("[ERROR] tried to divide by 0, result is Undefined\n");
        //return;
    } 
    // @@Note: Sign extend by 64?
    s64 q = (s32)(w1 / w2);
    ee->LO = q;
    s64 r = (s32)(w1 % w2);
    ee->HI = r;

    intlog("DIVU [{:d}] [{:d}]\n", i->rs, i->rt);
}

static inline void
DIVU1 (R5900_Core *ee,Instruction *i)
{
    s32 w1 = ee->reg.r[i->rs].SW[0];
    s32 w2 = ee->reg.r[i->rt].SW[0];

    if (w2 == 0) {
        ee->LO1 = (int)0xffffffff;
        ee->HI1 = ee->reg.r[i->rs].UD[0];
        errlog("[ERROR] tried to divide by 0, result is Undefined\n");
        //exit(1);
        //return;
    } 

    s64 q  = (w1 / w2);
    ee->LO1 = q;
    s64 r  = (w1 % w2);
    ee->HI1 = r;

    intlog("DIVU_1 [{:d}] [{:d}]\n", i->rs, i->rt);
}
/*****************************

******************************/
static inline void 
AND (R5900_Core *ee, Instruction *i) 
{
    ee->reg.r[i->rd].UD[0] = ee->reg.r[i->rs].UD[0] & ee->reg.r[i->rt].UD[0];
    intlog("AND [{:d}] [{:d}] [{:d}] \n", i->rd, i->rs, i->rt);
}

static inline void 
ANDI (R5900_Core *ee, Instruction *i) 
{
    ee->reg.r[i->rt].UD[0] = ee->reg.r[i->rs].UD[0] & (u64)i->imm; 
    intlog("ANDI: [{:d}] [{:d}] [{:#x}] \n", i->rt, i->rs, (u64)i->imm);
}

static inline void 
OR (R5900_Core *ee, Instruction *i) 
{
    ee->reg.r[i->rd].SD[0] = ee->reg.r[i->rs].SD[0] | ee->reg.r[i->rt].SD[0];
    intlog("OR [{:d}] [{:d}] [{:d}]\n", i->rd, i->rs, i->rt);
}

static inline void 
ORI (R5900_Core *ee, Instruction *i) 
{
    ee->reg.r[i->rt].UD[0] = ee->reg.r[i->rs].UD[0] | (u64)i->imm; 
    intlog("ORI: [{:d}] [{:d}] [{:#x}] \n", i->rt, i->rs, (u64)i->imm);
}

/* @Temporary: No XOR instruction? */
static inline void
XORI (R5900_Core *ee, Instruction *i)
{
    ee->reg.r[i->rt].UD[0] = ee->reg.r[i->rs].UD[0] ^ (u64)i->imm;
    intlog("XORI [{:d}] [{:d}] [{:#x}] \n", i->rt, i->rs, (u64)i->imm);
}

static inline void 
NOR (R5900_Core *ee, Instruction *i) 
{
    ee->reg.r[i->rd].SD[0] = ~(ee->reg.r[i->rs].SD[0] | ee->reg.r[i->rt].SD[0]);
    // assert(1);
    intlog("NOR [{:d}] [{:d}] [{:d}]\n", i->rd, i->rs, i->rt);
}

static inline void 
SLL (R5900_Core *ee, Instruction *i) 
{
    u32 sa                  = i->sa;
    ee->reg.r[i->rd].SD[0]  = (u64)((s32)ee->reg.r[i->rt].UW[0] << i->sa);

    intlog("SLL source: [{:d}] dest: [{:d}] [{:#x}]\n", i->rd, i->rt, i->sa);
}

static inline void 
SLLV (R5900_Core *ee, Instruction *i) 
{
    u32 sa                  = ee->reg.r[i->rs].SW[0] & 0x1F;
    ee->reg.r[i->rd].SD[0]  = (s64)(ee->reg.r[i->rt].SW[0] << sa);

    intlog("SLLV source: [{:d}] dest: [{:d}] [{:#x}]\n", i->rd, i->rt, sa);
}

static inline void 
DSLL (R5900_Core *ee, Instruction *i) 
{
    ee->reg.r[i->rd].UD[0] = ee->reg.r[i->rt].UD[0] << i->sa;
    intlog("DSLL source: [{:d}] dest: [{:d}] [{:#x}]\n", i->rd, i->rt, i->sa);
}

static inline void
DSLLV (R5900_Core *ee, Instruction *i)
{
    s32 sa                  = ee->reg.r[i->rs].UW[0] & 0x3F;
    ee->reg.r[i->rd].UD[0]  = ee->reg.r[i->rt].UD[0] << sa;
    intlog("DSLLV [{:d}] [{:d}] [{:d}]\n", i->rd, i->rt, i->rs); 
}

static inline void 
SRL (R5900_Core *ee, Instruction *i) 
{
    u32 result              = (ee->reg.r[i->rt].UW[0] >> i->sa);
    ee->reg.r[i->rd].SD[0]  = (s32)result;

    intlog("SRL source: [{:d}] dest: [{:d}] [{:#x}]\n", i->rd, i->rt, i->sa);
}

static inline void 
SRLV (R5900_Core *ee, Instruction *i) 
{
    u32 sa                  = ee->reg.r[i->rs].SW[0] & 0x1F;
    ee->reg.r[i->rd].SD[0]  = (s64)(ee->reg.r[i->rt].SW[0] >> sa);

    intlog("SRLV source: [{:d}] dest: [{:d}] [{:#x}]\n", i->rd, i->rt, sa);
}

static inline void 
SRA (R5900_Core *ee, Instruction *i) 
{
    s32 result              = (ee->reg.r[i->rt].SW[0]) >> (s32)i->sa;
    ee->reg.r[i->rd].SD[0]  = result;

    intlog("SRA source: [{:d}] dest: [{:d}] [{:#x}]\n", i->rd, i->rt, (s32)i->sa);
}

static inline void
SRAV (R5900_Core *ee, Instruction *i)
{
    u32 sa                  = ee->reg.r[i->rs].UB[0] & 0x1F;
    s32 result              = ee->reg.r[i->rt].SW[0] >> sa;
    ee->reg.r[i->rd].SD[0]  = (s64)result;

    intlog("SRAV [{:d}] [{:d}] [{:d}]\n", i->rd, i->rt, i->rs);
}

static inline void 
DSRAV (R5900_Core *ee, Instruction *i)
{
    u32 sa                  = ee->reg.r[i->rs].UW[0] & 0x3F;
    ee->reg.r[i->rd].SD[0]  = ee->reg.r[i->rt].SD[0] >> sa;

    intlog("DSRAV [{:d}] [{:d}] [{:d}]\n", i->rd, i->rt, i->rs);
}

static inline void 
DSRA32 (R5900_Core *ee, Instruction *i) 
{
    u32 s                   = i->sa + 32;
    s64 result              = ee->reg.r[i->rt].SD[0] >> s;
    ee->reg.r[i->rd].SD[0]  = result;

    intlog("DSRA32 source: [{:d}] dest: [{:d}] s: [{:#x}] \n", i->rd, i->rt, s);
}

static inline void 
DSLL32 (R5900_Core *ee, Instruction *i) 
{
    u32 s                   = i->sa + 32;
    ee->reg.r[i->rd].UD[0]  = ee->reg.r[i->rt].UD[0] << s;

    intlog("DSLL32 source: [{:d}] dest: [{:d}] s: [{:#x}] \n", i->rd, i->rt, s);
}

static inline void 
DSRL (R5900_Core *ee, Instruction *i) 
{
    ee->reg.r[i->rd].UD[0] = ee->reg.r[i->rt].UD[0] >> i->sa;

    intlog("DSRL source: [{:d}] dest: [{:d}] s: [{:#x}] \n", i->rd, i->rt, i->sa);
}

static inline void 
DSRL32 (R5900_Core *ee, Instruction *i) 
{
    u32 s                   = i->sa + 32;
    ee->reg.r[i->rd].UD[0]  = ee->reg.r[i->rt].UD[0] >> s;

    intlog("DSRL32 source: [{:d}] dest: [{:d}] s: [{:#x}] \n", i->rd, i->rt, s);
}

static inline void 
SLT (R5900_Core *ee, Instruction *i) 
{
    s32 result              = ee->reg.r[i->rs].SD[0] < ee->reg.r[i->rt].SD[0] ? 1 : 0;
    ee->reg.r[i->rd].SD[0]  = result;
    intlog("SLT [{:d}] [{:d}] [{:d}]\n", i->rd, i->rs, i->rt);
}

static inline void 
SLTU (R5900_Core *ee, Instruction *i) 
{
    u32 result              = (ee->reg.r[i->rs].UD[0] < ee->reg.r[i->rt].UD[0]) ? 1 : 0;
    ee->reg.r[i->rd].UD[0]  = result;
    
    intlog("SLTU [{:d}] [{:d}] [{:d}]\n", i->rd, i->rs, i->rt);
}

static inline void 
SLTI (R5900_Core *ee, Instruction *i) 
{
    s32 result              = (ee->reg.r[i->rs].SD[0] < (s32)i->sign_imm) ? 1 : 0;
    ee->reg.r[i->rt].SD[0]  = result;
    
    intlog("SLTI [{:d}] [{:d}], [{:#x}]\n", i->rt, i->rs, i->sign_imm);
}

static inline void 
SLTIU (R5900_Core *ee, Instruction *i) 
{
    u32 result              = (ee->reg.r[i->rs].UD[0] < (u64)i->sign_imm) ? 1 : 0;
    ee->reg.r[i->rt].UD[0]  = result;
    
    intlog("SLTIU [{:d}] [{:d}] [{:#x}]\n", i->rt, i->rs, (u64)i->sign_imm);
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

static inline void 
J (R5900_Core *ee, Instruction *i) 
{
    u32 offset = ((ee->pc + 4) & 0xF0000000) + (i->instr_index << 2);
    jump_to(ee, offset);
    intlog("J [{:#x}]\n", offset);
}

static inline void 
JR(R5900_Core *ee, Instruction *i) 
{
    // @TODO: Check if the LSB is 0
    jump_to(ee, ee->reg.r[i->rs].UW[0]);
    intlog("JR source: [{:d}] pc_dest: [{:#x}]\n", i->rs, ee->reg.r[i->rs].UW[0]);
}

static inline void 
JAL(R5900_Core *ee, Instruction *i) 
{
    u32 instr_index     = i->instr_index;
    u32 target_address  = ((ee->pc + 4) & 0xF0000000) + (instr_index << 2);

    jump_to(ee, target_address); 
    ee->reg.r[31].UD[0]  = ee->pc + 8;

    intlog("JAL [{:#x}] \n", target_address);   
}

static inline void 
JALR(R5900_Core *ee, Instruction *i) 
{
    u32 return_addr = ee->pc + 8;

    if (i->rd != 0) {
        ee->reg.r[i->rd].UD[0] = return_addr;
    } else {
        ee->reg.r[31].UD[0] = return_addr;
    }

    jump_to(ee, ee->reg.r[i->rs].UW[0]);

    intlog("JALR [{:d}]\n", i->rs);
}

static inline void 
BNE(R5900_Core *ee, Instruction *i) 
{
    s32 imm         = i->sign_imm << 2;
    bool condition  = ee->reg.r[i->rs].SD[0] != ee->reg.r[i->rt].SD[0];

    branch(ee, condition, imm);

    intlog("BNE [{:d}] [{:d}], [{:#x}]\n", i->rt, i->rs, imm);
}

static inline void 
BEQ(R5900_Core *ee, Instruction *i) 
{
    s32 imm         = i->sign_imm << 2;
    bool condition  = ee->reg.r[i->rs].SD[0] == ee->reg.r[i->rt].SD[0];

    branch(ee, condition, imm);

    intlog("BEQ [{:d}] [{:d}] [{:#x}] \n", i->rs, i->rt, imm);
}

static inline void 
BEQL(R5900_Core *ee, Instruction *i) 
{
    s32 imm         = i->sign_imm << 2;
    bool condition  = ee->reg.r[i->rs].SD[0] == ee->reg.r[i->rt].SD[0];

    branch_likely(ee, condition, imm);

    intlog("BEQL [{:d}] [{:d}] [{:#x}]\n", i->rs, i->rt, imm);    
}

static inline void 
BNEL(R5900_Core *ee, Instruction *i) 
{
    s32 imm         = i->sign_imm << 2;
    bool condition  = ee->reg.r[i->rs].SD[0] != ee->reg.r[i->rt].SD[0];

    branch_likely(ee, condition, imm);

    intlog("BNEL [{:d}] [{:d}] [{:#x}]\n", i->rs, i->rt, imm);
}

static inline void 
BLEZ (R5900_Core *ee, Instruction *i) 
{
    s32 offset      = (i->sign_offset << 2);
    bool condition  = ee->reg.r[i->rs].SD[0] <= 0;

    branch(ee, condition, offset);

    intlog("BLEZ [{:d}] [{:#x}]\n", i->rs, offset); 
}

static inline void 
BLEZL (R5900_Core *ee, Instruction *i) 
{
    s32 offset      = (i->sign_offset << 2);
    bool condition  = ee->reg.r[i->rs].SD[0] <= 0;
    
    branch_likely(ee, condition, offset);
    
    intlog("BLEZ [{:d}] [{:#x}]\n", i->rs, offset); 
}

static inline void 
BLTZ (R5900_Core *ee, Instruction *i) 
{
    s32 offset      = i->sign_offset << 2;
    bool condition  = ee->reg.r[i->rs].SD[0] < 0;
    
    branch(ee, condition, offset);

    intlog("BLTZ [{:d}] [{:#x}] \n", i->rs, offset);
}

static inline void
BGTZ (R5900_Core *ee, Instruction *i)
{
    s32 offset      = i->sign_offset << 2;
    bool condition  = ee->reg.r[i->rs].SD[0] > 0;

    branch(ee, condition, offset);
    
    intlog("BGTZ [{:d}] [{:#x}] \n", i->rs, offset);
}

static inline void 
BGEZ (R5900_Core *ee, Instruction *i) 
{
    s32 offset      = i->sign_offset << 2;
    bool condition  = ee->reg.r[i->rs].SD[0] >= 0;

    branch(ee, condition, offset);

    intlog("BGEZ [{:d}] [{:#x}] \n", i->rs, offset);    
}

/*******************************************
 * Miscellaneous Instructions
*******************************************/
// @@Implementation We have no debugger yet so theres no need to implement this now
static inline void 
BREAKPOINT(R5900_Core *ee, u32 instruction) 
{
    // intlog("BREAKPOINT\n");
}

/*******************************************
 * SYNCHRONOUS Instructions
*******************************************/
// @@Note: Issued after every MTC0 write The PS2 possibly executes millions of memory loads / stores per second, 
// which is entirely feasible in hardware. But a complete pain in software as that would completely kill performance
// May be cool to implement this for accuracy since that is the purpose of the emulator
static inline void 
SYNC(R5900_Core *ee, u32 instruction) 
{
    // intlog("SYNC\n");
}

/*******************************************
 * COP0 Instructions
*******************************************/
static inline void 
MFC0 (R5900_Core *ee, Instruction *i) 
{
    u32 cop0            = i->rd;
    u32 gpr             = i->rt;
    ee->reg.r[gpr].SD[0] = ee->cop0.regs[cop0];

    // intlog("MFC0 GPR: [{:d}] COP0: [{:d}]\n", gpr, cop0);
}

static inline void 
MTC0 (R5900_Core *ee, Instruction *i) 
{
    u32 cop0             = i->rd;
    u32 gpr              = i->rt;

    if(cop0 == 12) {
        ee->cop0.status.IE    = (ee->reg.r[gpr].SW[0] >> 0) & 0x1;
        ee->cop0.status.EXL   = (ee->reg.r[gpr].SW[0] >> 1) & 0x1;
        ee->cop0.status.ERL   = (ee->reg.r[gpr].SW[0] >> 2) & 0x1;
        ee->cop0.status.KSU   = (ee->reg.r[gpr].SW[0] >> 3) & 0x1;
        ee->cop0.status.IM_2  = (ee->reg.r[gpr].SW[0] >> 10) & 0x1;
        ee->cop0.status.IM_3  = (ee->reg.r[gpr].SW[0] >> 11) & 0x1;
        ee->cop0.status.BEM   = (ee->reg.r[gpr].SW[0] >> 12) & 0x1;
        ee->cop0.status.IM_7  = (ee->reg.r[gpr].SW[0] >> 15) & 0x1;
        ee->cop0.status.EIE   = (ee->reg.r[gpr].SW[0] >> 16) & 0x1;
        ee->cop0.status.EDI   = (ee->reg.r[gpr].SW[0] >> 17) & 0x1;
        ee->cop0.status.CH    = (ee->reg.r[gpr].SW[0] >> 18) & 0x1;
        ee->cop0.status.BEV   = (ee->reg.r[gpr].SW[0] >> 22) & 0x1;
        ee->cop0.status.DEV   = (ee->reg.r[gpr].SW[0] >> 23) & 0x1;
        ee->cop0.status.CU    = (ee->reg.r[gpr].SW[0] >> 28) & 0xF;
    } else if (cop0 == 13) {
        ee->cop0.cause.ex_code       = (ee->reg.r[gpr].SW[0] >> 2)  & 0x1F; 
        ee->cop0.cause.int0_pending  = (ee->reg.r[gpr].SW[0] >> 10) & 0x1;
        ee->cop0.cause.int1_pending  = (ee->reg.r[gpr].SW[0] >> 11) & 0x1;
        ee->cop0.cause.timer_pending = (ee->reg.r[gpr].SW[0] >> 15) & 0x1;
        ee->cop0.cause.EXC2          = (ee->reg.r[gpr].SW[0] >> 16) & 0x3;
        ee->cop0.cause.CE            = (ee->reg.r[gpr].SW[0] >> 28) & 0x3;
        ee->cop0.cause.BD2           = (ee->reg.r[gpr].SW[0] >> 30) & 0x1;
        ee->cop0.cause.BD            = (ee->reg.r[gpr].SW[0] >> 31) & 0x1;
    } else {
        ee->cop0.regs[cop0] = ee->reg.r[gpr].SW[0];
    }     

    // intlog("MTC0 GPR: [{:d}] COP0: [{:d}]\n", gpr, cop0);
}

static inline void
ERET (R5900_Core *ee, u32 instruction) 
{
    //@@Removal @@Implementation: This could be shortened down a fair bit with just an mfc0 instruction
    ee->pc = ee->cop0.EPC;
}

// @@Implemtation: implement TLB
static inline void 
TLBWI (R5900_Core *ee, u32 instruction, int index) 
{
    //TLB_Entry current = TLBS.at(index);
    // intlog("TLBWI not yet implemented\n");
}

static inline void 
CACHE_IXIN (R5900_Core *ee, u32 instruction) 
{
    // intlog("Invalidate instruction cache");
}

#define BIOS_HLE
#ifndef BIOS_HLE
static void
SYSCALL (R5900_Core *ee, u32 instruction)
{
    printf("SYSCALL [%#02x]  ", ee->reg.r[3].UB[0]);
    Exception exc   = get_exception(V_COMMON, __SYSCALL);
    ee->pc          = handle_exception_level_1(&ee->cop0, &exc, ee->pc, ee->is_branching);
}

#else

/* 
From:   https://www.psdevwiki.com/ps2/Syscalls 
        https://psi-rockin.github.io/ps2tek/#bioseesyscalls
*/
static void
SYSCALL (R5900_Core *ee)
{
    //printf("SYSCALL [%#02x]  ", ee->reg.r[3].UB[0]);
    u32 syscall     = ee->reg.r[3].UB[0];
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
cop0_timer_compare_check (R5900_Core *ee)
{
    bool are_equal = ee->cop0.regs[9] == ee->cop0.regs[11];
    if (are_equal) {
        ee->cop0.cause.timer_pending = 1;
        ee->cop0.status.IM_7 = 1;
    }
}
/*******************************************
 * EE Instructions
*******************************************/
static inline void 
MOVN (R5900_Core *ee, Instruction *i)
{
    if (ee->reg.r[i->rt].UD[0] != 0) {
        ee->reg.r[i->rd].UD[0] = ee->reg.r[i->rs].UD[0];
    }
    intlog("MOVN [{:d}] [{:d}] [{:d}]\n", i->rd, i->rs, i->rt);
}

static inline void 
MOVZ (R5900_Core *ee, Instruction *i) 
{
    if (ee->reg.r[i->rt].SD[0] == 0) {
        ee->reg.r[i->rd].SD[0] = ee->reg.r[i->rs].SD[0];
    }

    intlog("MOVZ [{:d}] [{:d}] [{:d}]\n",i->rd, i->rs, i->rt);
}

static inline void 
MFLO (R5900_Core *ee, Instruction *i) 
{
    ee->reg.r[i->rd].UD[0] = ee->LO;
    intlog("MFLO [{:d}] \n", i->rd);
}

static inline void 
MFLO1 (R5900_Core *ee, Instruction *i)
{
    ee->reg.r[i->rd].UD[0] = ee->LO1;
    intlog("MFLO_1 [{:d}]\n", i->rd);
}

static inline void 
MFHI (R5900_Core *ee, Instruction *i) 
{
    ee->reg.r[i->rd].UD[0] = ee->HI;
    intlog("MFHI [{:d}]\n", i->rd);
}

static inline void
MTLO (R5900_Core *ee, Instruction *i) 
{
    ee->LO = ee->reg.r[i->rs].UD[0];
    intlog("MTLO [{:d}]\n", i->rs);
}

static inline void
MTHI (R5900_Core *ee, Instruction *i) 
{
    ee->HI = ee->reg.r[i->rs].UD[0];
    intlog("MTHI [{:d}]\n", i->rs);
}

void 
decode_and_execute (R5900_Core *ee, u32 instruction)
{    
    if (instruction == 0x00000000) return;
    
    Instruction instr = {
        .rd          = (u8)((instruction >> 11) & 0x1F),
        .rt          = (u8)((instruction >> 16) & 0x1F),
        .rs          = (u8)((instruction >> 21) & 0x1F),
        .sa          = (u32)((instruction >> 6)  & 0x1F),

        .imm         = (u16)(instruction & 0xFFFF),
        .sign_imm    = (s16)(instruction & 0xFFFF),
        .offset      = (u16)(instruction & 0xFFFF),
        .sign_offset = (s16)(instruction & 0xFFFF),
        .instr_index = (u32)(instruction & 0x3FFFFFF),
    };

    int opcode = instruction >> 26;
    switch (opcode) {
        case COP0:
        {
            int fmt = instruction >> 21 & 0x3F;
            switch (fmt) 
            {
                case 0x000000: MFC0(ee, &instr);  break;
                case 0x000004: MTC0(ee, &instr);  break;
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
                case 0x00000000:    SLL(ee, &instr);    break;
                case 0x0000003f:    DSRA32(ee, &instr); break;
                case 0x00000008:    JR(ee, &instr);     break;
                case 0x0000000f:    SYNC(ee, instruction);   break;
                case 0x00000009:    JALR(ee, &instr);   break;
                case 0x0000002d:    DADDU(ee, &instr);  break;
                case 0x00000025:    OR(ee, &instr);     break;
                case 0x00000018:    MULT(ee, &instr);   break;
                case 0x0000001B:    DIVU(ee, &instr);   break;
                case 0x0000000D:    BREAKPOINT(ee, instruction);break;
                case 0x00000012:    MFLO(ee, &instr);   break;
                case 0x0000002B:    SLTU(ee, &instr);   break;
                case 0x00000038:    DSLL(ee, &instr);   break;
                case 0x0000003C:    DSLL32(ee, &instr); break;
                case 0x0000003A:    DSRL(ee, &instr);   break;
                case 0x0000003E:    DSRL32(ee, &instr); break;
                case 0x00000024:    AND(ee, &instr);    break;
                case 0x00000002:    SRL(ee, &instr);    break;
                case 0x00000003:    SRA(ee, &instr);    break;
                case 0x000000A:     MOVZ(ee, &instr);   break;
                case 0x0000022:     SUB(ee, &instr);    break;
                case 0x0000023:     SUBU(ee, &instr);   break;
                case 0x0000010:     MFHI(ee, &instr);   break;
                case 0x0000020:     ADD(ee, &instr);    break;
                case 0x000002A:     SLT(ee, &instr);    break;
                case 0x0000004:     SLLV(ee, &instr);   break;
                case 0x0000006:     SRLV(ee, &instr);   break;
                case 0x0000027:     NOR(ee, &instr);    break;
                case 0x0000021:     ADDU(ee, &instr);   break;
                case 0x000000B:     MOVN(ee, &instr);   break;
                case 0x000001A:     DIV(ee, &instr);    break;
                case 0x0000007:     SRAV(ee, &instr);   break;
                case 0x0000017:     DSRAV(ee, &instr);  break;
                case 0x0000014:     DSLLV(ee, &instr);  break;
                case 0x000000C:     SYSCALL(ee);        break;
                case 0x0000011:     MTHI(ee, &instr);   break;
                case 0x0000013:     MTLO(ee, &instr);   break;

                default: 
                {
                    errlog("[ERROR]: Could not interpret special instruction: [{:#09x}]\n", instruction);
                } break;
            }
        } break;

        case 0b001010:  SLTI(ee, &instr);   break;
        case 0b000101:  BNE(ee, &instr);    break;
        case 0b000010:  J(ee, &instr);      break;
        case 0b001111:  LUI(ee, &instr);    break;
        case 0b001100:  ANDI(ee, &instr);   break;
        case 0b001101:  ORI(ee, &instr);    break;
        case 0b001000:  ADDI(ee, &instr);   break;
        case 0b001001:  ADDIU(ee, &instr);  break;
        case 0b101011:  SW(ee, &instr);     break;
        case 0b111111:  SD(ee, &instr);     break;
        case 0b101100:  SDL(ee, &instr);    break;
        case 0b101101:  SDR(ee, &instr);    break;
        case 0b000011:  JAL(ee, &instr);    break;
        case 0b000100:  BEQ(ee, &instr);    break;
        case 0b010100:  BEQL(ee, &instr);   break;
        case 0b001011:  SLTIU(ee, &instr);  break;
        case 0b010101:  BNEL(ee, &instr);   break;
        case 0b100011:  LW(ee, &instr);     break;
        case 0b110001:  LWC1(ee, &instr);   break;
        case 0b100111:  LWU(ee, &instr);    break;
        case 0b100000:  LB(ee, &instr);     break;
        case 0b111001:  SWC1(ee, &instr);   break;
        case 0b100100:  LBU(ee, &instr);    break;
        case 0b000110:  BLEZ(ee, &instr);   break;
        case 0b010110:  BLEZL(ee, &instr);  break;
        case 0b110111:  LD(ee, &instr);     break;
        case 0b011010:  LDL(ee, &instr);    break;
        case 0b011011:  LDR(ee, &instr);    break;
        case 0b101000:  SB(ee, &instr);     break;
        case 0b101001:  SH(ee, &instr);     break;
        case 0b100001:  LH(ee, &instr);     break;
        case 0b100101:  LHU(ee, &instr);    break;
        case 0b000111:  BGTZ(ee, &instr);   break;
        case 0b001110:  XORI(ee, &instr);   break;
        case 0b011001:  DADDIU(ee, &instr); break;
        case 0b101111:  CACHE_IXIN(ee, instruction); break;
        case 0b011111:  SQ(ee, &instr);     break;
        case 0b011110:  LQ(ee, &instr);     break;
        case MMI:
        {
            int fmt = instruction & 0x3f;
            switch(fmt) 
            {
                case 0x00000001: MADDU(ee, instruction); break;
                case 0x00000012: MFLO1(ee, &instr); break;
                case 0x0000001B: DIVU1(ee, &instr); break;
                case 0x00000018: MULT1(ee, &instr); break;
                case 0x00000029:
                {
                    // intlog("Dunno what this is\n");
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
                case 0b00000: BLTZ(ee, &instr); break;
                case 0b00001: BGEZ(ee, &instr); break;
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

    ee->current_instruction = ee_core_load_32(ee->pc);
    ee->next_instruction    = ee_core_load_32(ee->pc + 4);
    decode_and_execute(ee, ee->current_instruction);

    ee->pc              += 4;
    ee->cop0.regs[9]    += 1;
    ee->reg.r[0].SD[0]  = 0;

    // @Speed: These functions emit a ungodly amount of assembly. Fix!!!
    // set_kernel_mode(&ee->cop0);
}

void 
r5900_shutdown()
{
    free(_scratchpad_);
    fclose(dis);
    console.close();
}