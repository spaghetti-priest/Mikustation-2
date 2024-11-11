/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include "../include/ee/gif.h"
#include "../include/ee/gs.h"
#include <iostream>

alignas(16) GIF gif;

void 
gif_reset () 
{
	printf("Resetting GIF interface\n");
	memset(&gif, 0, sizeof(gif));
}

enum Data_Modes : u8 {
	PACKED 	= 0b00,
	REGLIST = 0b01,
	IMAGE 	= 0b10,
	DISABLE = 0b11,
};

u32 reg_counter = 0;

void 
gif_process_packed (GIF_Tag *current_tag, u128 data)
{
	u32 NLOOP = current_tag->NLOOP;
	u32 total_data_in_gif = current_tag->NLOOP * current_tag->NREGS; /* When NREGS * NLOOP is odd, the last doubleword in a primitive is discarded.*/
	u32 destination = (current_tag->REGS >> (4 * reg_counter)) & 0xF;
	if (current_tag->PRE == 1) {
		gs_set_primitive(current_tag->PRIM);
	} else {
		/* Idle Cylcle Here */
	}

	switch(destination)
	{
		case 0x00: 
		{
			gs_set_primitive(data.lo & 0x3FF);
		} break;

		case 0x01: 
		{
			u8 r, g, b, a;
			r = data.lo & 0xFF;
			g = (data.lo >> 32) & 0xFF;
			b = (data.hi >> 0) & 0xFF;
			a = (data.hi >> 32) & 0xFF;
			gs_set_rgbaq(r, g, b, a);
		} break;
		
		case 0x02: 
		{
			f32 s, t, q;
			s = data.lo & 0xFFFF;
			t = data.lo >> 32;
			q = data.hi & 0xFFFF;
			gs_set_st(s, t);
			gs_set_q(q);
		} break;
		
		case 0x03: 
		{
			u32 u, v;
			u = data.lo & 0x3FFF;
			v = (data.lo >> 32) & 0x3FFF;
			gs_set_uv(u, v);
		} break;

		case 0x04: 
		{
			u16 x, y;
			u32 z;
			u8 f;
			bool ADC;
			x 	= data.lo & 0xFFFF;
			y 	= (data.lo >> 32) & 0xFFFF;
			z 	= (data.hi >> 4) & 0xFFFFFF;
			f 	= (data.hi >> 36) & 0xFF;
			ADC = (data.hi >> 47) & 0x1;

			if (ADC == 0)	gs_set_xyzf2(x,y,z,f);
			else 			gs_set_xyzf3(x,y,z,f);
		} break;

		case 0x05: 
		{
			s16 x, y;
			u32 z;
			bool ADC;
			x 	= data.lo & 0xFFFF;
			y 	= (data.lo >> 32) & 0xFFFF;
			z 	= (data.hi >> 4) & 0xFFFFFFFF;

			ADC = (data.hi >> 47) & 0x1;

			if (ADC == 0)	gs_set_xyz2(x, y, z);
			else 			gs_set_xyz3(x, y, z);
		} break;

		case 0x0a: 
		{
			u8 fog = (data.hi >> 36) & 0xFF;
			gs_set_fog(fog);
		} break;
		
		case 0x0e: {
			/* 0xA+D */
			u8 addr 			= data.hi & 0xFF;
			u64 packaged_data 	= data.lo;
			gs_write_64_internal(addr, packaged_data);
		} break;
		
		case 0x0f: 
		{
			/*No Output */
			return;
		} break;

		default:
		{
			printf("Unrecognized GIF tag address\n");
		} break;
	}
}

void gif_process_reglist(GIF_Tag *current_tag) 
{
//	printf("GIFtag process reglist\n");
}

void gif_process_image(GIF_Tag *current_tag, u128 data) 
{
	//u32 NLOOP = current_tag->NLOOP;
	//for (int i = 0; i < NLOOP; ++i) {
		gs_write_hwreg(data.lo);
		gs_write_hwreg(data.hi);
	//}
	printf("GIFtag process reglist\n");
}
void gif_process_disable(GIF_Tag *current_tag) {
//	printf("GIFtag process reglist\n");
}

void
gif_select_mode (GIF_Tag *current_tag, u128 data)
{
	u16 mode = current_tag->FLG;
	switch (mode)
	{
		case PACKED:
		{
			gif_process_packed(current_tag, data);
			reg_counter += 1;
			if (current_tag->NREGS <= reg_counter) {
				if (current_tag->data_left == 0) {
					current_tag->is_tag = false;
				}
				current_tag->data_left--;
				reg_counter = 0;
			}
		} break;

		case REGLIST: 	gif_process_reglist(current_tag); break;
		
		case IMAGE: 
		{
			gif_process_image(current_tag, data);
			current_tag->data_left--;
		} break;	
		
		case DISABLE: 	gif_process_disable(current_tag); break;
		default:
			//printf("Unrecognized GIFtag format\n");
		break;
	};
}

//@@Incomplete: This only accepts tags from PATH3
void
gif_unpack_tag (u128 pack)
{
	if (!gif.tag[0].is_tag) {
		// of course c++ turns something nice into something ugly and redundant
		GIF_Tag new_gif_tag = {
			.NLOOP 		=  (u16)(pack.lo & 0x7fff), 
			.EOP 		= (bool)((pack.lo >> 15) & 0x1),   
			.PRE 		= (bool)((pack.lo >> 46) & 0x1),   
			.PRIM 		= (u16)((pack.lo >> 47) & 0x7ff),  
			.FLG 		= (u8)((pack.lo >> 58) & 0x3),   
			.NREGS 		= (u8)((pack.lo >> 60) & 0xf),  
			.REGS 		= pack.hi,
			.is_tag 	= true,
		};

		//new_gif_tag.regs_left = new_gif_tag.NREGS;
		new_gif_tag.data_left = new_gif_tag.NLOOP;
		if (new_gif_tag.NREGS == 0 && new_gif_tag.NLOOP != 0)  new_gif_tag.NREGS = 16; 
		gif.tag[0] = new_gif_tag;
		gs_set_q(1.0); 
	}

	gif_select_mode(&gif.tag[0], pack);
}

void
gif_process_path3 (u128 data) 
{
	if(data.lo || data.hi)
		gif_unpack_tag(data);
}

u32 
gif_read_32 (u32 address)
{
	printf("READ: gif_read32 address: [%#08x]\n", address);

	if (gif.ctrl.pause) {
		switch (address) 
		{
			case 0x10003020:
			{
				printf("READ: STAT [%#08x]\n", gif.stat.value);
				return 0;				
			}
			case 0x10003040:
			{
				printf("READ: TAG0 [%#08x]\n", gif.tag0.value);
				return gif.tag0.value;
			} break;

			case 0x10003050:
			{
				printf("READ: TAG1 [%#08x]\n", gif.tag1.value);
				return gif.tag1.value;
			} break;
			
			case 0x10003060:
			{
				printf("READ: TAG2 [%#08x]\n", gif.tag2.value);
				return gif.tag2.value;
			} break;
			
			case 0x10003070:
			{
				printf("READ: TAG3 [%#08x]\n", gif.tag3.value);
				return gif.tag3.value;
			} break;
			
			case 0x10003080:
			{
				printf("READ: CNT [%#08x]\n", gif.cnt.value);
				return gif.cnt.value;
			} break;
			
			case 0x10003090:
			{
				printf("READ: P3CNT [%#08x]\n", gif.p3cnt.value);
				return gif.p3cnt.value;
			} break;
			
			case 0x100030A0:
			{
				printf("READ: P3TAG [%#08x]\n", gif.p3tag.value);
				return gif.p3tag.value;
			} break;

			default:
			{
				printf("[ERROR] Failed to read gif_read32\n");
				return 0;
			} break;
		}
	}
	
	return 0;
}

void 
gif_write_32 (u32 address, u32 value)
{
	printf("WRITE: gif_write32 address: [%#08x]\n", address);
	switch(address)
	{
		case 0x10003000:
		{
			printf("WRITE: GIF CTRL value: [%#08x]\n", value);
			gif.ctrl.reset = value & 0x1;
			gif.ctrl.pause = (value >> 3) & 0x1;
			gif.ctrl.value = value;
			return;
		} break;

		case 0x10003010:
		{
			printf("WRITE: GIF MODE value: [%#08x]\n", value);
			gif.mode.mask 				= value & 0x1;
			gif.mode.intermittent_mode = (value >> 2) & 0x1;
			gif.mode.value 			= value;
			return;
		} break;

		default:
		{
			printf("[ERROR] Failed to read gif_write32\n");
			return;
		} break;
	}
	return;
}

void 
gif_fifo_write (u32 address, u64 data) 
{
	//printf("WRITE: GIF FIFO\n");
}