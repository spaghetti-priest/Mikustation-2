/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include "../include/ee/intc.h"
#include "../include/ps2.h"
#include "../include/common.h"
#include <iostream>

void
intc_reset()
{
	INTC_MASK = 0;
	INTC_STAT = 0;
	syslog("Resetting Interrupt Controller\n");
}

static u32
trigger_interrupt_int0 ()
{
	bool int0 = INTC_MASK & INTC_STAT;
	u32 r = check_interrupt(int0, true, false);
	if (r == 0)
		return 0;
	else 
		syslog("Interrupt triggered\n");
}

void
request_interrupt (u32 index)
{
	INTC_STAT |= 1 << index;
	trigger_interrupt_int0();
}

u32 
intc_read (u32 address)
{
	switch(address) 
	{
		case 0x1000f000:
        {
            syslog("INTC_STAT is [{:#x}]\n", INTC_STAT);
            return INTC_STAT;
        } break;

        case 0x1000f010:
        {
            syslog("INTC_MASK is [{:#x}]\n", INTC_MASK);
            return INTC_MASK;
        } break;
	}
	return 0;
}

void 
intc_write (u32 address, u32 value)
{
	switch(address)	
	{
		case 0x1000f000:
		{
	        syslog("Writing to INTC_STAT [{:#x}]\n", value);
			INTC_STAT &= (~value & 0x7ff);
			trigger_interrupt_int0();
			return;			
		}

		case 0x1000f010: 
		{
			syslog("Writing to INTC_MASK [{:#x}]\n", value);
			INTC_MASK ^= (value & 0x7ff);
			trigger_interrupt_int0();
			return;	
		}
	}
	return;
}
