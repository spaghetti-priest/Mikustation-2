/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include "../include/cop1.h"

COP1_Registers cop1 = {0};

static f32
convert (u32 index)
{
	return *(f32*)&cop1.fpr[index].u;
}

f32
cop1_getFPR(u32 index)
{
	return cop1.fpr[index].u;
}

void 
cop1_reset()
{
	printf("Resetting Coprocessor 1\n");
	memset(&cop1, 0, sizeof(cop1));
	cop1.fcr0 = 0x2e30;
}

void 
cop1_decode_and_execute (R5900_Core *ee, u32 instruction)
{
	u16 opcode = (instruction >> 21) & 0x1F;
	switch(opcode)
	{
		case 0x00000004:
		{
			u32 fs = (instruction >> 11) & 0x1F;
			u32 rt = (instruction >> 16) & 0x1F;
			cop1.fpr[fs].u = ee->reg.r[rt].UW[0];
			
			printf("MTC1 [%d] [%d] \n", rt, fs);
		} break;

		case 0x00000010:
		{
			u32 fs = (instruction >> 11) & 0x1F;
			u32 ft = (instruction >> 16) & 0x1F;
			f32 reg1 = convert(fs);
			f32 reg2 = convert(ft);
			cop1.ACC.f = reg1 + reg2;
			printf("ADDA.S [%d] [%d] \n", fs, ft);
		} break;

		case 0x00000006:
		{
			u32 fs = (instruction >> 11) & 0x1F;
			u32 rt = (instruction >> 16) & 0x1F;
			cop1.fcr31 = ee->reg.r[rt].UW[0];
			
			printf("MTC1 [%d] [%d] \n", rt, fs); 
		} break;
		default:
		{
			printf("ERROR: Could not interpret COP1 instruction {%#09x}\n", instruction);
		} break;
	}
}