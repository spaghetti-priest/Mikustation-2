#ifndef INTC_H
#define INTC_H

struct Intc_Handler
{
	u32 mask;
	u32 stat;
};

enum INTCRequests : u32 {
	INT_GS 		= 0, 	INT_SBUS 	= 1, 	INT_VB_ON 	= 2, 	INT_VB_OFF 	= 3,
	INT_VIF0 	= 4, 	INT_VIF1 	= 5, 	INT_VU0 	= 6, 	INT_VU1 	= 7,
	INT_IPU 	= 8, 	INT_TIMER0 	= 9,    INT_TIMER1 	= 10,	INT_TIMER2 	= 11,
	INT_TIMER3 	= 12,	INT_SFIFO 	= 13,	INT_VU0WD 	= 14
};

void 	intc_reset();
u32 	intc_read(u32 address);
void 	intc_write(u32 address, u32 value);
void 	request_interrupt(u32 value);

#endif