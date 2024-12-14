#ifndef _IOP_TIMER_H
#define _IOP_TIMER_H

#include "../common.h"

enum Timers {
	TIMER_0,
	TIMER_1,
	TIMER_2,
	TIMER_3,
	TIMER_4,
	TIMER_5,
};

union TN_COUNT {
	u32 value;
};

union TN_MODE {
	u32 value;
};

union TN_TARGET {
	u32 value;
};
#endif