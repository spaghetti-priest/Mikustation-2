#pragma once

#ifndef PS2_H
#define PS2_H

#include "SDL2/include/SDL.h"
/*
typedef struct _R5900_ {
    R5900_Core ee_core;
} R5900;
*/

// @Cleanup: Maybe this is the best place to place this?
typedef struct SDL_Backbuffer {
    uint32_t w;
    uint32_t h;
    uint32_t pixel_format;
    uint32_t pitch;
    uint32_t *pixels;
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
    uint32_t start;
    uint32_t size;
    
    inline Range(uint32_t _start, uint32_t _size) : start(_start), size(_size) {}
    inline bool contains (uint32_t addr);
} Range;

inline bool Range::contains (uint32_t addr) 
{
    uint32_t range  = start + size;
    bool contains   = (addr >= start) && (addr < range);

    return contains;
}

uint8_t     ee_load_8  (uint32_t address); 
uint16_t    ee_load_16 (uint32_t address); 
uint32_t    ee_load_32 (uint32_t address); 
uint64_t    ee_load_64 (uint32_t address);
//void      ee_load_128() {return;}

void        ee_store_8  (uint32_t address, uint8_t value);
void        ee_store_16 (uint32_t address, uint16_t value);
void        ee_store_32 (uint32_t address, uint32_t value);
void        ee_store_64 (uint32_t address, uint64_t value);

uint8_t     iop_load_8 (uint32_t address);
uint16_t    iop_load_16 (uint32_t address);
uint32_t    iop_load_32 (uint32_t address);

void        iop_store_8 (uint32_t address, uint8_t value);
void        iop_store_16 (uint32_t address, uint16_t value);
void        iop_store_32 (uint32_t address, uint32_t value);

uint32_t    check_interrupt (bool value, bool int0_priority, bool int1_priorirty);
 
#endif