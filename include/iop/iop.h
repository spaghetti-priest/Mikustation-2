#pragma once

#ifndef R5000INTERPRETER_H
#define R5000INTERPRETER_H

#include "../ps2types.h"

/*
    KUSEG: 00000000h-7FFFFFFFh User segment
    KSEG0: 80000000h-9FFFFFFFh Kernel segment 0
    KSEG1: A0000000h-BFFFFFFFh Kernel segment 1
*/
union IOP_COP0_Cause {
    struct {
        bool unused[2];
        u8 excode : 5;
        bool unused2;
        u8 interrupt_pending_field;
        bool unused3[12];
        u8 coprocessor_error : 2;
        bool unused4;
        bool branch_delay_pointer;
    };

    u32 value;
};

union IOP_COP0_Status {
    struct {
        bool current_interrupt,
            current_kernel_mode,
            previous_interrupt,
            previous_kernel_mode,
            old_interrupt,
            old_kernel,
            unused[2];
        u8 interrupt_mask;
        bool isolate_cache,
            swapped_cache,
            cache_parity_bit,
            last_load_operation,
            cache_parity_error,
            TLB_shutdown,
            boot_exception_vector,
            unused2[2],
            reverse_endianess,
            unused3[2],
            cop0_enable;
    };
    u32 value;
};

union IOP_COP0 {
    struct {
        u32 unused[2],
            BPC,
            unused1,
            BDA,
            JUMPDEST,
            DCIC,
            BadVaddr,
            BDAM,
            unused2,
            BPCM;
        IOP_COP0_Status status;
        IOP_COP0_Cause cause;
        u32 EPC,
            PRID,
            unused3[48];
    };

    u32 r[32];
};

typedef struct _R5000Core_ {
    //GP_Registers   reg;
    u32 reg[32];
    u32 HI, LO;
    IOP_COP0 cop0;

    bool is_branching;
    int delay_slot;
    int opmode;
    u32 sa;
    u32 pc;
    u32 branch_pc;
    u32 current_cycle;
    u32 current_instruction;
    u32 next_instruction;
} R5000_Core;
  
// @Temporary @Note: Only using this for EE and IOP loads in the interconnect
u32 get_iop_pc();
u32 get_iop_register(u32 r);

void iop_cycle();
void iop_reset();

#endif