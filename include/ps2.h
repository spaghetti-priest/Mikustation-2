#pragma once

#ifndef PS2_H
#define PS2_H
#include "ps2types.h"
#include "ee/r5900Interpreter.h"

typedef struct _R5900_ {
    R5900_Core ee_core;
} R5900;

u8  ee_load_8  (u32 address); 
u16 ee_load_16 (u32 address); 
u32 ee_load_32 (u32 address); 
u64 ee_load_64 (u32 address);
//void ee_load_128() {return;}

void ee_store_8  (u32 address, u8 value);
void ee_store_16 (u32 address, u16 value);
void ee_store_32 (u32 address, u32 value);
void ee_store_64 (u32 address, u64 value);

u8  iop_load_8 (u32 address);
u16 iop_load_16 (u32 address);
u32 iop_load_32 (u32 address);

void iop_store_8 (u32 address, u8 value);
void iop_store_16 (u32 address, u16 value);
void iop_store_32 (u32 address, u32 value);

u32 check_interrupt(bool value);

void bios_hle_syscall(R5900_Core *ee, u32 call);

#endif