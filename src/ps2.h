#pragma once

#ifndef PS2_H
#define PS2_H

// #include "SDL2/include/SDL.h"
// #include "ps2types.h"
/*
typedef struct _R5900_ {
    R5900_Core ee_core;
} R5900;
*/

// @Cleanup: Maybe this is the best place to place this?
typedef struct SDL_Backbuffer {
    u32 w;
    u32 h;
    u32 pixel_format;
    u32 pitch;
    u32 *pixels;
} SDL_Backbuffer;

typedef struct _SDL_Context_ {
    SDL_Event       *event;
    SDL_Window      *window;
    SDL_Renderer    *renderer;
    SDL_Texture     *texture;
    SDL_Backbuffer  *backbuffer;
    SDL_Surface     *surface; 

    bool            running;
    bool            left_down;
} SDL_Context;

// @Cleanup: This has become redundant

typedef struct Range {
    u32 start;
    u32 size;
    
    inline Range(u32 _start, u32 _size) : start(_start), size(_size) {}
    inline bool contains (u32 addr);
} Range;

inline bool Range::contains (u32 addr) 
{
    u32 range  = start + size;
    bool contains   = (addr >= start) && (addr < range);

    return contains;
}

u8      ee_load_8  (u32 address); 
u16     ee_load_16 (u32 address); 
u32     ee_load_32 (u32 address); 
u64     ee_load_64 (u32 address);
//void      ee_load_128() {return;}

void    ee_store_8  (u32 address, u8 value);
void    ee_store_16 (u32 address, u16 value);
void    ee_store_32 (u32 address, u32 value);
void    ee_store_64 (u32 address, u64 value);

u8      iop_load_8 (u32 address);
u16     iop_load_16 (u32 address);
u32     iop_load_32 (u32 address);

void    iop_store_8 (u32 address, u8 value);
void    iop_store_16 (u32 address, u16 value);
void    iop_store_32 (u32 address, u32 value);

u32     check_interrupt (bool value, bool int0_priority, bool int1_priorirty);
 
#endif