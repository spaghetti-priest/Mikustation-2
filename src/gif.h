#ifndef GIF_H

// #include <queue>

enum Data_Modes : u8 
{
	PACKED 	= 0b00,
	REGLIST = 0b01,
	IMAGE 	= 0b10,
	DISABLE = 0b11,
};

enum Packing_Formats : u8
{
	_PRIM 	= 0x00,
	_RGBAQ 	= 0x01,
	_ST 	= 0x02,
	_UV 	= 0x03,
	_XYZF 	= 0x04,
	_XYZ 	= 0x05,
	_FOG 	= 0x0a,
	_A_D 	= 0x0e,
};

typedef struct GIFTag_t {
	u16 	NLOOP : 15; // Data size
	bool 	EOP;   		// End of packet
	u32 	unused : 29;
	bool 	PRE;   		// PRIM field enable
	u16 	PRIM : 11;  // data sent to prim of GS
	u8 		FLG : 2;   	// Data format
	u8 		NREGS;  	// Number of Register descriptors
	u64 	REGS;   	// Register descriptor 

	u32 	reg_count;
	u32 	data_left;
	
	bool 	is_tag;
} GIF_Tag;

// @@Accuracy: Should go back and implement unused bits in these registers
// Unncessarily did these in an hurry
union GIF_CTRL {
	struct {
		bool reset;
		bool pause;		
	};
	u32 value;
};

union GIF_MODE {
	struct {
		bool mask;
		bool unused;
		bool intermittent_mode;
	};
	u32 value;
};

union GIF_STAT {
	struct {
		bool 	path3_mask; 		// PATH3 masked by GIF_MODE
		bool 	vif1_mask; 			// PATH3 masked by VIF1 MASKP3
		bool 	transfer_mode; // Intermittent mode
		bool 	temporary_stop; 	// Temporary stop
		bool 	path3_interrupt; 	// PATH3 interrupted (by intermittent mode?)
		bool 	path3_queued; 		// PATH3 queued
		bool 	path2_queued; 		// PATH2 queued
		bool 	path1_queued; 		// PATH1 queued
		bool 	output; 			// Output path
		u8 		active : 1; 		// Active path
		bool 	direction; 			// Transfer direction
		u8 		data_count : 4;   	// Data in GIF FIFO		
	};
	u32 value;
};

union GIF_TAG0 {
	struct {
		u16 NLOOP : 15;
		bool EOP;
		u16 tag;
	};
	u32 value;
};

union GIF_TAG1 {
	struct {
		u16 tag : 14;
		bool PRE;
		u16 PRIM : 11;
		bool FLG;
		u8 NREG : 4;
	};
	u32 value;
};

typedef struct GIF_TAG2_t {
	u32 value;
} GIF_TAG2;

typedef struct GIF_TAG3_t {
	u32 value;
} GIF_TAG3;

union GIF_CNT {
	struct {
		u16 loop_count;
		u8 reg_count;
		u16 vu_address;
	};
	u32 value;
};

union GIF_P3CNT {
	struct {
		u16 p3_count : 15;
	};
	u32 value;
};

union GIF_P3TAG {
	struct {
		u16 loop_count : 15;
		bool EOP;
	};
	u32 value;
};

typedef struct GIF_t {
	GIF_CTRL ctrl;    // Control register
	GIF_MODE mode;    // Mode setting
	GIF_STAT stat;    // Status
	GIF_TAG0 tag0;    // Bits 0-31 of tag before
	GIF_TAG1 tag1;    // Bits 32-63 of tag before
	GIF_TAG2 tag2;    // Bits 64-95 of tag before
	GIF_TAG3 tag3;    // Bits 96-127 of tag before
	GIF_CNT cnt;     // Transfer status counter
	GIF_P3CNT p3cnt;   // PATH3 transfer status counter
	GIF_P3TAG p3tag;   // Bits 0-31 of PATH3 tag when interrupted
	GIF_Tag tag[4]; // GIF_TAG 0-4
} GIF;

void 	gif_reset();
u32  	gif_read (u32 address);
void 	gif_write (u32 address, u32 value);
void 	gif_process_path3(u128 data);
void 	gif_fifo_write(u32 address);
void 	gif_fifo_read(u32 address);

#define GIF_H
#endif