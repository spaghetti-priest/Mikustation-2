/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#ifndef TIMER_H
#define TIMER_H

#include <cstdint>

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
		uint8_t clock_selection : 2;
		bool gate_function_enable;
		bool gate_selection;
		uint8_t gate_mode : 2;
		bool zero_return;
		bool count_enable;
		bool compare_interrupt;
		bool overflow_interrupt;
		bool equal_flag;
		bool overflow_flag;
		bool unused[19];
	};
	uint32_t value;
};

union Tn_COUNT {
	struct {
		uint16_t count;
		uint16_t unused;
	};
	uint32_t value;
};

union Tn_COMP {
	struct {
		uint16_t compare;
		uint16_t unused;
	};
	uint32_t value;
};

union Tn_HOLD {
	struct {
		uint16_t hold;
		uint16_t unused;
	};
	uint32_t value;
};

typedef struct _Timer_ {
	union Tn_MODE mode;
	union Tn_COUNT count;
	union Tn_COMP comp;
	union Tn_HOLD hold;

	uint32_t counter;
	uint32_t prescaler;
} Timer;

void timer_reset();
uint32_t timer_read(uint32_t address);
void timer_write(uint32_t address, uint32_t value);
void timer_tick();

#endif