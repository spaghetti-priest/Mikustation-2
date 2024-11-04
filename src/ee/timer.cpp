/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include "../include/ee/timer.h"
#include "../include/ee/intc.h"
#include <iostream>

// @@Note: Only Timers 0 and 1 have a Tn_HOLD register
Timer timers[4];

enum Timer_Registers {
	Tn_COUNT = 0x10000000, 	Tn_MODE = 0x10000010,
	Tn_COMP = 0x10000020, 	Tn_HOLD = 0x10000030,
};

void 
timer_reset()
{
	printf("Resetting EE Timers\n");
	memset(&timers, 0, 4 * sizeof(Timer));
}

u32 
timer_read (u32 address)
{
	int index = (address >> 11) & 0x3;
	int reg = (address & ~0x1800);
	//printf("Timer index: [%d] [%04x]\n", index, reg);
	switch(reg)
	{
		case Tn_COUNT: 
		{
			printf("READ: Index [%d] Tn_COUNT\n", index);
			return timers[index].count.value;
		} break;

		case Tn_MODE: 
		{
			printf("READ: Index [%d] Tn_MODE\n", index);
			return timers[index].mode.value;
		} break;

		case Tn_COMP: 
		{
			printf("READ: Index [%d] Tn_COMP\n", index);
			return timers[index].comp.value;
		} break;

		case Tn_HOLD: 
		{
			printf("READ: Index [%d] Tn_HOLD\n", index);
			return timers[index].hold.value;
		} break;
	}
	return 0;
}

void 
timer_write (u32 address, u32 value)
{
	int index = (address >> 11) & 0x3;
	int reg = (address & ~0x1800);
	switch(reg)
	{
		case Tn_COUNT: 
		{
			printf("Timer index: [%d]\n", index);
			printf("WRITE: Tn_COUNT. Value : [%#08x]\n", value);
			timers[index].count.count = value & 0xFFFF;
			return;
		} break;

		case Tn_MODE: 
		{
			printf("Timer index: [%d]\n", index);
			printf("WRITE: Tn_MODE. Value : [%#08x]\n", value);
			timers[index].mode.clock_selection 		= (value >> 0) & 0x3;
			timers[index].mode.gate_function_enable = (value >> 2) & 0x1;
			timers[index].mode.gate_selection 		= (value >> 3) & 0x1;
			timers[index].mode.gate_mode 			= (value >> 4) & 0x3;
			timers[index].mode.zero_return 			= (value >> 6) & 0x1;
			timers[index].mode.count_enable 		= (value >> 7) & 0x1;
			timers[index].mode.compare_interrupt 	= (value >> 8) & 0x1;
			timers[index].mode.overflow_interrupt 	= (value >> 9) & 0x1;
			timers[index].mode.equal_flag 			= (value >> 10) & 0x1;
			timers[index].mode.overflow_flag 		= (value >> 11) & 0x1;
			//timers[index].mode.value 				= value;

			switch(timers[index].mode.clock_selection)
			{
				case 0: { timers[index].prescaler = 1; 		} break;
				case 1: { timers[index].prescaler = 16; 	} break;
				case 2: { timers[index].prescaler = 256; 	} break;
				/* HBLANK NTSC Timings from: https://psi-rockin.github.io/ps2tek/#eetimers */
				case 3: { timers[index].prescaler = 9370; 	} break;
			}
			return;
		} break;

		case Tn_COMP: 
		{
			printf("Timer index: [%d]\n", index);
			printf("WRITE: Tn_COMP. Value : [%#08x]\n", value);
			timers[index].comp.compare = value & 0xFFFF;
			return;
		} break;

		case Tn_HOLD: 
		{
			printf("Timer index: [%d]\n", index);
			printf("WRITE: Tn_HOLD. Value : [%#08x]\n", value);
			timers[index].hold.hold = value & 0xFFFF;
			return;
		} break;
	}
}

void 
timer_tick() 
{
	u32 internal_counter;
	/* Timers are 32 bits but the count register is only 16 bits in length */ 
	u32 overflow = (1 << 16); 
	/*Technically theres 5 timers but this is for 4 */
	for (int i = 0; i <= 3; i++) {
		auto &timer = timers[i];
		if (!timer.mode.count_enable) continue;

		internal_counter = timer.counter;
		timer.counter += 1;

		/* 	We use an internal counter here in order to synchronize the timer with the prescaler ratio
			This is quite expensive so in the future I might change it in accordance with other emulators
			and how they do timing
		*/
		if (internal_counter > timer.prescaler) {
			timer.counter = 0;
			timer.count.count += 1;
		}

		if (timer.count.count == timer.comp.compare) {
			if (timer.mode.compare_interrupt && !timer.mode.equal_flag) {
				/* Edge triggered IRQ */
				timer.mode.equal_flag = 1;
				printf("Trigger compare interrupt\n");
				send_interrupt(INT_TIMER0 + i);
			}

			if (timer.mode.zero_return) timer.count.count = 0;
		}
		
		if (timer.count.count > overflow) {
			if (timer.mode.overflow_interrupt && !timer.mode.overflow_flag) {
				/* Edge triggered IRQ */
				timer.mode.overflow_flag = 1;
				printf("Trigger overflow interrupt");
				send_interrupt(INT_TIMER0 + i);
			}
			timer.count.count = 0;
		}
	}
	return;
}