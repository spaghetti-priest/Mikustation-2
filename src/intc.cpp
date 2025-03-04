/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include "../include/intc.h"
#include "../include/ps2.h"
#include "../include/common.h"
#include "../include/ps2types.h"
#include <iostream>

Intc_Handler intc_handler = {0};

void
intc_reset()
{
	memset(&intc_handler, 0, sizeof(Intc_Handler));
	syslog("Resetting Interrupt Controller\n");
}

static u32
trigger_interrupt_int0 ()
{
	bool int0 	= intc_handler.mask & intc_handler.stat;
	u32 r 		= check_interrupt(int0, true, false);
	if (r == 0)
		return 0;
	else 
		syslog("[INT0]: Interrupt triggered\n");
}

void
request_interrupt (u32 index)
{
	intc_handler.stat |= 1 << index;
	trigger_interrupt_int0();
}

u32 
intc_read (u32 address)
{
	switch(address) 
	{
		case 0x1000f000:
        {
            syslog("READ: INTC_STAT is [{:#x}]\n", intc_handler.stat);
            return intc_handler.stat;
        } break;

        case 0x1000f010:
        {
            //syslog("READ: INTC_MASK is [{:#x}]\n", intc_handler.mask);
            return intc_handler.mask;
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
	        syslog("WRITE: INTC_STAT [{:#x}]\n", value);
			intc_handler.stat &= (~value & 0x7ff);
			trigger_interrupt_int0();
			return;			
		}

		case 0x1000f010: 
		{
			syslog("WRITE: INTC_MASK [{:#x}]\n", value);
			intc_handler.mask ^= (value & 0x7ff);
			trigger_interrupt_int0();
			return;	
		}
	}
	return;
}
