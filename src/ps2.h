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

u32     check_interrupt (bool value, bool int0_priority, bool int1_priorirty);

#endif