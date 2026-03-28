/*
__________________________________________________________
 ___________    _______   ____  _____   __    __
/\          \  /\__  __\ /\   \/ ____\ /\ \  /  \
\ \___  __ __\ \/_/\ \-/ \ \   \/____/ \ \ \ \   \
 \ \  \ \ \\  \   __\_\__ \ \   \_____  \ \_\_____\
  \ \__\ \_\\__\ /\-_____\ \ \___\____\  \ \_______\
   \/_\/\/_//__/ \/______/  \/___/____/   \/_______/  Mikustation 2: Playstation 2 Emulator
___________________________________________________________

-----------------------------------------------------------------------------
* Copyright (c) 2023-2024 Xaviar Roach
* SPD X-License-Identifier: MIT
*/
#pragma warning(disable:4311 312)

// #define global_variable static
// #define function static

#include <thread>
#include <typeinfo>
#include <assert.h>
#include <cmath>
#include <fstream>
#include <cstring>
#include <queue>

#include "SDL2/include/SDL.h"
#include "SDL2/include/SDL_timer.h"

#include "fmt-10.2.1/include/fmt/core.h"
#include "fmt-10.2.1/include/fmt/color.h"
#include "fmt-10.2.1/include/fmt/format-inl.h"
#include "fmt-10.2.1/include/fmt/format.h"
#include "fmt-10.2.1/src/format.cc"

#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#include <glad/glad.h>

// @Temporary: Maybe move this into the debugtools folder inc?
#include "imgui/backends/imgui_impl_sdl2.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/backends/imgui_impl_sdl2.cpp"
#include "imgui/backends/imgui_impl_opengl3.cpp"

//#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_draw.cpp"
#include "imgui_tables.cpp"
#include "imgui_widgets.cpp"
//#include "imgui.cpp"

#include "ps2types.h"
#include "common.h"
#include "loader.h"
#include "ps2.h"

#include "ee/ee_inc.h"
#include "iop/iop_inc.h"

#include "bus.h"
#include "kernel.h"
#include "gif.h"
#include "sif.h"
#include "dmac.h"
#include "intc.h"
#include "ipu.h"
#include "vu.h"
#include "vif.h"
#include "gs/gs_inc.h"

#include "bus.cpp"
#include "kernel.cpp"
#include "ee/ee_inc.cpp"
#include "iop/iop_inc.cpp"
#include "loader.cpp"

#include "gif.cpp"
#include "sif.cpp"
#include "dmac.cpp"
#include "intc.cpp"
#include "ipu.cpp"
#include "vu.cpp"
#include "vif.cpp"
#include "gs/gs_inc.cpp"

alignas(16) R5900_Core ee = {};

u32
check_interrupt (bool value, bool int0_priority, bool int1_priorirty)
{
   Exception e = get_exception(V_INTERRUPT, __INTERRUPT);
   if (!value)
      return 0;

   //@@Note: Not sure what to do when they both assert interrupts
   // Edge triggered interrupt
   if (int0_priority) {
      ee.cop0.status.IM_2         = 1;
      ee.cop0.cause.int0_pending &= ~1;
      ee.cop0.cause.int0_pending |= value;
      printf("Asserting INT0 signal\n");
   } else if (int1_priorirty) {
      ee.cop0.status.IM_3         = true;
      ee.cop0.cause.int1_pending  &= ~true;
      ee.cop0.cause.int1_pending  |= ~true;
      printf("Asserting INT1 signal\n");
   }

   bool interrupt_enable               = ee.cop0.status.IE && ee.cop0.status.EIE;
   bool check_exception_error_level    = ee.cop0.status.ERL && ee.cop0.status.EXL;
   interrupt_enable                    = interrupt_enable && !check_exception_error_level;

   if (!interrupt_enable)
      return 0;

   handle_exception_level_1(&ee.cop0, &e, ee.pc, ee.is_branching);
   return 1;
}

// @Implementation: This would be eventually defined in the command line arguement
// #define USE_SOFTWARE 1
#define USE_HARDWARE 1

inline void
swap_framebuffers (SDL_Window *window, SDL_Backbuffer *backbuffer, SDL_Surface *surface)
{
#if USE_SOFTWARE
   SDL_UnlockSurface(surface);

   u32 mem_size = backbuffer->w * sizeof(u32) * backbuffer->h;
   memcpy(surface->pixels, backbuffer->pixels, mem_size);

   SDL_LockSurface(surface);
   SDL_UpdateWindowSurface(window);
#endif
}

int
main (int argc, char **argv)
{
   const char *elf_filename = "";
   /*
   *   @Note: Eventually the full current implementation of executing from the command line should be this:
   *   Mikustation-2.exe [bios_filename] [elf_filename] --USE_SOFTWARE_RENDERING or --USE_HARDWARE_RENDERING
   *   This is before the implementation of the cdvd so things might become a little bit more crazy
   */

   // We dont support full games yet so the BIOS file is not required to load
/*
   {
      bool err = false;
      if (argc < 2) {
         printf("[ERROR]: ELF filename has not been detected\n");
         err = true;
      } else if (argc > 2) {
         printf("[ERROR]: Too many arguments!\n");
         err = true;
      }*/
      /*if (err) {
         printf("Usage: Mikustation-2 [filename]\n");
         return 0;
      }*/
/*
       elf_filename = (const char*)argv[1];
      printf("%s\n", elf_filename);
   }
*/
   // @Remove: For debug and iteration purposes. Just hardcode this into the program
   // elf_filename = "..\\Mikustation-2\\data\\ps2tut\\ps2tut_01\\demo1.elf";
   elf_filename = "..\\Mikustation-2\\data\\ps2tut\\ps2tut_02a\\demo2a.elf";
   //elf_filename = "..\\Mikustation-2\\data\\cube\\cube.elf";
   // const char *bios_filename = "..\\Mikustation-2\\data\\bios\\scph10000.bin";
   const char *bios_filename = "..\\Mikustation-2\\data\\bios\\scph39001.bin";

   SDL_Context     main_context = {};
   SDL_Event       event        = {};
   SDL_Window      *window      = NULL;
   SDL_Surface     *surface     = NULL;
   bool running                 = true;
   bool left_down               = false;
   u32 instructions_run         = 0;

   const int screen_w = 640;
   const int screen_h = 480;

   if (SDL_Init(SDL_INIT_VIDEO) != 0) {
     printf("Error: %s\n", SDL_GetError());
     return -1;
   }

   u32 window_flags;
#if USE_SOFTWARE
   window_flags = 0;
#elif USE_HARDWARE
   window_flags = SDL_WINDOW_OPENGL;
#endif

   window = SDL_CreateWindow("Mikustation 2\n",
                           SDL_WINDOWPOS_UNDEFINED,
                           SDL_WINDOWPOS_UNDEFINED,
                           screen_w,
                           screen_h,
                           window_flags);
#if USE_SOFTWARE
   surface = SDL_GetWindowSurface(window);

   if (!surface) {
      printf("[ERROR]: %s\n", SDL_GetError());
      return 0;
   }
#endif

   main_context = {
      .event      = &event,
      .window     = window,
      .surface    = surface,
   };

   bool success = false;

#ifdef USE_HARDWARE
   success = init_opengl(&main_context, screen_w, screen_h );
#endif

   printf("===========================================================================\n");
   printf("Mikustation 2: A Playstation 2 Emulator and Debugger\n");
   printf("===========================================================================\n");

   printf("\n=========================\nInitializing System\n=========================\n");
   // @Incomplete: Create a Virtual Memmory map for the VM and map these mallocs to the virtual memory
   _bios_memory_       = (u8 *)malloc(sizeof(u8) * MEGABYTES(4));
   _rdram_             = (u8 *)malloc(sizeof(u8) * MEGABYTES(32));
   _iop_ram_           = (u8 *)malloc(sizeof(u8) * MEGABYTES(2));
   _vu0_code_memory_   = (u8 *)malloc(sizeof(u8) * KILOBYTES(4));
   _vu0_data_memory_   = (u8 *)malloc(sizeof(u8) * KILOBYTES(4));
   _vu1_code_memory_   = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));
   _vu1_data_memory_   = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));

   ee_reset(&ee);
   dmac_reset();
   gs_reset();
   gif_reset();
   intc_reset();
   timer_reset();
   cop1_reset();
   ipu_reset();
   iop_reset();
   vu_reset();
   vif_reset();

   //if (read_bios(bios_filename, _bios_memory_) != 1) return 0;
   load_elf(&ee, elf_filename);

   // u64 begin_time = 0, end_time = 0, delta_time = 0;
   while (running) 
   {
      /* Emulator Step Through*/
      while(SDL_PollEvent(&event)) 
      {
#if 1
         ImGui_ImplSDL2_ProcessEvent(&event);
         switch (event.type)
         {
            //@Incomplete: This takes event control away from this event loop.
            // There isnt any control right now. so anyways i am
            // leaving this note here when controls get implemented
            case SDL_QUIT:
               running = false;
            break;

            case SDL_MOUSEBUTTONUP:
              if (event.button.button == SDL_BUTTON_LEFT)
                left_down = false;
            break;

            case SDL_MOUSEBUTTONDOWN:
              if (event.button.button == SDL_BUTTON_LEFT)
                left_down = true;
            break;

            case SDL_KEYDOWN:
               if (event.key.keysym.sym == SDLK_ESCAPE)
                 running = false;
            break;
         }
#endif
      }

      instructions_run = 0;
      // @Todo: When the backbuffer is resized the bb should retrieve
      // the new screen w and h
      SDL_Backbuffer backbuffer   = {};
      backbuffer.w                = screen_w;
      backbuffer.h                = screen_h;
      backbuffer.pixel_format     = SDL_PIXELFORMAT_ARGB8888;
      backbuffer.pitch            = screen_w * 4;
      backbuffer.pixels           = (u32*)calloc(sizeof(u32), screen_w * screen_h);
      main_context.backbuffer     = &backbuffer;

      /*
      *   @@Note: In dobiestation and chonkystation there is a random instruction limit in order to
      *   synch the ee and iop cycle rate by 1/8 it looks like an arbitrary number.
      */
      while (instructions_run < 1000000) 
      {
         // backbuffer.pixels           = (u32*)malloc(sizeof(u32) * (screen_w * screen_h));

         /*
         *   @Note: This is a performance critical loop so no branching should exist here however the dmac and the iop operate and
         *   half and 1/8 the speed of the ee but since we're not emulating the iop and dmac sufficently we can just ignore these
         *   until a scheduler is written and can be performed better
         */
         r5900_cycle(&ee);
         if (instructions_run % 2 == 0) { dmac_cycle(); }
         timer_tick();
         // if (instructions_run % 8 == 0) { iop_cycle(); }
         instructions_run++;

         // if (intc_read(0x1000f010) & 0x4) {

         if (instructions_run == 975000) {
            // request_interrupt(INT_VB_ON);
            gs_render_crt(&main_context);
   #if USE_HARDWARE
            gl_render_frame();
            gl_swap_framebuffers(main_context.window, &backbuffer);
   #elif USE_SOFTWARE
            swap_framebuffers(main_context.window, &backbuffer, main_context.surface);
   #endif
         }
            // request_interrupt(INT_VB_OFF);
         }
         free(backbuffer.pixels);
   }

   r5900_shutdown();
   gs_shutdown();

#ifdef USE_HARDWARE
    shutdown_opengl();
    imgui_shutdown();
#endif

   free(_bios_memory_);
   free(_rdram_);
   free(_iop_ram_);
   free(_vu0_code_memory_);
   free(_vu0_data_memory_);
   free(_vu1_code_memory_);
   free(_vu1_data_memory_);

   SDL_DestroyWindow(window);
   SDL_Quit();

   return 0;
}

