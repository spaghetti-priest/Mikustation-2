#ifndef _IPU_H
#define _IPU_H

#include "ps2types.h"

union IPU_CMD {
	struct {
		u32 command_option : 28;
		u8 	command_code : 4;
		u32 unused;
	} write;

	struct {
		u32 	decoded_data;
		u32 	unused : 31;
		bool 	command_busy;
	} read;

	u64 value;
};

union IPU_TOP {
	struct {
		u32 	bstop;
		u32 	unused : 31;
		bool 	command_busy;
	};
	u64 value;
};

union IPU_CTRL {
	struct {
		u8 		input_fifo_counter 	: 4;
		u8 		output_fifo_counter : 4;
		u8 		coded_block_pattern : 6;
		bool 	error_code_detected;
		bool 	start_code_detected;
		u8 		intra_dc_precision 	: 2;
		u8 		unused0 			: 2;
		bool 	alternate_scan;
		bool 	intra_vlc_format;
		bool 	q_scale_step;
		bool 	mpeg_bit_stream;
		u8 		picture_type 		: 3;
		u8 		unused1 			: 2;
		bool 	reset;
		bool 	busy;
	};
	u32 value;
};

union IPU_BP {
	struct {
		u8 		bitstream_pointer 	: 7;
		bool 	unused0;
		u8 		fifo_counter 		: 4;
		u8  	unused1 			: 4;
		u8 		fifo_pointer 		: 2;
		u16 	unused3 			: 15;
	};
	u32 value;
};

struct IPU {
	IPU_CMD 	command;
	IPU_TOP 	bitstream;
	IPU_CTRL 	control;
	IPU_BP 		bitposition;
};

void 	ipu_reset();

void 	ipu_write_32(u32 address, u32 value);
void 	ipu_write_64(u32 address, u64 value);
u32 	ipu_read_32(u32 address);
u64 	ipu_read_64(u32 address);
void 	ipu_fifo_write();

#endif