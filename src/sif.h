#pragma once

// #include "ps2types.h"

#ifndef SIF_H
#define SIF_H

// From ps2tek: EE base is at 1000F200h, IOP base is at 1D000000h.
struct Sif {
	u32 mscom;
	u32 smcom;
	u32 msflg;
	u32 smflg;
	u32 ctrl;
	u32 bd6;
};


void sif_write(u32 address, u32 value);
u32  sif_read(u32 address);
#endif