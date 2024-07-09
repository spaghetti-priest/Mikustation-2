/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include "../include/cop0.h"
#include "../include/ps2.h"

void 
set_cop0_reg(int index, u32 value)
{
	return;
}

void
handle_exception_level_1 (R5900_Core *ee, Exception *exc)
{
	exc->vector = V_COMMON;

	ee->cop0.cause.ex_code = exc->code;
	if (ee->cop0.status.EXL) {
		exc->vector = V_COMMON;
	} else if (ee->is_branching) {
		ee->cop0.EPC 		= ee->pc - 4;
		ee->cop0.cause.BD 	= 1;
	} else {
		ee->cop0.EPC 		= ee->pc;
		ee->cop0.cause.BD 	= 0;
	}
	// Set privilege mode to kernel mode
	ee->cop0.status.EXL = 1;

	if 		(exc->code == __TLB_REFILL) exc->vector = V_TLB_REFILL;
	else if (exc->code == __INTERRUPT)  exc->vector = V_INTERRUPT;
	else  						  	  	exc->vector = V_COMMON;	

	if (ee->cop0.status.BEV)
		ee->pc = 0xBFC00200 + (u32)exc->vector;
	else
		ee->pc = 0x80000000 + (u32)exc->vector;

	ee->next_instruction = ee_load_32(ee->pc);
}

void 
handle_exception_level_2 (R5900_Core *ee, Exception *exc) {}

enum Kernel_modes : u8 {
    __KERNEL_MODE     = 0,
    __SUPERVISOR_MODE = 1,
    __USER_MODE       = 2,
};

inline void 
set_kernel_mode (COP0_Registers *cop0, Kernel_modes mode)
{
    COP0_Status *status; 
    u32 ERL 			= cop0->status.ERL;
    u32 EXL 			= cop0->status.EXL;
    if (ERL || EXL) return;

    switch (mode) {
        case __KERNEL_MODE: 		cop0->status.KSU = __KERNEL_MODE; break;
        case __SUPERVISOR_MODE: 	cop0->status.KSU = __SUPERVISOR_MODE; break;
        case __USER_MODE: 			cop0->status.KSU = __USER_MODE; break;
    };
}
