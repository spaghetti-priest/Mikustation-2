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

#include <thread> 
#include <typeinfo>

#include "../include/ps2.h"
#include "../include/loader.h"
#include "../include/common.h"
#include "../include/ps2types.h"
#include "../include/sif.h"

#include "../include/ee/r5900Interpreter.h"
#include "../include/ee/gs.h"
#include "../include/ee/gif.h"
#include "../include/ee/dmac.h"
#include "../include/ee/intc.h"
#include "../include/ee/cop0.h"
#include "../include/ee/cop1.h"
#include "../include/ee/timer.h"
#include "../include/ee/ipu.h"
#include "../include/ee/vu.h"
#include "../include/ee/vif.h"
#include "../include/iop/iop.h"
#include "../include/iop/iop_dmac.h"


u32 MCH_RICM    = 0, MCH_DRD    = 0;
u8 _rdram_sdevid = 0;

static u8 *_bios_memory_;
static u8 *_rdram_;

static u8 *_vu0_code_memory_;
static u8 *_vu0_data_memory_;
static u8 *_vu1_code_memory_;
static u8 *_vu1_data_memory_;

static u8 *_iop_ram_;

alignas(16) R5900_Core ee = {0};

// EE Virtual/Physical Memory Map:    From: Ps2tek
Range BIOS              = Range(0x1FC00000, MEGABYTES(4));
Range RDRAM             = Range(0x00000000, MEGABYTES(32));
Range IO_REGISTERS      = Range(0x10000000, KILOBYTES(64));
Range VU0_CODE_MEMORY   = Range(0x11000000, KILOBYTES(4));
Range VU0_DATA_MEMORY   = Range(0x11004000, KILOBYTES(4));
Range VU1_CODE_MEMORY   = Range(0x11008000, KILOBYTES(16));
Range VU1_DATA_MEMORY   = Range(0x1100C000, KILOBYTES(16));
Range GS_REGISTERS      = Range(0x12000000, KILOBYTES(8));
Range IOP_RAM           = Range(0x1C000000, MEGABYTES(2));
//Range SCRATCHPAD        = Range(0x70000000, KILOBYTES(16));

// IOP Physical Memory Map.          From:Ps2tek
Range PSX_RAM           = Range(0x00000000, MEGABYTES(2));
Range PSX_IO_REGISTERS  = Range(0x1F800000, KILOBYTES(64));
Range SPU2_REGISTERS    = Range(0x1F900000, KILOBYTES(1));

void 
output_to_console (u32 value)
{
    //console << (char)value;
    //console.flush();
    if ((char)value == '#') printf("\n");
    printf("%c", (char)value);
}
    
void 
iop_output_to_console (u32 iop_pc)
{
    std::string value;
    u32 pointer     = get_iop_register(5);
    u32 text_size   = get_iop_register(6);

    while (text_size) {
          auto c = (char)_iop_ram_[pointer & 0x1FFFFF];
          //putc(c);
          value += c;
          pointer++;
          text_size--;
    }
    printf("{%s}", value.c_str());
}

//@@Temporary: @@Move: Might move this into its own Bus file
u8 
iop_load_8 (u32 address)
{
    u8 r = 0;

    if (PSX_RAM.contains(address))  
        return *(u8*)&_iop_ram_[address];
    if (BIOS.contains(address))     
        return *(u8*)&_bios_memory_[address & 0x3FFFFF];

    errlog("[ERROR]: Could not read iop_load_memory8() at address [{:#09x}]\n", address);

    return r;
}

u16 
iop_load_16 (u32 address)
{
    u16 r = 0;

    if (PSX_RAM.contains(address))  
        return *(u16*)&_iop_ram_[address];
    if (BIOS.contains(address))     
        return *(u16*)&_bios_memory_[address & 0x3FFFFF];
 
    errlog("[ERROR]: Could not read iop_load_memory16() at address [{:#09x}]\n", address);
    
    return r;
}

u32 
iop_load_32 (u32 address)
{
    u32 r = 0;

    switch(address)
    {
        case 0x1f801010: return 0;      break; // BIOS_RAM
    }

    if(address == 0x1D000000) {
        printf("READ: EE->IOP communication\n");
        return 0;
    }

    if ((address >= 0x1F801080 && address <= 0x1F8010EF) || (address >= 0x1F801500 && address <= 0x1F80155F)) {
        return iop_dmac_read_32(address);
    }

    if (PSX_RAM.contains(address))  
        return *(u32*)&_iop_ram_[address];
    if (BIOS.contains(address))     
        return *(u32*)&_bios_memory_[address & 0x3FFFFF];

    errlog("[ERROR]: Could not read iop_load_memory32() at address [{:#09x}]\n", address);

    return r;
}

void 
iop_store_8 (u32 address, u8 value)
{
    if (PSX_RAM.contains(address))  {
      *(u8*)&_iop_ram_[address] = value;
      return;  
    } 

    // @@Note: This is a write to POST2 which is unknown as to what it does?
    if (address == 0x1F802070) return;

    errlog("[ERROR]: Could not write iop_store_memory8() value: [{:#09x}] to address: [{:#09x}] \n", value, address);

}

void 
iop_store_16 (u32 address, u16 value)
{
    if (PSX_RAM.contains(address)) {
      *(u16*)&_iop_ram_[address] = value;
      return;  
    } 

    errlog("[ERROR]: Could not write iop_store_memory16() value: [{:#09x}] to address: [{:#09x}] \n", value, address); 
}

void 
iop_store_32 (u32 address, u32 value)
{
    switch (address) {
        case 0x1f801010: return;    break; // BIOS ROM
        case 0x1f801020: return;    break; // COMMON DELAY
        case 0x1f801004: return;    break; // Expansion 2 Base Address
        case 0x1f80101c: return;    break; // Expansion 2 
        case 0x1f802070: return;    break; // POST2
        case 0x1f801000: return;    break; // Expansion 1 Base Adresss
        case 0x1f801008: return;    break; // Expansion 1 
        case 0x1f801014: return;    break; // SPU_DELAY  
        case 0x1f801018: return;    break; // CDROM_DELAY  
        case 0x1f80100c: return;    break; // Expansion 3  
        case 0x1f8010f0: return;    break; // DMA_Control Register  
        case 0x1f808268: return;    break; // SIO2 control
        case 0x1f801060: return;    break; // RAM SIZE
    }

    u32 iop_pc = get_iop_pc();
    if (iop_pc == 0x12C48 || iop_pc == 0x1420C || iop_pc == 0x1430C) {
        iop_output_to_console(iop_pc);
        return;
    }

    if ((address >= 0x1F801080 && address <= 0x1F8010EF) || (address >= 0x1F801500 && address <= 0x1F80155F)) {
        iop_dmac_write_32(address, value);
        return;
    }

    if (address == 0x1D000010) {
        printf("WRITE: IOP->EE communication\n");
        return;
    } 

    if (PSX_RAM.contains(address))  {
        *(u32*)&_iop_ram_[address] = value;
        return;  
    }

    if (address >= 0x1F808240 && address <= 0x1F80825F) {
        // SIO_SEND1/ SEND2 or Port 1/2 Control
        //printf("SIO registers address [%#x]\n", address);
        return;
    }

    errlog("[ERROR]: Could not write iop_store_memory32() value: [{:#09x}] to address: [{:#09x}] \n", value, address);

}

/*******************************************
 * Load Functions
*******************************************/
u8 
ee_load_8 (u32 address) 
{
    uint8_t r = 0;

    if (BIOS.contains(address)) 
        return *(u8*)&_bios_memory_[address & 0x3FFFFF];
    
    if (RDRAM.contains(address))
        return *(u8*)&_rdram_[address];

    errlog("[ERROR]: Could not read load_memory8() at address [{:#09x}]\n", address);    

    return r;
}

u16 
ee_load_16 (u32 address) 
{
    uint16_t r = 0;

    if (BIOS.contains(address)) 
        return *(u16*)&_bios_memory_[address & 0x3FFFFF];
   
    if (RDRAM.contains(address)) 
        return *(u16*)&_rdram_[address];

    if (IOP_RAM.contains(address)) 
        return *(u16*)&_iop_ram_[address & 0x1FFFFF];

    if (address == 0x1a000006) 
        return 1;

    errlog("[ERROR]: Could not read load_memory16() at address [{:#09x}]\n", address);
    
    return r;
}

u32 
ee_load_32 (u32 address) 
{
    uint32_t r = 0;
    
    switch(address) 
    {
        case 0x10002010:
        {
            return ipu_read_32(address);
        } break;

        case 0x1000f130:
        {
            return 0;
        } break;

        case 0x1000f000:
        {
            return intc_read(address);
        } break;

        case 0x1000f010:
        {
            return intc_read(address);
        } break;

        case 0x1000f430:
        {
            //printf("Read from MCH_RICM\n");
            return 0;
        } break;
        
        case 0x1000f440:
        {
            uint8_t SOP = (MCH_RICM >> 6) & 0xF;
            uint8_t SA  = (MCH_RICM >> 16) & 0xFFF;
            if (!SOP)
            {
                switch (SA)
                {
                case 0x21:
                    if (_rdram_sdevid < 2)
                    {
                        _rdram_sdevid++;
                        return 0x1F;
                    }
                    return 0;
                case 0x23:
                    return 0x0D0D;
                case 0x24:
                    return 0x0090;
                case 0x40:
                    return MCH_RICM & 0x1F;
                }
            }
        } break;
    }

    if (address >= 0x1000F200 && address <= 0x1000F260)
        return sif_read(address);

    if (GS_REGISTERS.contains(address))
        return gs_read_32_priviledged(address);

    //if ((address & 0xFFFFF000) == 0x10003000)
    if ((address >= 0x10003000) && (address <= 0x100030A0))
        return gif_read(address);

    if (address >= 0x10008000 && address < 0x1000f590)
        return dmac_read(address);

    if (address >= 0x10000000 && address <= 0x10001840)
        return timer_read(address);
    
    if (IOP_RAM.contains(address))
       return *(u32*)&_iop_ram_[address & 0x1FFFFF];

    if (BIOS.contains(address)) 
        return *(uint32_t*)&_bios_memory_[address & 0x3FFFFF];

    if (RDRAM.contains(address)) 
        return *(uint32_t*)&_rdram_[address];

    errlog("[ERROR]: Could not read load_memory32() at address [{:#09x}]\n", address);
    
    return r;
}

u64 
ee_load_64 (u32 address) 
{
    uint64_t r = 0;
    if (RDRAM.contains(address)) 
        return *(uint64_t*)&_rdram_[address];

    if (GS_REGISTERS.contains(address))
        return gs_read_64_priviledged(address);
    
    if (address == 0x10006000 || address == 0x10006008) {
        gif_fifo_read(address);
        return 0;
    }
    
    // errlog("[ERROR]: Could not read load_memory64() at address [{:#09x}]\n", address);
    return r;
}

u128 ee_load_128() {
    u128 r = { 0 };
    return r;
}

/*******************************************
 * Store Functions
*******************************************/
void 
ee_store_8 (u32 address, u8 value) 
{
    if (RDRAM.contains(address)) {
        *(u8*)&_rdram_[address] = value;
        return;
    }

    if (address == 0x1000f180) {
        output_to_console(value);
        return;
    };

    if (IOP_RAM.contains(address)) {
       *(u8*)&_iop_ram_[address & 0x1FFFFF] = value;
        return;
    }

    errlog("[ERROR]: Could not write store_memory8() value: [{:#09x}] to address: [{:#09x}] \n", value, address);
}

void 
ee_store_16 (u32 address, u16 value) 
{
    if (RDRAM.contains(address)) {
        *(u16*)&_rdram_[address] = value;
        return;
    }

    if (address == 0x1000f180) {
        output_to_console(value);
        return;
    };
    
    if (IOP_RAM.contains(address)) {
       *(u16*)&_iop_ram_[address & 0x1FFFFF] = value;
        return;
    }

    errlog("[ERROR]: Could not write store_memory16() value: [{:#09x}] to address: [{:#09x}] \n", value, address);
}

void 
ee_store_32 (u32 address, u32 value) 
{
    switch (address)
    {
        case 0x10002000:
        {
            ipu_write_32(address, value);
            return;
        } break;

        case 0x10002010:
        {
            ipu_write_32(address, value);
            return;
        } break;

        case 0x1000f180:
        {
            output_to_console(value);
            return;
        } break;

        case 0x1000f000:
        {
            intc_write(address, value);
            return;
        } break;

        case 0x1000f010:
        {
            intc_write(address, value);
            return;
        } break;

        case 0x1000f430:
        {
            uint8_t SA = (value >> 16) & 0xFFF;
            uint8_t SBC = (value >> 6) & 0xF;

            if (SA == 0x21 && SBC == 0x1 && ((MCH_DRD >> 7) & 1) == 0)
                _rdram_sdevid = 0;

            MCH_RICM = value & ~0x80000000;
            return;
        } break;

        case 0x1000f440:
        {
           //printf("Writing to MCH_DRD [%#08x]\n", value);
            MCH_DRD = value;
            return;
        } break;

        return;
    }
    
    //@@Note: Not sure what is this is
    if (address == 0x1000f500) return;

    if (address >= 0x1000F200 && address <= 0x1000F260) {
        sif_write(address, value);
        return;
    }

    if (GS_REGISTERS.contains(address)) {
        gs_write_32_priviledged(address, value);
        return;
    }

    //if (address && 0xFFFFF000 == 0x10003000 || address == 0x10006000) {
    if ((address >= 0x10003000) && (address <= 0x100030A0)) {
        gif_write(address, value);
        return;
    }

    if (address >= 0x10008000 && address < 0x1000f000) {
       dmac_write(address, value);
       return;
    }

    if (address >= 0x10000000 && address <= 0x10001840) {
        timer_write(address, value);
        return;
    }

    if (RDRAM.contains(address)) {
        *(u32*)&_rdram_[address] = value;
        return;
    }

    if (IOP_RAM.contains(address)) {
       *(u32*)&_iop_ram_[address & 0x1FFFFF] = value;
        return;
    }

    errlog("[ERROR]: Could not write store_memory32() value: [{:#09x}] to address: [{:#09x}] \n", value, address);
}

void 
ee_store_64 (u32 address, u64 value) 
{
    if (RDRAM.contains(address)) {
        *(u64*)&_rdram_[address] = value;
        return;
    }

    if (GS_REGISTERS.contains(address)) {
        gs_write_64_priviledged(address, value);
        return;
    }

    /* @@Move @@Incomplete: This is a 128 bit move this should be moved into a different function */
    if (address == 0x10006000 || address == 0x10006008) {
        gif_fifo_write(address);
        return;
    }

    if (address ==  0x10007010 || address == 0x10007018) {
        ipu_fifo_write();
        return;
    }

    if (VU1_CODE_MEMORY.contains(address)) {
        //@HACK         
        *(u64*)&_vu1_code_memory_[address & 0x3FFF] = value;
        return;
    }

    if (VU1_DATA_MEMORY.contains(address)) {
        //@HACK 
        *(u64*)&_vu1_data_memory_[address & 0x3FFF] = value;
        return;   
    }

    if (VU0_CODE_MEMORY.contains(address)) {
        //@HACK 
        *(u64*)&_vu0_code_memory_[address & 0x3FFF] = value;
        return;
    }
    
    if (VU0_DATA_MEMORY.contains(address)) {
        //@HACK 
        *(u64*)&_vu0_data_memory_[address & 0x3FFF] = value;
        return;
    }   

    /* @@Move @@Incomplete: This is a 128 bit move this should be moved into a different function */
    if (address == 0x10004000 || address == 0x10004008) {
        printf("VIF0 FIFO Write \n");
        return;
    }

    /* @@Move @@Incomplete: This is a 128 bit move this should be moved into a different function */
    if (address == 0x10005000 || address == 0x10005008){
        printf("VIF1 FIFO Write \n");
        return;
    }

    errlog("[ERROR]: Could not write store_memory64() value: [{:#09x}] to address: [{:#09x}] \n", value, address);
}

void ee_store_128 (u32 address, u128 value) {}

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

    handle_exception_level_1(&ee, &e);
    return 1;
}

inline void 
swap_framebuffers (SDL_Context *context)
{
    // @Fix: Pitch is giving me a headache I can see the light in the future
    SDL_UnlockSurface(context->surface);

    u32 mem_size = context->backbuffer->w * context->backbuffer->h * sizeof(u32);
    memcpy(context->surface->pixels, context->backbuffer->pixels, mem_size);

    SDL_LockSurface(context->surface);
    SDL_UpdateWindowSurface(context->window);
}

int 
main (int argc, char **argv) 
{
    SDL_Context main_context;
    SDL_Event       event;
    SDL_Window      *window;
    SDL_Renderer    *renderer;
    SDL_Texture     *texture;
    SDL_Surface     *surface; 
    SDL_Backbuffer  backbuffer;
    bool running    = true;
    bool left_down  = false;

    u32 instructions_run;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    printf("Mikustation 2: A Playstation 2 Emulator and Debugger\n");
    printf("\n=========================\n...Reseting...\n=========================\n");

// @@Incomplete @@Implementation: Eventually remove hardcoded path. Ask for the path at runtime 
#if _WIN32 || _WIN64
    //const char *bios_filename = "..\\Mikustation-2\\data\\bios\\scph10000.bin";
    const char *bios_filename = "..\\Mikustation-2\\data\\bios\\scph39001.bin";
    // const char* elf_filename = "..\\Mikustation-2\\data\\ps2tut\\ps2tut_01\\demo1.elf";
    const char* elf_filename = "..\\Mikustation-2\\data\\ps2tut\\ps2tut_02a\\demo2a.elf";
    // const char* elf_filename = "..\\Mikustation-2\\data\\ps2tut\\ps2tut_02b\\demo2b.elf";
#else
    const char *bios_filename = "../data/bios/scph10000.bin";
#endif    
    
    _bios_memory_       = (u8 *)malloc(sizeof(u8) * MEGABYTES(4));
    _rdram_             = (u8 *)malloc(sizeof(u8) * MEGABYTES(32));
    _iop_ram_           = (u8 *)malloc(sizeof(u8) * MEGABYTES(2));
    _vu0_code_memory_   = (u8 *)malloc(sizeof(u8) * KILOBYTES(4));
    _vu0_data_memory_   = (u8 *)malloc(sizeof(u8) * KILOBYTES(4));
    _vu1_code_memory_   = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));
    _vu1_data_memory_   = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));
    
    if (read_bios(bios_filename, _bios_memory_) != 1) return 0;

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

    printf("\n=========================\nInitializing System\n=========================\n");

    load_elf(&ee, elf_filename);

    backbuffer = {
        .w              = 640,
        .h              = 480,
        .pixel_format   = SDL_PIXELFORMAT_ARGB8888,
        .pitch          = 640 * 4,
        .pixels         = new uint32_t[648 * 480],
    };

    window = SDL_CreateWindow("Mikustation 2\n",
                            SDL_WINDOWPOS_UNDEFINED, 
                            SDL_WINDOWPOS_UNDEFINED, 
                            backbuffer.w, 
                            backbuffer.h, 
                            0);
    
    renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_RenderSetLogicalSize(renderer, 640, 480);
    
    texture = SDL_CreateTexture(renderer,
                                backbuffer.pixel_format, 
                                SDL_TEXTUREACCESS_STREAMING, 
                                backbuffer.w, 
                                backbuffer.h);

    surface = SDL_GetWindowSurface(window);

    if (!surface) {
        printf("[ERROR]: %s\n", SDL_GetError());
        return 0;
    }

    main_context = {
        .event      = &event,
        .window     = window,
        .renderer   = renderer,
        .texture    = texture,
        .surface    = surface,
        .backbuffer = &backbuffer,
    };

    while (running) {
        instructions_run = 0;  
        // @@Note: In dobiestation and chonkystation there is a random instruction limit in order to 
        // synch the ee and iop cycle rate by 1/8 it looks like an arbitrary number.      
        while (instructions_run < 500000) {
            /* Step Through Playstation 2 Pipeline */
            r5900_cycle(&ee);
            if (instructions_run % 2 == 0) { dmac_cycle(); } 
            timer_tick();
            if (instructions_run % 8 == 0) { iop_cycle(); }
            instructions_run++;            

            // if (intc_read(0x1000f010) & 0x4) {
            
            if(instructions_run == 475000) {
                request_interrupt(INT_VB_ON);
                gs_render_crt(&main_context);
                swap_framebuffers(&main_context);
            }
            request_interrupt(INT_VB_OFF);
        
        }
#if 1
        /* Emulator Step Through*/
        SDL_PollEvent(&event);
        switch (event.type)
        {
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
        }
#endif
    }

    r5900_shutdown();
    gs_shutdown();

    free(_bios_memory_);
    free(_rdram_);
    delete[] backbuffer.pixels;
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}

