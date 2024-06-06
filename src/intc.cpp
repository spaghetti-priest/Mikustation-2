/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include "../include/intc.h"
#include "../include/ps2.h"

#include <iostream>

u32
assert_interrupt (u32 value)
{
	bool int0 = INTC_MASK & INTC_STAT;
	u32 r = check_interrupt(int0);
	if (r == 0)
		return 0;
	else 
		printf("Interrupt triggered\n");
}

u32 
intc_read (u32 address)
{
	switch(address) 
	{
		case 0x1000f000:
        {
            printf("INTC_STAT is [%#08x]\n", INTC_STAT);
            return INTC_STAT;
        } break;

        case 0x1000f010:
        {
            printf("INTC_MASK is [%#08x]\n", INTC_MASK);
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
	        printf("Writing to INTC_STAT [%#08x]\n", value);
			INTC_STAT &= (~value & 0x7ff);
			assert_interrupt(INTC_STAT);
			return;			
		}

		case 0x1000f010: 
		{
			printf("Writing to INTC_MASK [%#08x]\n", value);
			INTC_MASK ^= (value & 0x7ff);
			assert_interrupt(INTC_MASK);
			return;	
		}
	}
	return;
}
