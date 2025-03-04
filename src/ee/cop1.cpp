/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

// #include "ee/cop1.h"
// #include "common.h"
// #include <cmath>

COP1_Registers cop1 = {0};

enum Instruction_Type 
{
	BC_INSTR = 0x8,
	S_INSTR = 0x10,
	W_INSTR = 0x14,
};

void 
cop1_reset()
{
	syslog("Resetting Coprocessor 1\n");
	memset(&cop1, 0, sizeof(cop1));
	cop1.fcr0 = 0x2e30;
}

static f32
convert (u32 index)
{
	return *(f32*)&cop1.fpr[index].u;
}

void
cop1_setFPR (u32 index, u32 data)
{
	cop1.fpr[index].u = data;
}

f32
cop1_getFPR (u32 index)
{
	return cop1.fpr[index].u;
}

void 
cop1_decode_and_execute (R5900_Core *ee, u32 instruction)
{
	u8 opcode = (instruction >> 21) & 0x1F;
	switch(opcode)
	{
		case 0x00:
		{
			u32 fs 			= (instruction >> 11) & 0x1F;
			u32 rt 			= (instruction >> 16) & 0x1F;
			ee->reg.r[rt].UW[0] = (s32)cop1.fpr[fs].u;
			// intlog("MFC1 [{:d}] [{:d}] \n", rt, fs);
		} break;
		
		case 0x04:
		{
			u32 fs 			= (instruction >> 11) & 0x1F;
			u32 rt 			= (instruction >> 16) & 0x1F;
			cop1.fpr[fs].u 	= ee->reg.r[rt].UW[0];
			
			// intlog("MTC1 [{:d}] [{:d}] \n", rt, fs);
		} break;

		case 0x06:
		{
			u32 fs 		= (instruction >> 11) & 0x1F;
			u32 rt 		= (instruction >> 16) & 0x1F;
			
			if(fs != 31) return;

			cop1.fcr31 	= ee->reg.r[rt].UW[0];
			
			// intlog("CTC1 [{:d}] [{:d}] \n", rt, fs); 
		} break;

		case BC_INSTR:
		{
			u8 bc = (instruction >> 16) & 0x3F;
			switch (bc)
			{
				default:
				{
					errlog("ERROR: Could not interpret COP1 BC instruction [{:#x}]\n", bc);
					return;
				} break;
			}
		} break;

		case S_INSTR:
		{
			u16 s = instruction & 0x3F;	
			switch(s)
			{
				case 0x02:
				{
					u32 fd = (instruction >> 6) & 0x1F;
					u32 fs = (instruction >> 11) & 0x1F;
					u32 ft = (instruction >> 16) & 0x1F;
					f32 reg1 = convert(fs);
					f32 reg2 = convert(ft);

					cop1.fpr[fd].f = reg1 * reg2;

					/* @@Incomplete: Add exponent overflow and undeflow */
					// intlog("MUL.S [{:d}] [{:d}] [{:d}] \n", fd, fs, ft);
				} break;

				case 0x03:
				{
					u32 fd = (instruction >> 6) & 0x1F;
					u32 fs = (instruction >> 11) & 0x1F;
					u32 ft = (instruction >> 16) & 0x1F;
					
					f32 nom 	= convert(fs);
					f32 denom 	= convert(ft);

					if ((denom == 0) && nom != 0) {
						//@@Incomplete: +maximum and -maximum
						// Set D and SD bits 
						cop1.fcr31 |= (1 << 5);
						cop1.fcr31 |= (1 << 16);
					} else if ((denom == 0) && (nom == 0)) {
						// Set I and SI bits 
						cop1.fcr31 |= (1 << 6);
						cop1.fcr31 |= (1 << 17);
					}

					cop1.fpr[fd].f = nom / denom;
					
					/* @@Incomplete: Add exponent overflow and undeflow */
					// intlog("DIV.S [{:d}] [{:d}] [{:d}] \n", fd, fs, ft);
				} break;
		
				case 0x06:
				{
					u32 fd = (instruction >> 6) & 0x1F;
					u32 fs = (instruction >> 11) & 0x1F;
					cop1.fpr[fd].f = cop1.fpr[fs].f;

					// intlog("MOV.S [{:d}] [{:d}]\n", fd, fs);
				}		
				case 0x18:
				{
					u32 fs 		= (instruction >> 11) & 0x1F;
					u32 ft 		= (instruction >> 16) & 0x1F;
					f32 reg1 	= convert(fs);
					f32 reg2 	= convert(ft);
					cop1.ACC.f 	= reg1 + reg2;
					// intlog("ADDA.S [{:d}] [{:d}] \n", fs, ft);
				} break;

				case 0x24:
				{
					u32 fd = (instruction >> 6) & 0x1F;
					u32 fs = (instruction >> 11) & 0x1F;
					s32 k = (s32)floor(cop1.fpr[fs].f);
					cop1.fpr[fd].u = (u32)k;
					// intlog("CVT.W.S [{:d}] [{:d}] \n", fd, fs);
				} break;

				default:
				{
					errlog("ERROR: Could not interpret COP1 S instruction [{:#x}]\n", s);
					return;
				} break;
			}
		} break;

		case W_INSTR:
		{	
			u16 w = instruction & 0x3F;
			switch (w)
			{
				case 0x20:
				{
					u32 fd = (instruction >> 6) & 0x1F;
					u32 fs = (instruction >> 11) & 0x1F;
					f32 k = (f32)cop1.fpr[fs].u;
					cop1.fpr[fd].f = k;
					// intlog("CVT.S.W [{:d}] [{:d}] \n", fd, fs);
				} break;

				default:
				{
					errlog("ERROR: Could not interpret COP1 W instruction [{:#x}]\n", w);
					return;
				} break;
			}

		} break;

		default:
		{
			errlog("ERROR: Could not interpret COP1 instruction [{:#x}]\n", instruction);
		} break;
	}
}