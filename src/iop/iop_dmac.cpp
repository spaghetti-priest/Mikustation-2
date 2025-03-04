// #include "iop_dmac.h"
// #include "ps2types.h"
// #include <iostream>

void 
iop_dmac_write_32 (u32 address, u32 value) 
{
	switch(address)
	{
        default:
        {
            printf("ERROR: UNRECOGNIZED IOP_DMAC_WRITE: address[%#x]\n", address);
        	return;
        } break;
	}
}

u32 
iop_dmac_read_32 (u32 address)
{
	switch(address)
	{
        default:
        {
            printf("ERROR: UNRECOGNIZED IOP_DMAC_READ: address[%#x]\n", address);
        	return 0;
        } break;
	}

	return 0;
}