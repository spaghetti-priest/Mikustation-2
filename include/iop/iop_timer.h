#ifndef _IOP_TIMER_H
#define _IOP_TIMER_H

#include <cstdint>

enum Timers {
	TIMER_0,
	TIMER_1,
	TIMER_2,
	TIMER_3,
	TIMER_4,
	TIMER_5,
};

union TN_COUNT {
	uint32_t value;
};

union TN_MODE {
	uint32_t value;
};

union TN_TARGET {
	uint32_t value;
};
#endif