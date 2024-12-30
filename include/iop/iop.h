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

inline void
iop_cop0_write_cause (IOP_COP0_Cause *cause, u32 data)
{
    cause->excode                    = (data >> 0)  & 0x1F;
    cause->interrupt_pending_field   = (data >> 1)  & 0xFF;
    cause->coprocessor_error         = (data >> 28) & 0x3;
    cause->branch_delay_pointer      = (data << 31);
}

inline void 
iop_cop0_write_status (IOP_COP0_Status *status, u32 data)
{
    status->current_interrupt     = (data >> 0) & 0x1;
    status->current_kernel_mode   = (data >> 1) & 0x1;
    status->previous_interrupt    = (data >> 2) & 0x1;
    status->previous_kernel_mode  = (data >> 3) & 0x1;
    status->old_interrupt         = (data >> 4) & 0x1;
    status->old_kernel            = (data >> 5) & 0x1;
    status->interrupt_mask        = ((data >> 8) & 0xFF);
    status->isolate_cache         = (data >> 16) & 0x1;
    status->swapped_cache         = (data >> 17) & 0x1;
    status->cache_parity_bit      = (data >> 18) & 0x1;
    status->last_load_operation   = (data >> 19) & 0x1;
    status->cache_parity_error    = (data >> 20) & 0x1;
    status->TLB_shutdown          = (data >> 21) & 0x1;
    status->boot_exception_vector = (data >> 22) & 0x1;
    status->reverse_endianess     = (data >> 25) & 0x1;
    status->cop0_enable           = (data >> 28) & 0x1;
}
  
// @Temporary @Note: Only using this for EE and IOP loads in the interconnect
u32 get_iop_pc();
u32 get_iop_register(u32 r);

void iop_cycle();
void iop_reset();

#endif