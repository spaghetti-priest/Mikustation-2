#pragma once

#ifndef COP0_H
#define COP0_H

// #include "../ps2types.h"
// #include "r5900Interpreter.h"


/*typedef struct _TLB_Entry_ {
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
};*/

// @@Move: might move to ps2types.h
enum ExceptionCodes {
    __INTERRUPT               = 0x0,  __TLB_MODIFIED            = 0x1,
    __TLB_REFILL              = 0x2,  __TLB_REFILL_STORE        = 0x3,
    __ADDRESS_ERROR           = 0x4,  __ADDRESS_ERROR_STORE     = 0x5,
    __BUS_ERROR_INSTRUCTION   = 0x6,  __BUS_ERROR_DATA          = 0x7,
    __SYSCALL                 = 0x8,  __BREAKPOINT              = 0x9,
    __RES_INSTRUCTION         = 0xA,  __COP_UNUSABLE            = 0xB,
    __OVER_FLOW               = 0xC,  __TRAP                    = 0xD,
};

enum ExceptionVectors {
    V_RESET_NMI = 0x000, V_TLB_REFILL   = 0x000,
    V_COUNTER   = 0x080, V_DEBUG        = 0x100,
    V_COMMON    = 0x180, V_INTERRUPT    = 0x200
};

enum Kernel_modes : u8 {
    __KERNEL_MODE     = 0,
    __SUPERVISOR_MODE = 1,
    __USER_MODE       = 2,
};

typedef struct _Exception_ {
    ExceptionVectors vector;
    ExceptionCodes code;
} Exception;

inline Exception 
get_exception(ExceptionVectors vector, ExceptionCodes code)
{
    Exception r;
    r.vector = vector != V_COMMON ? vector : V_COMMON;
    r.code   = code;
    return r;
}

u32 handle_exception_level_1 (COP0_Registers *cop0, Exception *exc, unsigned int current_pc, bool is_branching);

/*
inline void
cop0_timer_compare_check(_R5900Core_ *ee)
{
    bool are_equal = ee->cop0.regs[9] == ee->cop0.regs[11];
    if (are_equal) {
        ee->cop0.cause.timer_pending = 1;
        ee->cop0.status.IM_7 = 1;
    }
}*/
#endif