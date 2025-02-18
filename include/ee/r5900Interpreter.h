#pragma once

#ifndef R5900INTERP_H
#define R5900INTERP_H

/*
#define WIN32_LEAN_AND_MEAN
#if _WIN32 || _WIN64
#include <windows.h>
#endif
*/

#include "../ps2types.h"

/*
  KUSEG: 00000000h-7FFFFFFFh User segment
  KSEG0: 80000000h-9FFFFFFFh Kernel segment 0
  KSEG1: A0000000h-BFFFFFFFh Kernel segment 1
  KSSEG: C0000000h-DFFFFFFFh Supervisor segment
  KSEG3: E0000000h-FFFFFFFFh Kernel segment 3
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
    INSTR_COP0 = 0b010000, INSTR_SPECIAL = 0b000000,
    INSTR_MMI  = 0b011100, INSTR_REGIMM  = 0b000001,
    INSTR_COP1 = 0b010001,
};

union COP0_Cause {
    struct {
        u32 unused          : 2;
        u32 ex_code         : 5; // Exception Code
        u32 unused1         : 3; 
        u32 int1_pending    : 1;
        u32 int0_pending    : 1;
        u32 timer_pending   : 1; // COP0 timer pending
        u32 EXC2            : 3; // Level 2 exceptions
        u32 unused2         : 9; 
        u32 CE              : 2; // Coprocessor number when coprocessor unstable
        u32 BD2             : 1; // level 2 exception branch delay slot
        u32 BD              : 1; // level 1 exception branch delay slot
    };
    u32 value;
};

union COP0_Status {
    struct {
        u32 IE      : 1, // Interrupt enable
            EXL     : 1, // Exception level (set when a level 1 exception occurs)
            ERL     : 1, // Error level (set when a level 2 exception occurs)
            KSU     : 2, // Kernel Priviledge Level
            IM_2    : 1, // interrupt_0 pending
            IM_3    : 1, // interrupt_1 pending
            BEM     : 1, // Bus error mask - when set, bus errors are disabled
            IM_7    : 1, // COP0 timer interrupt pending
            EIE     : 1, // EIE - Master interrupt enable
            EDI     : 1, // EDI - If not set, EI/DI only works in kernel mode
            CH      : 1, // CH - Status of most recent Cache Hit instruction
            BEV     : 1, // BEV - If set, level 1 exceptions go to "bootstrap" vectors in BFC00xx
            DEV     : 1, // DEV - If set, level 2 exceptions go to "bootstrap" vectors in BFC00xxx
            CU      : 4; //Coprocessor Usage, COP3 is disabled
    };
    u32 value;
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
            unused0,
            //unreserved
            badVAddr,
            count,
            entryHi0,
            compare;
        COP0_Status status;
        COP0_Cause cause;
        u32 EPC,
            PRid,
            config,
            unused1[6],
            // 17-22 reserved
            bad_paddr,
            debug,
            perf,
            unused2[2],
            //26-27 reserved
            tagLo,
            tagHi,
            errorEPC,
            unused3;
            //reserved
    };
    u32 regs[32];

    inline u32 operator [](u32 i) {
        return regs[i];
    }
};

#if 0
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
#endif

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
    //COP1_Registers cop1;

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
void ee_reset(R5900_Core *ee);
void r5900_shutdown();

#endif