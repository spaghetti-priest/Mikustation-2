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
// FILE* dis = fopen("disasm.txt", "w+");

// std::ofstream console("disasm.txt", std::ios::out);

// static u8 *_icache_         = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));
// static u8 *_dcache_         = (u8 *)malloc(sizeof(u8) * KILOBYTES(8));
static u8 *_scratchpad_     = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));

 //Range SCRATCHPAD = Range(0x70000000, KILOBYTES(16));

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

// @@Note: Fuck everything I said previously. On the r5900load delay slots are optional
static void load_delay() {}

enum Access {
    _LOAD,
    _STORE,
};

enum Type {
    _HALF,
    _WORD,
    _DOUBLE,
    _QUAD,
};

enum Sign {
    _UNSIGN,
    _SIGN,
};

static inline bool check_address_error_exception (R5900_Core *ee, Access access, Type type, u32 vaddr)
{
    bool err = false;
    u32 low_bits = 0;
    // If the exception system becomes a problem to debug just add more types to the type enum 
    // to have it point to the location of the functoin caller or something else more efficient 
    const char *acc = 0;
    const char *location = 0;
    switch(access)
    {
        case _LOAD:
            acc = "Load";        
        break;

        case _STORE:
            acc = "Store";
        break;
    };

    switch(type)
    {
        case _HALF: 
            low_bits = vaddr & 0x1;
            location = "Half";
        break;
        
        case _WORD:     
            low_bits = vaddr & 0x3;
            location = "Word"; 
        break;
        
        case _DOUBLE:   
            low_bits = vaddr & 0x7;
            location = "Double"; 
        break;
        
        case _QUAD:    
            low_bits = vaddr & 0xF;
            location = "Quad"; 
        break;
    };

    if (low_bits != 0) {
        errlog("[ERROR] {:s}_{:s} is not properly aligned. Vaddr: [{:#08x}]\n", acc, location, vaddr);
        Exception exc   = get_exception(V_COMMON, __ADDRESS_ERROR);
        ee->pc          = handle_exception_level_1(&ee->cop0, &exc, ee->pc, ee->is_branching);
        err             = true;
    }

    return err;
}

/* This is an interesting and clear implemtation from Dobiestation */
static const u8 LDL_SHIFT[8] = {56, 48, 40, 32, 24, 16, 8, 0};
static const u8 LDR_SHIFT[8] = {0, 8, 16, 24, 32, 40, 48, 56};
static const u8 SDL_SHIFT[8] = {56, 48, 40, 32, 24, 16, 8, 0};
static const u8 SDR_SHIFT[8] = {0, 8, 16, 24, 32, 40, 48, 56};

static const u64 LDL_MASK[8] =
{   0x00ffffffffffffffULL, 0x0000ffffffffffffULL, 0x000000ffffffffffULL, 0x00000000ffffffffULL,
    0x0000000000ffffffULL, 0x000000000000ffffULL, 0x00000000000000ffULL, 0x0000000000000000ULL
};

static const u64 LDR_MASK[8] = 
{
    0x0000000000000000ULL, 0xff00000000000000ULL, 0xffff000000000000ULL, 0xffffff0000000000ULL, 
    0xffffffff00000000ULL, 0xffffffffff000000ULL, 0xffffffffffff0000ULL, 0xffffffffffffff00ULL      
};

static const u64 SDR_MASK[8] = 
{
    0x0000000000000000ULL, 0xff00000000000000ULL, 0xffff000000000000ULL, 0xffffff0000000000ULL, 
    0xffffffff00000000ULL, 0xffffffffff000000ULL, 0xffffffffffff0000ULL, 0xffffffffffffff00ULL      
};

static const u64 SDL_MASK[8] =
{   0x00ffffffffffffffULL, 0x0000ffffffffffffULL, 0x000000ffffffffffULL, 0x00000000ffffffffULL,
    0x0000000000ffffffULL, 0x000000000000ffffULL, 0x00000000000000ffULL, 0x0000000000000000ULL
};

static inline void add_overflow() {return;}
static inline void add_overflow64() {return;}
static inline void sub_overflow() {return;}
static inline void sub_overflow64() {return;}

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
   
    switch (mode) 
    {
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
    switch (opcode) 
    {
/*
*-----------------------------------------------------------------------------
* COP0 Instructions
*-----------------------------------------------------------------------------
*/
        case INSTR_COP0:
        {
            int fmt = instruction >> 21 & 0x3F;
            if(fmt == 0x10) return; // @Hack: putting this here so that it doesnt spam console
            switch (fmt) 
            {
                case 0x00:
                {
                    u32 cop0                = instr.rd;
                    u32 gpr                 = instr.rt;
                    ee->reg.r[gpr].SD[0]    = ee->cop0.regs[cop0];
                    // intlog("MFC0 GPR: [{:d}] COP0: [{:d}]\n", gpr, cop0);
                } break;

                case 0x04:
                {
                    u32 cop0    = instr.rd;
                    s32 gpr     = ee->reg.r[instr.rt].SW[0];

                    if(cop0 == 12) {
                        ee->cop0.status.IE              = (gpr >> 0) & 0x1;
                        ee->cop0.status.EXL             = (gpr >> 1) & 0x1;
                        ee->cop0.status.ERL             = (gpr >> 2) & 0x1;
                        ee->cop0.status.KSU             = (gpr >> 3) & 0x1;
                        ee->cop0.status.IM_2            = (gpr >> 10) & 0x1;
                        ee->cop0.status.IM_3            = (gpr >> 11) & 0x1;
                        ee->cop0.status.BEM             = (gpr >> 12) & 0x1;
                        ee->cop0.status.IM_7            = (gpr >> 15) & 0x1;
                        ee->cop0.status.EIE             = (gpr >> 16) & 0x1;
                        ee->cop0.status.EDI             = (gpr >> 17) & 0x1;
                        ee->cop0.status.CH              = (gpr >> 18) & 0x1;
                        ee->cop0.status.BEV             = (gpr >> 22) & 0x1;
                        ee->cop0.status.DEV             = (gpr >> 23) & 0x1;
                        ee->cop0.status.CU              = (gpr >> 28) & 0xF;
                    } else if (cop0 == 13) {
                        ee->cop0.cause.ex_code          = (gpr >> 2)  & 0x1F; 
                        ee->cop0.cause.int0_pending     = (gpr >> 10) & 0x1;
                        ee->cop0.cause.int1_pending     = (gpr >> 11) & 0x1;
                        ee->cop0.cause.timer_pending    = (gpr >> 15) & 0x1;
                        ee->cop0.cause.EXC2             = (gpr >> 16) & 0x3;
                        ee->cop0.cause.CE               = (gpr >> 28) & 0x3;
                        ee->cop0.cause.BD2              = (gpr >> 30) & 0x1;
                        ee->cop0.cause.BD               = (gpr >> 31) & 0x1;
                    } else {
                        ee->cop0.regs[cop0] = gpr;
                    }     
                    // intlog("MTC0 GPR: [{:d}] COP0: [{:d}]\n", gpr, cop0);
                } break;

                case 0x08:
                {
                    int tlb = instruction & 0x3F;
                    switch(tlb)
                    {
                        case 0x2:
                        {
                            // @@Implemtation: implement TLB
                            int index = ee->cop0.regs[0];
                            //TLB_Entry current = TLBS.at(index);
                            // intlog("TLBWI not yet implemented\n");
                        } break;

                        case 0x18:
                        {
                            ee->pc = ee->cop0.EPC;
                            intlog("ERET");
                        } break;
                    }
                } break;

                default:
                {
                    errlog("[ERROR]: Could not interpret COP0 instruction format opcode [{:#09x}]\n", fmt);
                } break;
            }
        } break;

/*
*-----------------------------------------------------------------------------
* COP1 Instructions
*-----------------------------------------------------------------------------
*/
        case INSTR_COP1: { cop1_decode_and_execute(ee, instruction); } break;

/*
*-----------------------------------------------------------------------------
* Special Instructions
*-----------------------------------------------------------------------------
*/
        case INSTR_SPECIAL:
        {
            u32 special = instruction & 0x3F; // DSRA32 is 63 so make it up to 64
            switch(special) 
            {
                case 0x00: 
                {
                    u32 sa                      = instr.sa;
                    ee->reg.r[instr.rd].SD[0]   = (u64)((s32)ee->reg.r[instr.rt].UW[0] << instr.sa);
                    intlog("SLL source: [{:d}] dest: [{:d}] [{:#x}]\n", instr.rd, instr.rt, instr.sa);
                } break;

                case 0x02: 
                {
                    u32 result                  = (ee->reg.r[instr.rt].UW[0] >> instr.sa);
                    ee->reg.r[instr.rd].SD[0]   = (s32)result;
                    intlog("SRL source: [{:d}] dest: [{:d}] [{:#x}]\n", instr.rd, instr.rt, instr.sa);
                } break;

                case 0x03: 
                {
                    s32 result                  = (ee->reg.r[instr.rt].SW[0]) >> (s32)instr.sa;
                    ee->reg.r[instr.rd].SD[0]   = result;
                    intlog("SRA source: [{:d}] dest: [{:d}] [{:#x}]\n", instr.rd, instr.rt, (s32)instr.sa);
                } break;

                case 0x04: 
                {
                    u32 sa                      = ee->reg.r[instr.rs].SW[0] & 0x1F;
                    ee->reg.r[instr.rd].SD[0]   = (s64)(ee->reg.r[instr.rt].SW[0] << sa);
                    intlog("SLLV source: [{:d}] dest: [{:d}] [{:#x}]\n", instr.rd, instr.rt, sa);
                } break;

                case 0x06: 
                {
                    u32 sa                      = ee->reg.r[instr.rs].SW[0] & 0x1F;
                    ee->reg.r[instr.rd].SD[0]   = (s64)(ee->reg.r[instr.rt].SW[0] >> sa);
                    intlog("SRLV source: [{:d}] dest: [{:d}] [{:#x}]\n", instr.rd, instr.rt, sa);
                } break;

                case 0x07: 
                {
                    u32 sa                      = ee->reg.r[instr.rs].UB[0] & 0x1F;
                    s32 result                  = ee->reg.r[instr.rt].SW[0] >> sa;
                    ee->reg.r[instr.rd].SD[0]   = (s64)result;
                    intlog("SRAV [{:d}] [{:d}] [{:d}]\n", instr.rd, instr.rt, instr.rs);
                } break;

                case 0x08: 
                {
                    // @TODO: Check if the LSB is 0
                    jump_to(ee, ee->reg.r[instr.rs].UW[0]);
                    intlog("JR source: [{:d}] pc_dest: [{:#x}]\n", instr.rs, ee->reg.r[instr.rs].UW[0]);
                } break;

                case 0x09: 
                {
                    u32 return_addr = ee->pc + 8;
                    
                    if (instr.rd != 0) {
                        ee->reg.r[instr.rd].UD[0] = return_addr;
                    } else {
                        ee->reg.r[31].UD[0] = return_addr;
                    }

                    jump_to(ee, ee->reg.r[instr.rs].UW[0]);

                    intlog("JALR [{:d}]\n", instr.rs); 
                } break;

                case 0x0A: 
                {
                    if (ee->reg.r[instr.rt].SD[0] == 0) {
                        ee->reg.r[instr.rd].SD[0] = ee->reg.r[instr.rs].SD[0];
                    }
                    intlog("MOVZ [{:d}] [{:d}] [{:d}]\n",instr.rd, instr.rs, instr.rt);
                } break;

                case 0x0B: 
                {
                    if (ee->reg.r[instr.rt].UD[0] != 0) {
                        ee->reg.r[instr.rd].UD[0] = ee->reg.r[instr.rs].UD[0];
                    }
                    intlog("MOVN [{:d}] [{:d}] [{:d}]\n", instr.rd, instr.rs, instr.rt);
                } break;

                case 0x0C: 
                {
                   SYSCALL(ee); 
                } break;

                case 0x0D: 
                {
                    // @@Implementation We have no debugger yet so theres no need to implement this now
                    // intlog("BREAKPOINT\n");
                } break;

                case 0x0F: 
                {
                    // @@Note: Issued after every MTC0 write The PS2 possibly executes millions of memory loads / stores per second, 
                    // which is entirely feasible in hardware. But a complete pain in software as that would completely kill performance
                    // May be cool to implement this for accuracy since that is the purpose of the emulator
                    // intlog("SYNC\n");
                } break;

                case 0x10: 
                {
                    ee->reg.r[instr.rd].UD[0] = ee->HI;
                    intlog("MFHI [{:d}]\n", instr.rd);
                } break;

                case 0x11: 
                {
                    ee->HI = ee->reg.r[instr.rs].UD[0];
                    intlog("MTHI [{:d}]\n", instr.rs);
                } break;

                case 0x12: 
                {
                    ee->reg.r[instr.rd].UD[0] = ee->LO;
                    intlog("MFLO [{:d}] \n", instr.rd);
                } break;

                case 0x13: 
                {
                    ee->LO = ee->reg.r[instr.rs].UD[0];
                    intlog("MTLO [{:d}]\n", instr.rs);
                } break;

                case 0x14: 
                {
                    s32 sa                      = ee->reg.r[instr.rs].UW[0] & 0x3F;
                    ee->reg.r[instr.rd].UD[0]   = ee->reg.r[instr.rt].UD[0] << sa;
                    intlog("DSLLV [{:d}] [{:d}] [{:d}]\n", instr.rd, instr.rt, instr.rs); 
                } break;

                case 0x17: 
                {
                    u32 sa                      = ee->reg.r[instr.rs].UW[0] & 0x3F;
                    ee->reg.r[instr.rd].SD[0]   = ee->reg.r[instr.rt].SD[0] >> sa;
                    intlog("DSRAV [{:d}] [{:d}] [{:d}]\n", instr.rd, instr.rt, instr.rs);
                } break;

                case 0x18: 
                {
                    s64 w1      = (s64)ee->reg.r[instr.rs].SW[0];
                    s64 w2      = (s64)ee->reg.r[instr.rt].SW[0];
                    s64 prod    = (w1 * w2);
                    
                    ee->LO      = (s32)(prod & 0xFFFFFFFF);
                    ee->HI      = (s32)(prod >> 32);
                    // @@Note: C790 apparently states that the chip does not always call mflo everytime
                    // during a 3 operand mult
                    // But we could always use mflo because of the pipeline interlock 
                    ee->reg.r[instr.rd].SD[0] = ee->LO;
                    intlog("MULT [{:d}] [{:d}] [{:d}]\n", instr.rs, instr.rt, instr.rd);
                } break;

                case 0x1A: 
                {
                    if (ee->reg.r[instr.rt].UD == 0) {
                        ee->LO = (int)0xffffffff;
                        ee->HI = ee->reg.r[instr.rs].UD[0];
                        errlog("[ERROR]: Tried to Divide by zero\n");
                        //return;
                    }
                    s32 d   = ee->reg.r[instr.rs].SW[0] / ee->reg.r[instr.rt].SW[0];
                    s32 q   = ee->reg.r[instr.rs].SW[0] % ee->reg.r[instr.rt].SW[0];
                    ee->LO  = (s64)d;
                    ee->HI  = (s64)q;
                    intlog("DIV [{:d}] [{:d}]\n", instr.rs, instr.rt);
                } break;

                case 0x1B: 
                {
                    s64 w1 = (s64)ee->reg.r[instr.rs].SW[0];
                    s64 w2 = (s64)ee->reg.r[instr.rt].SW[0];

                    if (w2 == 0) {
                        errlog("[ERROR]: Tried to Divide by zero\n");
                        //return;
                    } 
                    // @@Note: Sign extend by 64?
                    s64 q   = (s32)(w1 / w2);
                    ee->LO  = q;
                    s64 r   = (s32)(w1 % w2);
                    ee->HI  = r;
                    intlog("DIVU [{:d}] [{:d}]\n", instr.rs, instr.rt);
                } break;

                case 0x20: 
                {
                    // @@Incomplete: The add instruction must signal an exception on overflow
                    // but we have no overflow detection for now
                    int temp                    = ee->reg.r[instr.rs].SW[0] + ee->reg.r[instr.rt].SW[0];
                    ee->reg.r[instr.rd].SD[0]   = (s64)temp;
                    intlog("ADD [{:d}] [{:d}] [{:d}]\n", instr.rd, instr.rs, instr.rt);   
                } break;

                case 0x21: 
                {
                    s32 result                  = ee->reg.r[instr.rs].SW[0] + ee->reg.r[instr.rt].SW[0];
                    ee->reg.r[instr.rd].UD[0]   = result;
                    intlog("ADDU [{:d}] [{:d}] [{:d}]\n", instr.rd, instr.rs, instr.rt);
                } break;

                case 0x22: 
                {
                    s32 result                  = ee->reg.r[instr.rs].SW[0] - ee->reg.r[instr.rt].SW[0];
                    ee->reg.r[instr.rd].UD[0]   = (s64)result;

                    /* @@Incomplete: No 2 complement Arithmetic Overflow error implementation*/
                    intlog("SUB [{:d}] [{:d}] [{:d}]\n", instr.rd, instr.rs, instr.rt);
                } break;

                case 0x23: 
                {
                    s32 result                  = ee->reg.r[instr.rs].SW[0] - ee->reg.r[instr.rt].SW[0];
                    ee->reg.r[instr.rd].UD[0]   = (u64)result;
                    intlog("SUBU [{:d}] [{:d}] [{:d}]\n", instr.rd, instr.rs, instr.rt);  
                } break;

                case 0x24: 
                {
                    ee->reg.r[instr.rd].UD[0] = ee->reg.r[instr.rs].UD[0] & ee->reg.r[instr.rt].UD[0];
                    intlog("AND [{:d}] [{:d}] [{:d}] \n", instr.rd, instr.rs, instr.rt);
                } break;

                case 0x25: 
                {
                    ee->reg.r[instr.rd].SD[0] = ee->reg.r[instr.rs].SD[0] | ee->reg.r[instr.rt].SD[0];
                    intlog("OR [{:d}] [{:d}] [{:d}]\n", instr.rd, instr.rs, instr.rt);
                } break;

                case 0x27: 
                {
                    ee->reg.r[instr.rd].SD[0] = ~(ee->reg.r[instr.rs].SD[0] | ee->reg.r[instr.rt].SD[0]);
                    // assert(1);
                    intlog("NOR [{:d}] [{:d}] [{:d}]\n", instr.rd, instr.rs, instr.rt);
                } break;

                case 0x2A: 
                {
                    s32 result                  = ee->reg.r[instr.rs].SD[0] < ee->reg.r[instr.rt].SD[0] ? 1 : 0;
                    ee->reg.r[instr.rd].SD[0]   = result;
                    intlog("SLT [{:d}] [{:d}] [{:d}]\n", instr.rd, instr.rs, instr.rt);
                } break;

                case 0x2B: 
                {
                    u32 result                  = (ee->reg.r[instr.rs].UD[0] < ee->reg.r[instr.rt].UD[0]) ? 1 : 0;
                    ee->reg.r[instr.rd].UD[0]   = result;
                    intlog("SLTU [{:d}] [{:d}] [{:d}]\n", instr.rd, instr.rs, instr.rt);
                } break;

                case 0x2D: 
                {
                    ee->reg.r[instr.rd].UD[0] = ee->reg.r[instr.rs].SD[0] + ee->reg.r[instr.rt].SD[0];
                    intlog("DADDU [{:d}] [{:d}] [{:d}]\n", instr.rd, instr.rs, instr.rt);  
                } break;

                case 0x38: 
                {
                    ee->reg.r[instr.rd].UD[0] = ee->reg.r[instr.rt].UD[0] << instr.sa;
                    intlog("DSLL source: [{:d}] dest: [{:d}] [{:#x}]\n", instr.rd, instr.rt, instr.sa);
                } break;
                
                case 0x3A: 
                {
                    ee->reg.r[instr.rd].UD[0] = ee->reg.r[instr.rt].UD[0] >> instr.sa;
                    intlog("DSRL source: [{:d}] dest: [{:d}] s: [{:#x}] \n", instr.rd, instr.rt, instr.sa);
                } break;

                case 0x3C: 
                {
                    u32 s                       = instr.sa + 32;
                    ee->reg.r[instr.rd].UD[0]   = ee->reg.r[instr.rt].UD[0] << s;
                    intlog("DSLL32 source: [{:d}] dest: [{:d}] s: [{:#x}] \n", instr.rd, instr.rt, s);
                } break;

                case 0x3E: 
                {
                    u32 s                       = instr.sa + 32;
                    ee->reg.r[instr.rd].UD[0]   = ee->reg.r[instr.rt].UD[0] >> s;
                    intlog("DSRL32 source: [{:d}] dest: [{:d}] s: [{:#x}] \n", instr.rd, instr.rt, s);
                } break;

                case 0x3F:
                {
                    u32 s = instr.sa + 32;
                    s64 result = ee->reg.r[instr.rt].SD[0] >> s;
                    ee->reg.r[instr.rd].SD[0] = result;
                    intlog("DSRA32 source: [{:d}] dest: [{:d}] s: [{:#x}] \n", instr.rd, instr.rt, s);
                } break;

                default: 
                {
                    errlog("[ERROR]: Could not interpret special instruction: [{:#09x}]\n", instruction);
                } break;
            }
        } break;
/*
*-----------------------------------------------------------------------------
* MMI Instructions
*-----------------------------------------------------------------------------
*/
        case INSTR_MMI:
        {
            int fmt = instruction & 0x3f;
            switch(fmt) 
            {
                case 0x01: 
                {
                    errlog("LOL\n");
                    // intlog("MADDU\n");
                    printf("lol");
                } break; 

                case 0x12: 
                {
                    ee->reg.r[instr.rd].UD[0] = ee->LO1;
                    intlog("MFLO_1 [{:d}]\n", instr.rd);
                } break; 
 
                case 0x18: 
                {
                    s32 w1 = (s32)ee->reg.r[instr.rs].SW[0];
                    s32 w2 = (s32)ee->reg.r[instr.rt].SW[0];
                    s64 prod = (s64)(w1 * w2);
                    
                    ee->LO1 = (s32)(prod & 0xFFFFFFFF);
                    ee->HI1 = (s32)(prod >> 32);
                    // @@Note: C790 apparently states that the chip does not always call mflo everytime
                    // during a 3-operand mult
                    // But we could always use mflo because of the pipeline interlock 
                    ee->reg.r[instr.rd].SD[0] = ee->LO1;

                    intlog("MULT1 [{:d}] [{:d}] [{:d}]\n", instr.rs, instr.rt, instr.rd);
                } break; 

                case 0x1B: 
                {
                    s32 w1 = ee->reg.r[instr.rs].SW[0];
                    s32 w2 = ee->reg.r[instr.rt].SW[0];

                    if (w2 == 0) {
                        ee->LO1 = (int)0xffffffff;
                        ee->HI1 = ee->reg.r[instr.rs].UD[0];
                        errlog("[ERROR] tried to divide by 0, result is Undefined\n");
                        //exit(1);
                    } 

                    s64 q   = (w1 / w2);
                    ee->LO1 = q;
                    s64 r   = (w1 % w2);
                    ee->HI1 = r;

                    intlog("DIVU_1 [{:d}] [{:d}]\n", instr.rs, instr.rt);
                } break; 

                case 0x29:
                {
                    // intlog("Dunno what this is\n");
                } break;

                default:
                {
                    errlog("[ERROR]: Could not interpret MMI instruction [{:#09x}]\n", fmt);
                } break;
            }

        } break;
/*
*-----------------------------------------------------------------------------
* REGIMM Instructions
*-----------------------------------------------------------------------------
*/
        case INSTR_REGIMM: 
        {
            int function = instruction >> 16 & 0x1f;
            switch(function) 
            {
                case 0x00: 
                {
                    s32 offset      = instr.sign_offset << 2;
                    bool condition  = ee->reg.r[instr.rs].SD[0] < 0;
                    branch(ee, condition, offset);

                    intlog("BLTZ [{:d}] [{:#x}] \n", instr.rs, offset); 
                } break;

                case 0x01: 
                {
                    s32 offset      = instr.sign_offset << 2;
                    bool condition  = ee->reg.r[instr.rs].SD[0] >= 0;
                    branch(ee, condition, offset);

                    intlog("BGEZ [{:d}] [{:#x}] \n", instr.rs, offset);  
                } break;

                default:
                {
                    errlog("[ERROR]: Could not interpret REGIMM instruction [{:#09x}]\n", function);
                } break;
            }
        } break;
/*
*-----------------------------------------------------------------------------
* Normal Instructions
*-----------------------------------------------------------------------------
*/
        case 0x02:
        {
            u32 offset = ((ee->pc + 4) & 0xF0000000) + (instr.instr_index << 2);
            jump_to(ee, offset);
            intlog("J [{:#x}]\n", offset);
        } break;

        case 0x03:
        {
            u32 instr_index     = instr.instr_index;
            u32 target_address  = ((ee->pc + 4) & 0xF0000000) + (instr_index << 2);

            jump_to(ee, target_address); 
            ee->reg.r[31].UD[0]  = ee->pc + 8;

            intlog("JAL [{:#x}] \n", target_address);   
        } break;

        case 0x04:
        {
            s32 imm         = instr.sign_imm << 2;
            bool condition  = ee->reg.r[instr.rs].SD[0] == ee->reg.r[instr.rt].SD[0];

            branch(ee, condition, imm);

            intlog("BEQ [{:d}] [{:d}] [{:#x}] \n", instr.rs, instr.rt, imm);
        } break;

        case 0x05:
        {
            s32 imm         = instr.sign_imm << 2;
            bool condition  = ee->reg.r[instr.rs].SD[0] != ee->reg.r[instr.rt].SD[0];

            branch(ee, condition, imm);

            intlog("BNE [{:d}] [{:d}], [{:#x}]\n", instr.rt, instr.rs, imm);
        } break;

        case 0x06:
        {
            s32 offset      = (instr.sign_offset << 2);
            bool condition  = ee->reg.r[instr.rs].SD[0] <= 0;

            branch(ee, condition, offset);

            intlog("BLEZ [{:d}] [{:#x}]\n", instr.rs, offset); 
        } break;

        case 0x07:
        {
            s32 offset      = instr.sign_offset << 2;
            bool condition  = ee->reg.r[instr.rs].SD[0] > 0;

            branch(ee, condition, offset);
            
            intlog("BGTZ [{:d}] [{:#x}] \n", instr.rs, offset);
        } break;

        case 0x08:
        {
            s32 result                  = ee->reg.r[instr.rs].SD[0] + instr.sign_imm;
            ee->reg.r[instr.rt].SD[0]   = result;
            /* @@Incomplete: No 2 complement Arithmetic Overflow error implementation*/
            intlog("ADDI: [{:d}] [{:d}] [{:#x}] \n", instr.rt, instr.rs, instr.sign_imm);
        } break;

        case 0x09:
        {
            s32 result                  = ee->reg.r[instr.rs].SD[0] + instr.sign_imm;
            ee->reg.r[instr.rt].SD[0]   = result;
            intlog("ADDIU: [{:d}] [{:d}] [{:#x}] \n", instr.rt, instr.rs, instr.sign_imm);
        } break;

        case 0x0A:
        {
            s32 result                  = (ee->reg.r[instr.rs].SD[0] < (s32)instr.sign_imm) ? 1 : 0;
            ee->reg.r[instr.rt].SD[0]   = result;
            
            intlog("SLTI [{:d}] [{:d}], [{:#x}]\n", instr.rt, instr.rs, instr.sign_imm);
        } break;

        case 0x0B:
        {
            u32 result                  = (ee->reg.r[instr.rs].UD[0] < (u64)instr.sign_imm) ? 1 : 0;
            ee->reg.r[instr.rt].UD[0]   = result;
            
            intlog("SLTIU [{:d}] [{:d}] [{:#x}]\n", instr.rt, instr.rs, (u64)instr.sign_imm);
        } break;

        case 0x0C:
        {
            ee->reg.r[instr.rt].UD[0] = ee->reg.r[instr.rs].UD[0] & (u64)instr.imm; 
            intlog("ANDI: [{:d}] [{:d}] [{:#x}] \n", instr.rt, instr.rs, (u64)instr.imm);
        } break;

        case 0x0D:
        {
            ee->reg.r[instr.rt].UD[0] = ee->reg.r[instr.rs].UD[0] | (u64)instr.imm; 
            intlog("ORI: [{:d}] [{:d}] [{:#x}] \n", instr.rt, instr.rs, (u64)instr.imm);
        } break;

        case 0x0E:
        {
            ee->reg.r[instr.rt].UD[0] = ee->reg.r[instr.rs].UD[0] ^ (u64)instr.imm;
            intlog("XORI [{:d}] [{:d}] [{:#x}] \n", instr.rt, instr.rs, (u64)instr.imm);
        } break;

        case 0x0F:
        {
            s64 imm                     = (s64)(s32)((instr.imm) << 16);
            ee->reg.r[instr.rt].UD[0]   = imm;

            intlog("LUI [{:d}] [{:#x}]\n", instr.rt, imm);
            // printf("LUI [{%d}] [{%#08x}]\n", rt, imm);
        } break;

        case 0x14:
        {
            s32 imm         = instr.sign_imm << 2;
            bool condition  = ee->reg.r[instr.rs].SD[0] == ee->reg.r[instr.rt].SD[0];

            branch_likely(ee, condition, imm);

            intlog("BEQL [{:d}] [{:d}] [{:#x}]\n", instr.rs, instr.rt, imm);  
        } break;

        case 0x15:
        {
            s32 imm         = instr.sign_imm << 2;
            bool condition  = ee->reg.r[instr.rs].SD[0] != ee->reg.r[instr.rt].SD[0];

            branch_likely(ee, condition, imm);

            intlog("BNEL [{:d}] [{:d}] [{:#x}]\n", instr.rs, instr.rt, imm);
        } break;

        case 0x16:
        {
            s32 offset      = (instr.sign_offset << 2);
            bool condition  = ee->reg.r[instr.rs].SD[0] <= 0;
            
            branch_likely(ee, condition, offset);
            
            intlog("BLEZL [{:d}] [{:#x}]\n", instr.rs, offset); 
        } break;

        case 0x19:
        {
            ee->reg.r[instr.rt].UD[0] = ee->reg.r[instr.rs].SD[0] + instr.sign_imm;
            intlog("DADDIU [{:d}] [{:d}] [{:#x}]\n", instr.rt, instr.rs, instr.sign_imm); 
        } break;

        case 0x1A:
        {
            u32 vaddr           = ee->reg.r[instr.rs].UW[0] + (s32)instr.sign_offset;
            u32 aligned_vaddr   = vaddr & ~0x7;
            u32 shift           = vaddr & 0x7;
            
            u64 aligned_dword   = ee_core_load_64(aligned_vaddr);
            u64 result          = (ee->reg.r[instr.rt].UD[0] & LDL_MASK[shift] | aligned_vaddr << LDL_SHIFT[shift]);
            ee->reg.r[instr.rt].UD[0] = result;
            intlog("LDL [{:d}] [{:#x}] [{:d}]\n", instr.rt, (s32)instr.sign_offset, instr.rs);
        } break;

        case 0x1B:
        {
            u32 vaddr           = ee->reg.r[instr.rs].UW[0] + (s32)instr.sign_offset;
            u32 aligned_vaddr   = vaddr & ~0x7;
            u32 shift           = vaddr & 0x7;
            
            u64 aligned_dword   = ee_core_load_64(aligned_vaddr);
            u64 result          = (ee->reg.r[instr.rt].UD[0] & LDR_MASK[shift] | aligned_vaddr << LDR_SHIFT[shift]);
            ee->reg.r[instr.rt].UD[0] = result;
            intlog("LDR [{:d}] [{:#x}] [{:d}]\n", instr.rt, (s32)instr.sign_offset, instr.rs);
            printf("LDR [{%d}] [{%#x}] [{%d}]\n", instr.rt, (s32)instr.sign_offset, instr.rs);
        } break;

        case 0x1E:
        {
            u32 vaddr = ee->reg.r[instr.rs].UW[0] + (s32)instr.sign_offset;
            
            if (check_address_error_exception(ee, _LOAD, _QUAD, vaddr))
                return;

            ee->reg.r[instr.rt].UD[0] = ee_core_load_64(vaddr);
            ee->reg.r[instr.rt].UD[1] = ee_core_load_64(vaddr + 8);
            // intlog("LQ [{:d}] [{:#x}] [{:d}]\n", instr.rt, (s32)instr.sign_offset, instr.rs);
        } break;

        case 0x1F:
        {
            u32 vaddr   = ee->reg.r[instr.rs].UW[0] + instr.sign_offset;
            u64 lov     = ee->reg.r[instr.rt].UD[0];
            u64 hiv     = ee->reg.r[instr.rt].UD[1];
           
            if (check_address_error_exception(ee, _STORE, _QUAD, vaddr))
                return;

            ee_core_store_64(vaddr, lov);       
            ee_core_store_64(vaddr + 8, hiv);       
            // intlog("SQ [{:d}] [{:#x}] [{:d}] \n", instr.rt, instr.sign_offset, base);
        } break;

        case 0x20:
        {
            u32 vaddr = ee->reg.r[instr.rs].UW[0] + (s32)instr.sign_offset;

            ee->reg.r[instr.rt].SD[0] = (s64)ee_core_load_8(vaddr);
            intlog("LB [{:d}] [{:#x}] [{:d}] \n", instr.rt, (s32)instr.sign_offset, instr.rs);  
        } break;

        case 0x21:
        {
            u32 vaddr = ee->reg.r[instr.rs].UW[0] + (s32)instr.sign_offset;
            
            if (check_address_error_exception(ee, _LOAD, _HALF, vaddr))
                return;

            ee->reg.r[instr.rt].SD[0] = (s64)ee_core_load_16(vaddr);
            intlog("LH [{:d}] [{:#x}] [{:d}] \n", instr.rt, (s32)instr.sign_offset, instr.rs);   
        } break;

        case 0x23:
        {
            u32 vaddr = ee->reg.r[instr.rs].UW[0] + (s32)instr.sign_offset;
            
            if (check_address_error_exception(ee, _LOAD, _WORD, vaddr))
                return;

            ee->reg.r[instr.rt].SD[0] = (s64)ee_core_load_32(vaddr);
            intlog("LW [{:d}] [{:#x}] [{:d}] \n", instr.rt, (s32)instr.sign_offset, instr.rs);
        } break;

        case 0x24:
        {
            u32 vaddr                   = ee->reg.r[instr.rs].UW[0] + (s32)instr.sign_offset;
            ee->reg.r[instr.rt].UD[0]   = (u64)ee_core_load_8(vaddr);
            intlog("LBU [{:d}] [{:#x}] [{:d}] \n", instr.rt, (s32)instr.sign_offset, instr.rs); 
        } break;

        case 0x25:
        {
            u32 vaddr = ee->reg.r[instr.rs].UW[0] + (s32)instr.sign_offset;

            if (check_address_error_exception(ee, _LOAD, _HALF, vaddr))
                return;

            ee->reg.r[instr.rt].UD[0] = (u64)ee_core_load_16(vaddr);
            intlog("LHU [{:d}] [{:#x}] [{:d}] \n", instr.rt, (s32)instr.sign_offset, instr.rs);
        } break;

        case 0x27:
        {
            u32 vaddr = ee->reg.r[instr.rs].UW[0] + (s32)instr.sign_offset;

            if (check_address_error_exception(ee, _LOAD, _WORD, vaddr))
                return;
           
            ee->reg.r[instr.rt].SD[0] = (u64)ee_core_load_32(vaddr);
            intlog("LWU [{:d}] [{:#x}] [{:d}] \n", instr.rt, (s32)instr.sign_offset, instr.rs);
        } break;

        case 0x28:
        {
            u32 vaddr   = ee->reg.r[instr.rs].UW[0] + (s32)instr.sign_offset;
            s8 value    = ee->reg.r[instr.rt].SB[0];

            ee_core_store_8(vaddr, value);
            intlog("SB [{:d}] [{:#x}] [{:d}] \n", instr.rt, (s32)instr.sign_offset, instr.rs);
        } break;

        case 0x29:
        {
            u32 vaddr   = ee->reg.r[instr.rs].UW[0] + (s32)instr.sign_offset;
            s16 value   = ee->reg.r[instr.rt].SH[0];

            if (check_address_error_exception(ee, _STORE, _HALF, vaddr))
                return;

            ee_core_store_16(vaddr, value);
            intlog("SH [{:d}] [{:#x}] [{:d}] \n", instr.rt, (s32)instr.sign_offset, instr.rs); 
        } break;

        case 0x2B:
        {
            u32 vaddr = ee->reg.r[instr.rs].UW[0] + (s32)instr.sign_offset;
            s32 value = ee->reg.r[instr.rt].SW[0];
            
            if (check_address_error_exception(ee, _STORE, _WORD, vaddr))
                return;

            ee_core_store_32(vaddr, value);
            intlog("SW [{:d}] [{:#x}] [{:d}] \n", instr.rt, instr.sign_offset, instr.rs);
        } break;

        case 0x2C:
        {
            u32 vaddr           = ee->reg.r[instr.rs].UW[0] + instr.sign_offset;
            u32 aligned_vaddr   = vaddr & ~0x7;
            u32 shift           = vaddr & 0x7;
            
            u64 aligned_dword   = ee_core_load_64(aligned_vaddr);
            u64 result          = (ee->reg.r[instr.rt].UD[0] & SDL_MASK[shift] | aligned_vaddr << SDL_SHIFT[shift]);
            ee_core_store_64(aligned_vaddr, result);
            intlog("SDL [{:d}] [{:#x}] [{:d}]\n", instr.rt, instr.sign_offset, instr.rs);
            printf("SDL [{%d}] [{%#x}] [{%d}]\n", instr.rt, instr.sign_offset, instr.rs);
        } break;

        case 0x2D:
        {
            u32 vaddr           = ee->reg.r[instr.rs].UW[0] + instr.sign_offset;
            u32 aligned_vaddr   = vaddr & ~0x7;
            u32 shift           = vaddr & 0x7;
            
            u64 aligned_dword   = ee_core_load_64(aligned_vaddr);
            u64 result          = (ee->reg.r[instr.rt].UD[0] & SDR_MASK[shift] | aligned_vaddr << SDR_SHIFT[shift]);
            ee_core_store_64(aligned_vaddr, result);
            intlog("SDR [{:d}] [{:#x}] [{:d}]\n", instr.rt, instr.sign_offset, instr.rs);
            printf("SDR [{%d}] [{%#x}] [{%d}]\n", instr.rt, instr.sign_offset, instr.rs);
        } break;

        case 0x2F:
        {
            // intlog("Invalidate instruction cache");
        } break;

        case 0x31:
        {
            u32 vaddr = ee->reg.r[instr.rs].UW[0] + (s32)instr.sign_offset;
            
            if (check_address_error_exception(ee, _LOAD, _WORD, vaddr))
                return;

            u32 data = (u32)ee_core_load_32(vaddr);

            cop1_setFPR(instr.rt, data);
            intlog("LWC1 [{:d}] [{:#x}] [{:d}] \n", instr.rt, (s32)instr.sign_offset, instr.rs);
        } break;

        case 0x37:
        {
            u32 vaddr = ee->reg.r[instr.rs].UW[0] + (s32)instr.sign_offset;
            
            if (check_address_error_exception(ee, _LOAD, _DOUBLE, vaddr))
                return;

            ee->reg.r[instr.rt].UD[0] = ee_core_load_64(vaddr);
            intlog("LD [{:d}] [{:#x}] [{:d}]\n", instr.rt, (s32)instr.sign_offset, instr.rs);
        } break;

        case 0x39:
        {
            u32 vaddr = ee->reg.r[instr.rs].UW[0] + (s32)instr.sign_offset;
            
            if (check_address_error_exception(ee, _STORE, _WORD, vaddr))
                return;

            ee_core_store_32(vaddr, cop1_getFPR(instr.rt));
            intlog("SWC1 [{:d}] [{:#x}] [{:d}] \n", instr.rt, (s32)instr.sign_offset, instr.rs); 
        } break;

        case 0x3F:
        {
            u32 vaddr   = ee->reg.r[instr.rs].UW[0] + (s32)instr.sign_offset;
            u64 value   = ee->reg.r[instr.rt].UD[0];
          
            if (check_address_error_exception(ee, _STORE, _DOUBLE, vaddr))
                return;

            ee_core_store_64(vaddr, value);   
         
            // intlog("SD [{:d}] [{:#x}] [{:d}] \n", instr.rt, (s32)instr.sign_offset, instr.rs);
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
    //fclose(dis);
    //console.close();
}