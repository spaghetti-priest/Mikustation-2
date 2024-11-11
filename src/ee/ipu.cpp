/*
 * Copyright 2023-2024 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include "../include/ee/ipu.h"
#include <iostream>

IPU ipu = {};

void
ipu_reset() 
{
	memset(&ipu, 0, sizeof(ipu));
	printf("Resetting IPU \n");
}

void 
ipu_write_32 (u32 address, u32 value)
{
	switch(address) 
	{		
		case 0x10002000:
		{
			ipu.command.write.command_option 	= value & 0x7FFFFFF;
			ipu.command.write.command_code 		= (value >> 28) & 0x1F;
			printf("IPU_CMD value: [%#08x]\n", value);
		} break;
		
		case 0x10002010:
		{
			ipu.control.intra_dc_precision 	= (value >> 16) & 0x3;
			ipu.control.alternate_scan 		= (value >> 20) & 0x1;
			ipu.control.intra_vlc_format 	= (value >> 21) & 0x1;
			ipu.control.q_scale_step 		= (value >> 22) & 0x1;
			ipu.control.mpeg_bit_stream 	= (value >> 23) & 0x1;
			ipu.control.picture_type 		= (value >> 24) & 0x7;
			ipu.control.reset 				= (value >> 30) & 0x1;
			printf("IPU_CTRL value: [%#08x]\n", value);
		} break;
	}
}

void 
ipu_write_64 (u32 address, u64 value)
{
	switch(address) 
	{
		case 0x10002000:
		{
			ipu.command.write.command_option 	= (value >> 0) & 0x0FFFFFFF;
			ipu.command.write.command_code 		= (value >> 28) & 0xF;
			printf("IPU_CMD\n");
		} break;
	}
}

u32 
ipu_read_32 (u32 address)
{
	switch(address) 
	{
		case 0x10002010:
		{
			u32 r;
			r |= ipu.control.input_fifo_counter 	<< 0;
			r |= ipu.control.output_fifo_counter 	<< 4;
			r |= ipu.control.coded_block_pattern  	<< 8;
			r |= ipu.control.error_code_detected  	<< 14;
			r |= ipu.control.start_code_detected  	<< 15;
			r |= ipu.control.intra_dc_precision  	<< 16;
			r |= ipu.control.alternate_scan  		<< 20;
			r |= ipu.control.intra_vlc_format  		<< 21;
			r |= ipu.control.q_scale_step  			<< 22;
			r |= ipu.control.mpeg_bit_stream  		<< 23;
			r |= ipu.control.picture_type  			<< 24;
			r |= ipu.control.busy  					<< 31;
			printf("IPU_CTRL value: [%#08x]\n", r);
			return r;
		} break;

		case 0x10002020:
		{
			u32 r;
			r |= ipu.bitposition.bitstream_pointer << 0;
			r |= ipu.bitposition.fifo_counter << 8;
			r |= ipu.bitposition.fifo_pointer << 16;
			printf("IPU_BP\n");
			return r;
		} break;
	}
}

u64 
ipu_read_64 (u32 address)
{
	switch(address) 
	{
		case 0x10002000:
		{
			u64 r;
			r |= ipu.command.read.decoded_data << 0; 
			r |= ipu.command.read.command_busy << 63;
			printf("IPU_CMD\n");
			return r;
		} break;
		
		case 0x10002030:
		{
			u64 r;
			r |= ipu.bitstream.bstop << 0;
			r |= ipu.bitstream.command_busy << 63;
			printf("IPU_TOP\n");
			return r;
		} break;
	}
}

void ipu_fifo_write() {
	printf("IPU in_fifo write\n");
}