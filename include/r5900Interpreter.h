#pragma once

#ifndef R5900INTERP_H
#define R5900INTERP_H

#include <cstdint>
#include <iostream>
#include <vector>
#include <fstream>
#include <stdio.h>
#include <cstdint>
#include <array>

#if _WIN32 || _WIN64
#include <windows.h>
#endif

#include "ps2types.h"


typedef struct Range {
    u32 start;
    u32 size;
    
    inline Range(u32 _start, u32 _size) : start(_start), size(_size) {}
/*
    inline u32 contains(u32 addr) {
        if (addr >= start && addr <= start + size) 
            return addr - start;
    }
    */
} Range;

/*
  KUSEG: 00000000h-7FFFFFFFh User segment
  KSEG0: 80000000h-9FFFFFFFh Kernel segment 0
  KSEG1: A0000000h-BFFFFFFFh Kernel segment 1
  KSSEG: C0000000h-DFFFFFFFh Supervisor segment
  KSEG3: E0000000h-FFFFFFFFh Kernel segment 3
*/

/*
Range BIOS          = Range(0x1fc00000, MEGABYTES(4));
Range RDRAM         = Range(0x00000000, MEGABYTES(32));
Range IO_REGISTERS  = Range(0x10000000, KILOBYTES(64));
Range VU0           = Range(0x11000000, KILOBYTES(4));
Range Vu1           = Range(0x11008000, KILOBYTES(16));
Range GS_REGISTERS  = Range(0x12000000, KILOBYTES(8));
Range IOP_RAM       = Range(0x1C000000, MEGABYTES(4));
Range SCRATCHPAD    = Range(0x70000000, KILOBYTES(16));
*/
/*

u32 MCH_RICM = 0, MCH_DRD = 0;
u8 rdram_sdevid = 0;
*/

union GP_Registers {
    struct {
        CPU_Types
        r0, // Hardwired to 0, writes are ignored
        at, // Temporary register used for pseudo-instructions
        v0, v1, // Return register, holds values returned by functions
        a0, a1, a2, a3, // Argument registers, holds first four parameters passed to a function
        t0, t1, t2, t3, t4, t5, t6, t7, // Temporary registers. t0-t3 may also be used as additional argument registers
        s0, s1, s2, s3, s4, s5, s6, s7, // Saved registers. Functions must save and restore these before using them
        t8, t9, //  Temporary registers
        k0, k1, // Reserved for use by kernels
        gp, // Global pointer
        sp, // Stack pointer
        fp, // Frame pointer
        ra; // Return address. Used by JAL and (usually) JALR to store the address to return to after a function
    } gpr;

    CPU_Types r[32];
};

// @@move: why is this in here
union INTC {
    uint32_t value;
    struct {
        u32 INT_GS : 1;
        u32 INT_SBUS : 1;
        u32 INT_VB_ON : 1; // Vblank
        u32 INT_VB_OFF : 1;
        u32 INT_VIF0 : 1;        
        u32 INT_VIF1 : 1;
        u32 INT_VU0 : 1;
        u32 INT_VU1 : 1;
        u32 INT_IPU : 1;
        u32 INT_TIMER0 : 1;
        u32 INT_TIMER1 : 1;
        u32 INT_TIMER2 : 1;
        u32 INT_TIMER3 : 1;
        u32 INT_SFIFO : 1;
        u32 INT_VU0WD : 1;
    };
};

typedef struct _TLB_Entry_ {
    bool valid[2];
    bool dirty[2];
    int cache_mode;
    u32 page_frame_number[2]; // even and odd pages
    bool set_scratchpad;
    u32 asid;
    bool set_global;
    u32 vpn2;
    u32 page_mask;
} TLB_Entry;

enum Cache_Mode : int {
    UNCACHED             = 2,
    CACHED               = 3,
    UNCACHED_ACCELERATED = 7,
};

enum INSTRUCTION_TYPE : int {
    COP0 = 0b010000, SPECIAL = 0b000000,
    MMI  = 0b011100, REGIMM  = 0b000001,
};

union COP0_Cause {
    struct {
        u32 unused          : 2;
        u32 ex_code         : 5; 
        u32 unused1         : 3;
        u32 int0_pending    : 1;
        u32 int1_pending    : 1;
        u32 timer_pending   : 1;
        u32 EXC2            : 3;
        u32 unused2         : 9;
        u32 CE              : 2;
        u32 BD2             : 1;
        u32 BD              : 1;
    };
    uint32_t value;
};

/* @@Remove: remove these bit masks eventually
/******************************************************
*               COP0_Status Bit Masks
*******************************************************/
/*
IE      = 0x01,         // Interrupt enable
EXL     = 0x02,         // Exception level (set when a level 1 exception occurs)
ERL     = 0x03,         //  Error level (set when a level 2 exception occurs)
KSU     = 0x18,         // Kernel Priviledge Level
IM_2_3  = 0xC00,        // INT0 enable (INTC)
BEM     = 0x1000,       // Bus error mask - when set, bus errors are disabled
IM_7    = 0x8000,       // INT5 enable (COP0 timer)
EIE     = 0x10000,      // EIE - Master interrupt enable
EDI     = 0x20000,      // EDI - If not set, EI/DI only works in kernel mode
CH      = 0x40000,      // CH - Status of most recent Cache Hit instruction
BEV     = 0x400000,     // BEV - If set, level 1 exceptions go to "bootstrap" vectors in BFC00xx
DEV     = 0x800000,     // DEV - If set, level 2 exceptions go to "bootstrap" vectors in BFC00xxx
FR      = 0x4000000,    // FR - ?????
CU      = 0xF0000000,   //Coprocessor Usage, COP3 is disabled
*/

union COP0_Status {
    struct {
        u32 IE      : 1, 
            EXL     : 1, 
            ERL     : 1, 
            KSU     : 2, 
            IM_2    : 1, 
            IM_3    : 1, 
            BEM     : 1, 
            IM_7    : 1, 
            EIE     : 1, 
            EDI     : 1, 
            CH      : 1, 
            BEV     : 1, 
            DEV     : 1, 
            CU      : 4; 
    };
    uint32_t value;
};

union COP0_Registers {
    struct {
        u32 index,
            random,
            entryLo0,
            entryLo1,
            context,
            pageMask,
            wired,
            //unreserved
            badVAddr,
            count,
            entryHi0,
            compare1;
        COP0_Status status;
        COP0_Cause cause;
        u32 EPC,
            PRid,
            config,
            // 17-22 reserved
            bad_paddr,
            debug,
            perf,
            //26-27 reserved
            tagLo,
            tagHi,
            errorEPC;
            //reserved
    };
    u32 regs[32];

    inline u32 operator [](uint32_t i) {
        return regs[i];
    }
};

// @@Rename: Possibly rename this to FPU registers.
union COP1_Registers {
    struct {
        u32
            f0, f1, f2, f3, // return values
            f4, f5, f6, f7, f8, f9, f10, f11, // temporary registers
            f12, f13, f14, f15, f16, f17, f18, f19, // argument registers
            f20, f21, f22, f23, f24, f25, f26, f27, f28, f29, f30, f31; // saved registers
    } fpr;
    u32 fprs[32];
    u32 fcr0; // reports implementation and revision of fpu
    u32 fcr31; // control register, stores status flags
};

union VU_Registers {
    struct {
        u32
            vf0,  vf1,  vf3,  vf4,  vf5,  vf6,  vf7,  vf8,  vf9,
            vf10, vf11, vf13, vf14, vf15, vf16, vf17, vf18, vf19,
            vf20, vf21, vf23, vf24, vf25, vf26, vf27, vf28, vf29,
            vf30, vf31;
    } f;
    struct {
        s16
            vi0,  vi1,  vi3,  vi4,  vi5,  vi6,  vi7,  vi8,  vi9,
            vi10, vi11, vi13, vi14, vi15, vi16;
    } i;

};

typedef struct _R5900Core_ {
    GP_Registers   reg;

    u64 HI, HI1, LO, LO1;
    COP0_Registers cop0;
    COP1_Registers cop1;

    bool is_branching;
    int delay_slot;
    int opmode;
    u32 sa;
    u32 pc;
    u32 branch_pc;
    u32 current_cycle;
    u32 current_instruction;
    u32 next_instruction;
} R5900_Core;

#if 0
template <typename T>
inline T ee_core_load_memory (u32 address) 
{
    T r;
    if (address >= 0x70000000 && address < 0x70004000)
        return *(T*)&_scratchpad_[address & 0x3FFF];
    if (address >= 0x30100000 && address < 0x31FFFFFF) // uncached and accelerated
        address -= 0x10000000;

    address &= 0x1FFFFFFF;
    
    return ee_load_memory<T>(address);
}

template <typename T>
inline void 
ee_core_store_memory(u32 address, T value) 
{
    if (address >= 0x70000000 && address < 0x70004000) {
        *(T*)&_scratchpad_[address & 0x3FFF] = value;
        return;
    }

    address &= 0x1FFFFFFF;
    ee_store_memory(address, value);
}
#endif

void r5900_cycle(R5900_Core *ee);
//void r5900_reset(R5900_Core &ee);
void r5900_shutdown();

#endif