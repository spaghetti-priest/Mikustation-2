#include "../include/common.h"

static Miku_Fifo 
miku_fifo_initialize (u32 size)
{
	Miku_Fifo fifo 		= {0};
	fifo.entries 		= 0,
	fifo.current_size 	= size,
	fifo.max_size 		= size,
	fifo.head 			= nullptr,
	fifo.tail 			= nullptr,

	return fifo;
}

static inline void miku_fifo_enqueue() {}
static inline void miku_fifo_dequeue() {}
static inline void miku_fifo_get_size() {}
