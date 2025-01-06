#pragma once

#ifndef GS_H
#define GS_H

#include <cstdint>
#include "../ps2types.h"
#include "../ps2.h"
/*
  00h     PRIM
  01h     RGBAQ
  02h     ST
  03h     UV
  04h     XYZF2
  05h     XYZ2
  06h/07h TEX0_1/2
  08h/09h CLAMP_1/2
  0Ah     FOG
  0Ch     XYZF3
  0Dh     XYZ3
  14h/15h TEX1_1/2
  16h/17h TEX2_1/2
  18h/19h XYOFFSET_1/2
  1Ah     PRMODECONT
  1Bh     PRMODE
  1Ch     TEXCLUT
  22h     SCANMSK
  34h/35h MIPTBP1_1/2
  36h/37h MIPTBP2_1/2
  3Bh     TEXA
  3Dh     FOGCOL
  3Fh     TEXFLUSH
  40h/41h SCISSOR_1/2
  42h/43h ALPHA_1/2
  44h     DIMX
  45h     DTHE
  46h     COLCLAMP
  47h/48h TEST_1/2
  49h     PABE
  4Ah/4Bh FBA_1/2
  4Ch/4Dh FRAME_1/2
  4Eh/4Fh ZBUF_1/2
  50h     BITBLTBUF
  51h     TRXPOS
  52h     TRXREG
  53h     TRXDIR
  54h     HWREG
  60h     SIGNAL
  61h     FINISH
  62h     LABEL
*/

/********************************
 * GS Primitives
********************************/		
union PRIM {
	struct {
		u8 primitive_type : 3;
		bool shading_method;
		bool do_texture_mapping;
		bool do_fogging;
		bool do_alpha_blending;
		bool do_1_pass_antialiasing;
		bool mapping_method;
		bool context;
		bool fragment_value_control;
		u64 unused : 53;
	};
	u64 value;
};

union PRMODE {
	struct {
		u8 unused : 3;
		bool shading_method;
		bool do_texture_mapping;
		bool do_fogging;
		bool do_alpha_blending;
		bool do_1_pass_antialiasing;
		bool mapping_method;
		bool context;
		bool fragment_value_control;
		u64 unused1 : 53;
	};
	u64 value;
};

union PRMODECONT {
	struct {
		bool specify_prim_register;
		u64 unused : 63;
	};
	u64 value;
};

/********************************
 * GS Vertex Attributes
********************************/	
union RGBAQ {
	struct {
		u8 r;
		u8 g;
		u8 b;
		u8 a;
		f32 q;
	};
	u64 value;
};

union XYZF {
	struct {
		s16 x;
		s16 y;
		u32 z : 24;
		u8 f;
	};
	u64 value;
};

union XYZ {
	struct {
		s16 x;
		s16 y;
		u32 z;
	};
	u64 value;
};

union XYOFFSET {
	struct {
		u16 x;
		u16 unused0;
		u16 y;
		u16 unused1;
	};
	u64 value;
};

/********************************
 * GS Frame and Z Buffers
********************************/	
union FRAME {
	struct {
		u16 base_pointer : 9;		u8 unused0 : 7;
		// u16 buffer_width : 6;		u8 unused1 : 2;
		u16 buffer_width ;			u8 unused1 : 2;
		u16 storage_format : 6;	u8 unused2 : 2;
		u32 drawing_mask;
	};
	u64 value;
};

union ZBUF {
	struct {
		u16 base_pointer : 9;		u16 unused0 : 15;
		u8 storage_format : 4;	u8 unused1 : 4;
		bool z_value_mask;			u32 unused2 : 31;
	};
	u64 value;
};

/********************************
 * GS Transfers
********************************/	
union BITBLTBUF {
	struct {
		u16 src_base_pointer : 14;	u8 unused0 : 3;
		// u8 src_buffer_width : 6;		u8 unused1 : 3;
		u8 src_buffer_width;			u8 unused1 : 3;
		u8 src_storage_format : 6;	u8 unused2 : 3;
		u16 dest_base_pointer : 14;	u8 unused3 : 3;
		// u8 dest_buffer_width : 6;		u8 unused4 : 3;
		u16 dest_buffer_width;		u8 unused4 : 3;
		u8 dest_storage_format : 6;	u8 unused5 : 3;
	};
	u64 value;
};

typedef struct HWREG {
	u64 data;	
} HWREG;

union TRXPOS {
	struct {
		u16 src_x_coord : 11;		u8 unused0 : 6;
		u16 src_y_coord : 11;		u8 unused1 : 6;
		u16 dest_x_coord : 11;	u8 unused2 : 6;
		u16 dest_y_coord : 11;
		u8 transmission_direction : 2; u8 unused3 : 2;
	};
	u64 value;
};

union TRXREG {
	struct {
		u16 width : 12;
		u32 unused0 : 21;
		u16 height : 12;
		u32 unused1 : 21;
	};
	u64 value;
};

union TRXDIR {
	struct {
		u8 direction : 2;
		u64 unused : 62;
	};
	u64 value;
};

/********************************
 * GS Textures
********************************/	
union TEX0 {
	struct {
		u16 base_pointer : 14; 
		u8 buffer_width : 6; 
		u8 pixel_storage_format : 6; 
		u8 texture_width : 4;
		u8 texture_height : 4;
		bool color_component;
		u8 texture_function : 2;
		u16 clut_base_pointer : 14;
		u8 clut_storage_format : 4;
		bool clut_storage_mode;
		u8 clut_entry_offset : 5;
		u8 clut_load_control : 3;
	};
	u64 value;
};

union TEX1 {
	struct {
		bool lod_method;
		u8 mip_max_level : 3;
		bool texture_mag; 
		u8 texture_min : 3;
		bool MTBA;
		u8 L : 2;
		u16 K : 12; 
	};
	u64 value;
};

union TEX2 {
	struct {
		u32 unused0 : 19;
		u8 pixel_storage_format : 6;
		u16 unused1 : 12;
		u16 clut_base_pointer : 14;
		u8 clut_storage_format : 4; 
		bool clut_storage_mode;
		u8 clut_entry_offset : 5;
		u8 clut_load_control : 3;
	};
	u64 value;
};

union TEXCLUT {
	struct {
		u8 buffer_width : 6;
		u8 u_offset : 6;
		u16 v_offset : 10;
		u64 unused : 40;
	};
	u64 value;
};

union ST {
	struct {
		f32 s;
		f32 t;
	};
	u64 value;
};

union UV {
	struct {
		u16 u : 14;
		u16 v : 14;

	};
	u64 value;
};

union MIPTBP1 {
	struct {
		u16 level1_base_pointer : 14;		u8 	level1_buffer_width : 6;
		u16 level2_base_pointer : 14;		u8 	level2_buffer_width : 6;
		u16 level3_base_pointer : 14;		u8 	level3_buffer_width : 6;
		u8 unused : 4;
	};
	u64 value;
};

union MIPTBP2 {
	struct {
		u16 level4_base_pointer : 14;		u8 	level4_buffer_width : 6;
		u16 level5_base_pointer : 14;		u8 	level5_buffer_width : 6;
		u16 level6_base_pointer : 14;		u8 	level6_buffer_width : 6;
		u8 unused : 4;
	};
	u64 value;
};

union CLAMP {
	struct {
		u8 horizontal_wrap_mode : 2;
		u8 vertical_wrap_mode : 2;
		u16 u_clamp_min : 10;		u16 u_clmap_max : 10;
		u16 v_clamp_min : 10;		u16 v_clmap_max : 10;
		u32 unused : 20;
	};
	u64 value;
};

union TEXA {
	struct {
		u8 alpha_value_field0;
		u16 unused0 : 7;
		bool expansion_method;
		u16 unused1;
		u8 alpha_value_field1;
		u32 unused2 : 24;
	};
	u64 value;
};

// Anything can be written to this register
typedef struct TEXFLUSH {
	u64 value;
} TEXFLUSH;

/********************************
 * GS Fog
********************************/	
union FOG {
	struct {
		u64 unused : 56;
		u8 fog;
	};
	u64 value;
};

union FOGCOL {
	struct {
		u8 r;
		u8 g;
		u8 b;
		u64 unused : 40;
	};
	u64 value;
};

/********************************
 * GS Alpha Blending
********************************/	
union ALPHA {
	struct {
		u8 a : 2;
		u8 b : 2;
		u8 c : 2;
		u8 d : 2;
		u32 unused : 24;
		u8 fixed_value;
		u32 unused1 : 24;
	};
	u64 value;
};

union COLCLAMP {
	struct {
		bool clamp_method;
		u64 unused : 63;
	};
	u64 value;
};

union PABE {
	struct {
		bool pixel_alpha_blending;
		u64 unused : 63;
	};
	u64 value;
};

union FBA {
	struct {
		bool framebuffer_alpha;
		u64 unused : 63;
	};
	u64 value;
};

/********************************
 * GS Tests and Pixel Control
********************************/
union SCISSOR {
	struct {
		u16 min_x 	: 11;		u8 unused0 	: 6;
		u16 max_x 	: 11;		u8 unused1 	: 6;
		u16 min_y 	: 11;		u8 unused2 	: 6;
		u16 max_y 	: 11;		u8 unused3 	: 6;
	};
	u64 value;
};

union TEST {
	struct {
		bool alpha_test;
		u8 alpha_test_method : 3;
		u8 alpha_comparison_value;
		u8 alpha_fail_method : 2;
		bool destination_test;
		bool destination_test_mode;
		bool depth_test;
		u8 depth_test_method : 2;
		u64 unused : 45;
	};
	u64 value;
};

union SCANMSK {
	struct {
		u8 mask : 2;
		u64 unused : 62;
	};
	u64 value;
};

union DIMX {
	struct {
		u8 dm00 : 3;  bool unused0;
		u8 dm01 : 3;  bool unused1;
		u8 dm02 : 3;  bool unused2;
		u8 dm03 : 3;	bool unused3;
		u8 dm10 : 3;	bool unused4;
		u8 dm11 : 3;	bool unused5;
		u8 dm12 : 3;	bool unused6;
		u8 dm13 : 3;	bool unused7;
		u8 dm20 : 3;	bool unused8;
		u8 dm21 : 3;	bool unused9;
		u8 dm22 : 3;	bool unused10;
		u8 dm23 : 3;	bool unused11;
		u8 dm30 : 3;	bool unused12;
		u8 dm31 : 3;	bool unused13;
		u8 dm32 : 3;	bool unused14;
		u8 dm33 : 3;	bool unused15;
	};
	u64 value;
};

union DTHE {
	struct {
		bool control;
		u64 unused : 63;
	};
	u64 value;
};

union SIGNAL {
	struct {
		u32 id;
		u32 mask;
	};
	u64 value;
};

typedef struct FINISH {
	u64 data;
} FINISH;

union LABEL {
	struct {
		u32 id;
		u32 mask;
	};
	u64 value;
};
/*
  12000000h    PMODE
  12000010h    SMODE1
  12000020h    SMODE2
  12000070h    DISPFB1
  12000080h    DISPLAY1
  12000090h    DISPFB2
  120000A0h    DISPLAY2
  120000B0h    EXTBUF
  120000C0h    EXTDATA
  120000D0h    EXTWRITE
  120000E0h    BGCOLOR
  12001000h    CSR
  12001010h    IMR
  12001040h    BUSDIR
  12001080h    SIGLBLID
*/

// @@Accuracy: Should go back and implement unused bits in these registers
// Unncessarily did these in an hurry
union PMODE {
	struct {
		bool is_circuit1;
		bool is_circuit2;
		u8 CRT : 3;
		bool value_selection;	
		bool output_selection;	
		bool blending_selection;
		u8 alpha_value;	
		u16 unused;
	};
	u64 value;
};

// @@Note @Incomplete: No noted documentation on SMODE1 so this is just copied from PCSX2 
// https://github.com/PCSX2/pcsx2/blob/2d5faa627ff54f3fb2a69a43286181bee071a1c3/pcsx2/GS/GSRegs.h#L461
union SMODE1 {
	struct {
		//bool vertical_front;
		//bool interlace_setting;
		//u8 mode : 2;
	};
	u64 value;
};

union SMODE2 {
	struct {
		bool interlace_mode;
		bool interlace_setting;
		u8 mode : 2;
	};
	u64 value;
};

union DISPFB {
	struct {
		u16 base_pointer : 9;
		// u8  buffer_width : 6;
		u16  buffer_width;
		u8  storage_formats : 5;
		u16 x_position : 11;
		u16 y_position : 11;
	};
	u64 value;
};

union DISPLAY {
	struct {
		u16 x_position : 12;
		u16 y_position : 12;
		u8  h_magnification : 4;
		u8  v_magnification : 4;
		u16 width  : 12;
		u16 height : 11;
	};
	u64 value;
};

union EXTDATA {
	struct {
		u16 x_coord : 12;
		u16 y_coord : 11;
		u8 h_sampling_rate : 4;
		u8 v_sampling_rate : 2;
		u16 width : 12;
		u16 height : 11;
	};
	u64 value;
};

union EXTBUF {
	struct {
		u16 base_pointer : 14;
		u8 buffer_width : 6;
		u8 input_selection : 2;
		bool interlace_mode;
		u8 input_alpha_method : 2;
		u8 input_color_method : 2;
		u16 x_coord : 11;
		u16 y_coord : 11;
	};
	u64 value;
};

typedef struct _EXTWRITE_ {
	bool activated;
} EXTWRITE;

typedef struct _BGCOLOR_ {
	u8 red;
	u8 green;
	u8 blue;
} BGCOLOR;

union CSR {
	struct {
		bool signal;
		bool finish;
		bool h_interrupt;
		bool v_interrupt;
		bool write_termination_control;
		bool unused[2];
		bool flush;
		bool reset;
		bool NFIELD_output;
		bool field_display;
		u8 FIFO_status : 2;
		u8 revision_num;
		u8 id;
	};
	u64 value;
};

union IMR {
	struct {
		bool signal_mask;
		bool finish_mask;
		bool hsync_mask;
		bool vsync_mask;
		bool write_termination_mask;
		bool unused[2]; //@@Note: These should automatically be set to 1
	};
	u64 value;
};

union SYNCH {
	//@@Note @Incomplete: Dont know what this is. No doc
	u32 value;
};

union SRFSH {
	//@@Note @Incomplete: Dont know what this is. No doc
	u32 value;
};

union SYNCV {
	//@@Note @Incomplete: Dont know what this is. No doc
	u32 value;
};

typedef struct _BUSDIR_ {
	bool direction;
} BUSDIR;

union SIGLBLID {
	struct {
		u32 signal_id;
		u32 label_id;
	};
	u64 value;
};

enum CRT_MODE : int {
	CRT_MODE_NTSC				= 0x02,
	CRT_MODE_PAL 				= 0x03,
	CRT_MODE_DTV_480P 	= 0x50
};

typedef struct _Context_ {
	XYZF 			xyzf;
	XYZ 			xyz;
	TEX0 			tex0;
	CLAMP 		clamp;
	TEX1 			tex1;
	TEX2 			tex2;
	XYOFFSET 	xyoffset;
	MIPTBP1 	miptbp1;
	MIPTBP2 	miptbp2;
	SCISSOR 	scissor;
	ALPHA 		alpha;
	TEST 			test;
	FRAME 		frame;
	ZBUF 			zbuf;
} Context;

struct Transmission_Buffer {
    u32 row;
    u32 pitch;
    u32 pixel_count;
    u32 address;
};

typedef struct _GraphicsSynthesizer_ {
	u32 *vram;
	CRT_MODE crt_mode;
	Transmission_Buffer transmission_buffer;

	Context context[2];
	// GS Internal Registers
  PRIM prim;
  RGBAQ rgbaq;
  ST st;
  UV uv;
  XYZF xyzf2; //@@Remove
  XYZF xyzf3; //@@Remove
  XYZ xyz2; //@@Remove
  XYZ xyz3; //@@Remove
  TEX0 tex0_1; //@@Remove
  TEX0 tex0_2; //@@Remove
  CLAMP clamp_1; //@@Remove
  CLAMP clamp_2; //@@Remove
  FOG fog;
  TEX1 tex1_1;  //@@Remove
  TEX1 tex1_2; //@@Remove
  TEX2 tex2_1; //@@Remove
  TEX2 tex2_2; //@@Remove
  XYOFFSET xyoffset_1; //@@Remove
  XYOFFSET xyoffset_2; //@@Remove
  PRMODECONT prmodecont;
  PRMODE prmode;
  TEXCLUT texclut;
  SCANMSK scanmsk;
  MIPTBP1 miptbp1_1; //@@Remove
  MIPTBP1 miptbp1_2; //@@Remove
  MIPTBP2 miptbp2_1; //@@Remove
  MIPTBP2 miptbp2_2; //@@Remove
  TEXA texa;
  FOGCOL fogcol;
  TEXFLUSH texflush;
  SCISSOR scissor_1; //@@Remove
  SCISSOR scissor_2; //@@Remove
  ALPHA alpha_1; //@@Remove
  ALPHA alpha_2; //@@Remove
  DIMX dimx;
  DTHE dthe;
  COLCLAMP colclamp;
  TEST test_1; //@@Remove
  TEST test_2; //@@Remove
  PABE pabe;
  FBA fba_1; //@@Remove
  FBA FBA_2; //@@Remove
  FRAME frame_1; //@@Remove
  FRAME frame_2; //@@Remove
  ZBUF zbuf_1; //@@Remove
  ZBUF zbuf_2; //@@Remove
  BITBLTBUF bitbltbuf;
  TRXPOS trxpos; 
  TRXREG trxreg;
  TRXDIR trxdir;
  HWREG hwreg;
  SIGNAL signal;
  FINISH finish;
  LABEL label;

	// EE Privileged Registers
	PMODE 	 pmode;
	DISPFB 	 dispfb1; 
	DISPFB 	 dispfb2;
	SMODE1 	 smode1;
	SMODE2 	 smode2;
	DISPLAY  display1;
	DISPLAY  display2;
	EXTDATA  extdata;
	EXTBUF 	 extbuf;
	EXTWRITE extwrite;
	BGCOLOR  bgcolor;
	CSR 	 	 csr;
	IMR 	 	 imr;
	BUSDIR 	 busdir;
	SIGLBLID siglbid;
	SRFSH srfsh;
	SYNCH synch1;
	SYNCH synch2;
	SYNCV syncv;
} GraphicsSynthesizer;

void gs_reset();
void gs_shutdown();

u32 gs_read_32_priviledged(u32 address);
u64 gs_read_64_priviledged(u32 address);
void gs_write_32_priviledged(u32 address, u32 value);
void gs_write_64_priviledged(u32 address, u64 value);

void gs_write_internal(u8 address, u64 value);

void gs_set_primitive(u64 prim_register);
void gs_set_q (f32 value);
void gs_set_rgbaq (u8 r, u8 g, u8 b, u8 a);
void gs_set_st (f32 s, f32 t);
void gs_set_uv (u32 u, u32 v);
void gs_set_fog (u8 fog);
void gs_set_xyzf2(s16 x, s16 y, u32 z, u8 f);
void gs_set_xyzf3(s16 x, s16 y, u32 z, u8 f);
void gs_set_xyz2(s16 x, s16 y, u32 z);
void gs_set_xyz3(s16 x, s16 y, u32 z);
void gs_set_crt(bool interlaced, s32 display_mode, bool ffmd);
void gs_write_hwreg(u64 data);

void gs_render_crt(SDL_Context *context);

#endif