#ifndef _IOP_DMAC_H
#define _IOP_DMAC_H

#include "../ps2types.h"

void 	iop_dmac_write_32(u32 address, u32 value);
u32 	iop_dmac_read_32(u32 address);

#endif