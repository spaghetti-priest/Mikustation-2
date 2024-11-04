/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#ifndef TIMER_H
#define TIMER_H

#include "../ps2types.h"
#include "dmac.h"

// From: https://github.com/ps2dev/ps2sdk/blob/master/ee/kernel/include/timer.h#L53
#define EECLK 		   (294912000)
#define BUSCLK         (147456000)
#define BUSCLKBY16     (BUSCLK / 16)
#define BUSCLKBY256    (BUSCLK / 256)
#define HBLNK_NTSC     (15734)

/* 
	There should be an option in a menu of somesort to figure out the encoding system
	idk yet so currently NTSC is the default
*/
#define HBLNK_PAL      (15625)
#define HBLNK_DTV480p  (31469)
#define HBLNK_DTV1080i (33750)

enum Timers {
    TIMER_1 = 0x10000000, TIMER_2 = 0x10000800,
    TIMER_3 = 0x10001000, TIMER_4 = 0x10001800
};

union Tn_MODE {
	struct {
		u8 clock_selection : 2;
		bool gate_function_enable;
		bool gate_selection;
		u8 gate_mode : 2;
		bool zero_return;
		bool count_enable;
		bool compare_interrupt;
		bool overflow_interrupt;
		bool equal_flag;
		bool overflow_flag;
		bool unused[19];
	};
	u32 value;
};

union Tn_COUNT {
	struct {
		u16 count;
		u16 unused;
	};
	u32 value;
};

union Tn_COMP {
	struct {
		u16 compare;
		u16 unused;
	};
	u32 value;
};

union Tn_HOLD {
	struct {
		u16 hold;
		u16 unused;
	};
	u32 value;
};

typedef struct _Timer_ {
	union Tn_MODE mode;
	union Tn_COUNT count;
	union Tn_COMP comp;
	union Tn_HOLD hold;

	u32 counter;
	u32 prescaler;
} Timer;

void timer_reset();
u32 timer_read(u32 address);
void timer_write(u32 address, u32 value);
void timer_tick();

#endif