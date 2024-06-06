#pragma once
#ifndef INTC_H
#define INTC_H

#include "ps2types.h"

global_variable u32 INTC_MASK = 0; 
global_variable u32 INTC_STAT = 0;

enum INTCRequests : int {
	INT_GS, 	INT_SBUS, 	INT_VB_ON, 	INT_VB_OFF,
	INT_VIF0, 	INT_VIF1, 	INT_VU0, 	INT_VU1,
	INT_IPU, 	INT_TIMER0, INT_TIMER1,	INT_TIMER2,
	INT_TIMER3,	INT_SFIFO,	INT_VU0WD
};

u32 intc_read(u32 address);
void intc_write(u32 address, u32 value);

#endif