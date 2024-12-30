#pragma once
#ifndef INTC_H
#define INTC_H

#include <cstdint>

struct Intc_Handler
{
	uint32_t mask;
	uint32_t stat;
};

enum INTCRequests : uint32_t {
	INT_GS 		= 0, 	INT_SBUS 	= 1, 	INT_VB_ON 	= 2, 	INT_VB_OFF 	= 3,
	INT_VIF0 	= 4, 	INT_VIF1 	= 5, 	INT_VU0 	= 6, 	INT_VU1 	= 7,
	INT_IPU 	= 8, 	INT_TIMER0 	= 9,    INT_TIMER1 	= 10,	INT_TIMER2 	= 11,
	INT_TIMER3 	= 12,	INT_SFIFO 	= 13,	INT_VU0WD 	= 14
};

void intc_reset();
uint32_t intc_read(uint32_t address);
void intc_write(uint32_t address, uint32_t value);
void request_interrupt(uint32_t value);

#endif