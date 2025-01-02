/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include "../include/ee/cop0.h"
void 
set_cop0_reg(int index, u32 value)
{
	return;
}

/*void
handle_exception_level_1 (R5900_Core *ee, Exception *exc)
{
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

	// @@Incomplete: This doesnt do anything or go anywhere since the next instruction is never
	//ee->next_instruction = ee_load_32(ee->pc);
}*/

u32
handle_exception_level_1 (COP0_Registers *cop0, Exception *exc, u32 current_pc, bool is_branching)
{
	u32 return_pc 		= 0;
	cop0->cause.ex_code = exc->code;

	if (cop0->status.EXL) {
		exc->vector = V_COMMON;
	} else if (is_branching) {
		cop0->EPC 		= current_pc - 4;
		cop0->cause.BD 	= 1;
	} else {
		cop0->EPC 		= current_pc;
		cop0->cause.BD 	= 0;
	}
	
	// Set privilege mode to kernel mode
	cop0->status.EXL = 1;

	if 		(exc->code == __TLB_REFILL) exc->vector = V_TLB_REFILL;
	else if (exc->code == __INTERRUPT)  exc->vector = V_INTERRUPT;
	else  						  	  	exc->vector = V_COMMON;	

	if (cop0->status.BEV)
		return_pc = 0xBFC00200 + (u32)exc->vector;
	else
		return_pc = 0x80000000 + (u32)exc->vector;

	return return_pc;
}

void 
handle_exception_level_2 (R5900_Core *ee, Exception *exc) {}
