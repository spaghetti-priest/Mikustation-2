/*
*    /   /      |    |  /  |     |
*   / \ / \     |    | /   |     |
*  /   \   \    |    | \   |_____|  Mikustation-2
* --------------------------------------
* Copyright (c) 2023-2024 Xaviar Roach
* SPD X-License-Identifier: MIT
*/ 
 
#include <thread> 
#include <fstream>
#include <typeinfo>
#include "SDL2/include/SDL.h"

#include "../include/ps2.h"
#include "../include/loader.h"
#include "../include/common.h"
#include "../include/ps2types.h"

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

typedef struct SDL_Backbuffer {
    uint32_t w;
    uint32_t h;
    uint32_t pixel_format;
    uint32_t pitch;
    uint32_t *pixels;
} SDL_Backbuffer;

std::ofstream console("disasm.txt", std::ios::out);

u32 MCH_RICM    = 0, MCH_DRD    = 0;
u8 _rdram_sdevid = 0;

static u8 *_bios_memory_;
static u8 *_rdram_;

static u8 *_vu0_code_memory_;
static u8 *_vu0_data_memory_;
static u8 *_vu1_code_memory_;
static u8 *_vu1_data_memory_;

static u8* _iop_ram_;


alignas(16) R5900_Core ee = {0};
alignas(16) GIF gif;
// alignas(16) GraphicsSynthesizer gs;

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
Range SCRATCHPAD        = Range(0x70000000, KILOBYTES(16));

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
    u8 r;

    if (PSX_RAM.contains(address))  
        return *(u8*)&_iop_ram_[address];
    if (BIOS.contains(address))     
        return *(u8*)&_bios_memory_[address & 0x3FFFFF];

    printf("[ERROR]: Could not write iop_load_memory8() address: [{%#09x}] \n", address);

    return r;
}

u16 
iop_load_16 (u32 address)
{
    u16 r;

    if (PSX_RAM.contains(address))  
        return *(u16*)&_iop_ram_[address];
    if (BIOS.contains(address))     
        return *(u16*)&_bios_memory_[address & 0x3FFFFF];
 
    printf("[ERROR]: Could not write iop_load_memory16() address: [{%#09x}] \n", address);
    
    return r;
}

u32 
iop_load_32 (u32 address)
{
    u32 r;

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

    printf("[ERROR]: Could not write iop_load_memory32() address: [{%#09x}] \n", address);

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

    printf("[ERROR]: Could not write iop_store_memory8() value: [{%#09x}] to address: [{%#09x}] \n", value, address);

}

void 
iop_store_16 (u32 address, u16 value)
{
    if (PSX_RAM.contains(address)) {
      *(u16*)&_iop_ram_[address] = value;
      return;  
    } 

    printf("[ERROR]: Could not write iop_store_memory16() value: [{%#09x}] to address: [{%#09x}] \n", value, address);

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
        return;
    }

    printf("[ERROR]: Could not write iop_store_memory32() value: [{%#09x}]  to address: [{%#09x}] \n", value, address);

}

/*******************************************
 * Load Functions
*******************************************/
u8 
ee_load_8 (u32 address) 
{
    uint8_t r;

    if (BIOS.contains(address)) 
        return *(u8*)&_bios_memory_[address & 0x3FFFFF];
    
    if (RDRAM.contains(address))
        return *(u8*)&_rdram_[address];

    //errlog("[ERROR]: Could not read load_memory() at address [{:#09x}]\n", address);
    
    //printf("[ERROR]: Could not write load_memory8() address: [{%#09x}] \n", address);

    return r;
}

u16 
ee_load_16 (u32 address) 
{
    uint16_t r;

    if (BIOS.contains(address)) 
        return *(u16*)&_bios_memory_[address & 0x3FFFFF];
   
    if (RDRAM.contains(address)) 
        return *(u16*)&_rdram_[address];

    if (IOP_RAM.contains(address)) 
        return *(u16*)&_iop_ram_[address & 0x1FFFFF];
    //errlog("[ERROR]: Could not read load_memory() at address [{:#09x}]\n", address);
    
    //printf("[ERROR]: Could not write load_memory16() address: [{%#09x}] \n", address);

    return r;
}

u32 
ee_load_32 (u32 address) 
{
    uint32_t r;
    
    switch(address) 
    {
        printf("Hardware I/O read addr: {%#x}\n", address); 

        case 0x10002010:
        {
            return ipu_read_32(address);
        } break;

        case 0x1000f130:
        {
            return 0;
        } break;

        case 0x1000f430:
        {
            //printf("Read from MCH_RICM\n");
            return 0;
        } break;


        case 0x1000F210:
        {
            printf("READ: IOP->EE communication\n");
            return 0x1000C030;
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

    if (address == 0x1000f000 || address == 0x1000f010) 
        return intc_read(address);

    if (GS_REGISTERS.contains(address))
        return gs_read_32_priviledged(address);

    if (address >= 0x10003000 && address <= 0x10006000)
        return gif_read_32(&gif, address);

    if (address >= 0x10008000 && address < 0x1000f000)
        return dmac_read_32(address);

    if (address >= 0x10000000 && address <= 0x10001840)
        return timer_read(address);
    
    if (IOP_RAM.contains(address)) {
       return *(u32*)&_iop_ram_[address & 0x1FFFFF];
    }

    if (BIOS.contains(address)) 
        return *(uint32_t*)&_bios_memory_[address & 0x3FFFFF];

    if (RDRAM.contains(address)) 
        return *(uint32_t*)&_rdram_[address];

    //printf("[ERROR]: Could not write load_memory32() address: [{%#09x}] \n", address);
    
    return r;
}

u64 
ee_load_64 (u32 address) 
{
    uint64_t r;
    if (RDRAM.contains(address)) 
        return *(uint64_t*)&_rdram_[address];

    if (GS_REGISTERS.contains(address))
        return gs_read_64_priviledged(address);

     //if (IOP_RAM.contains(address)) 
     //   return *(uint64_t*)&_iop_ram_[address & 0x1FFFFF];

    //errlog("[ERROR]: Could not read load_memory() at address [{:#09x}]\n", address);
    //printf("[ERROR]: Could not write load_memory64() address: [{%#09x}] \n", address);
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

    //if (address >= 0x1C000000 && address < 0x1C200000) {
    if (IOP_RAM.contains(address)) {
       *(u8*)&_iop_ram_[address & 0x1FFFFF] = value;
        return;
    }

    //printf("[ERROR]: Could not write store_memory8() value: [{%#09x}] to address: [{%#09x}] \n", value, address);
    //errlog("[ERROR]: Could not write store_memory8() value: [{:#09x}] to address: [{:#09x}] \n", value, address);
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
    
    //if (address >= 0x1C000000 && address < 0x1C200000) {
    if (IOP_RAM.contains(address)) {
       *(u16*)&_iop_ram_[address & 0x1FFFFF] = value;
        return;
    }

    printf("[ERROR]: Could not write ee_store_memory16() value: [{%#09x}] to address: [{%#09x}] \n", value, address);
    //errlog("[ERROR]: Could not write store_memory16() value: [{:#09x}] to address: [{:#09x}] \n", value, address);
}

void 
ee_store_32 (u32 address, u32 value) 
{
    switch (address)
    {
        printf("Hardware I/O write value: [%#08x], addr: [%#08x]\n", value, address ); 
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

        case 0x1000F200:
        {
            printf("WRITE: EE->IOP communication\n");
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

    if (address == 0x1000f000 || address == 0x1000f010) {
        intc_write(address, value);
        return;
    }

    if (GS_REGISTERS.contains(address)) {
        gs_write_32_priviledged(address, value);
        return;
    }

    if (address >= 0x10003000 && address <= 0x100030A0) {
        gif_write_32(&gif, address, value);
        return;
    }

    if (address >= 0x10008000 && address < 0x1000f000) {
       dmac_write_32(address, value);
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
    
    printf("[ERROR]: Could not write ee_store_memory32() value: [{%#09x}] to address: [{%#09x}] \n", value, address);
    //errlog("[ERROR]: Could not write store_memory32() value: [{:#09x}] to address: [{:#09x}] \n", value, address);
}

void 
ee_store_64 (u32 address, u64 value) 
{
    //if (address >= 0x11000000 && address < 0x11004040) {
    //    printf("READ: VU1 read address: [%#08x], value: [%#08x]\n", address, value);
    //    return;
    //}

    if (RDRAM.contains(address)) {
        *(u64*)&_rdram_[address] = value;
        return;
    }

    if (GS_REGISTERS.contains(address)) {
        gs_write_64_priviledged(address, value);
        return;
    }

    /* @@Move @@Incomplete: This is a 128 bit move this should be moved into a different function */
    if (address ==  0x10006000 || address == 0x10006008) {
        gif_fifo_write(address, value);
        return;
    }

    if (address ==  0x10007010 || address == 0x10007018) {
        ipu_fifo_write();
        return;
    }
    /*
    if (IOP_RAM.contains(address)) {
        *(u64*)&_iop_ram_[address & 0x1FFFFF] = value;
        return;
    }
    */
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

    printf("[ERROR]: Could not write ee_store_memory64() value: [{%#09x}] to address: [{%#09x}] \n", value, address);
    //errlog("[ERROR]: Could not write store_memory64() value: [{:#09x}] to address: [{:#09x}] \n", value, address);
}

void ee_store_128 (u32 address, u128 value) {}

u32 
check_interrupt (bool value)
{
    Exception e = get_exception(V_INTERRUPT, __INTERRUPT);
    ee.cop0.cause.int0_pending = value;
    if (!value)
        return 0;

    bool interrupt_enable               = ee.cop0.status.IE && ee.cop0.status.EIE;
    bool check_exception_error_level    = ee.cop0.status.ERL && ee.cop0.status.EXL;
    interrupt_enable                    = interrupt_enable && !check_exception_error_level;

    if (!interrupt_enable)
        return 0;

    handle_exception_level_1(&ee, &e);
    return 1;
}

void set_GS_IMR (R5900_Core *ee)
{
    u32 imr = ee->reg.r[4].UD[0];
    gs_write_64_priviledged(0x12001010, imr);
}

void 
bios_hle_syscall (R5900_Core *ee, u32 syscall)
{
    switch(syscall)
    {
        case 0x02:
        {

        } break;
        
        case 0x71:
        {
            set_GS_IMR(ee);
        } break;
    }
}

typedef struct _SDL_Context_ {
    SDL_Event       event;
    SDL_Window      *window;
    SDL_Renderer    *renderer;
    SDL_Texture     *texture;
} SDL_Context;

int 
main (int argc, char **argv) 
{

    SDL_Event       event;
    SDL_Window      *window;
    SDL_Renderer    *renderer;
    SDL_Texture     *texture;
    SDL_Backbuffer  backbuffer;
    bool running    = true;
    bool left_down  = false;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    printf("Mikustation 2: A Playstation 2 Emulator and Debugger\n");
    printf("\n=========================\n...Reseting...\n=========================\n");

#if _WIN32 || _WIN64
    const char *filename = "..\\Mikustation-2\\data\\bios\\scph10000.bin";
    const char* elf_filename = "..\\Mikustation-2\\data\\3stars\\3stars.elf";
    //const char* elf_filename = "..\\Mikustation-2\\data\\ps2tut\\ps2tut_01\\demo1.elf";
#else
    const char *filename = "../data/bios/scph10000.bin";
#endif    
    
    _bios_memory_       = (u8 *)malloc(sizeof(u8) * MEGABYTES(4));
    _rdram_             = (u8 *)malloc(sizeof(u8) * MEGABYTES(32));
    _iop_ram_           = (u8 *)malloc(sizeof(u8) * MEGABYTES(2));
    _vu0_code_memory_   = (u8 *)malloc(sizeof(u8) * KILOBYTES(4));
    _vu0_data_memory_   = (u8 *)malloc(sizeof(u8) * KILOBYTES(4));
    _vu1_code_memory_   = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));
    _vu1_data_memory_   = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));
    
    if (read_bios(filename, _bios_memory_) != 1) return 0;

    //@@Incomplete: Move this into its own function
    {
        printf("Resetting Emotion Engine Core\n");
        memset(&ee, 0, sizeof(R5900_Core));
        ee = {
            .pc            = 0xbfc00000,
            .current_cycle = 0,
        };
        ee.cop0.regs[15]    = 0x2e20;
//        _scratchpad_        = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));

    }
    
    //r5900_reset(ee);
    dmac_reset();
    gs_reset();
    gif_reset(&gif);
    timer_reset();
    cop1_reset();
    ipu_reset();
    iop_reset();
    vu_reset();
    vif_reset();

    printf("\n=========================\nInitializing System\n=========================\n");

    // @@Note: Testing this file not loading it
    load_elf(&ee, elf_filename);

    backbuffer = {
        .w              = 640,
        .h              = 480,
        .pixel_format   = SDL_PIXELFORMAT_ARGB8888,
        .pitch          = 640 * sizeof(uint32_t),
        .pixels         = new uint32_t[648 * 480],
    };

    window = SDL_CreateWindow("Mikustation_2\n",
                              SDL_WINDOWPOS_UNDEFINED, 
                              SDL_WINDOWPOS_UNDEFINED, 
                              backbuffer.h, 
                              backbuffer.w, 
                              0);
    renderer = SDL_CreateRenderer(window, -1, 0);
    texture = SDL_CreateTexture(renderer,
                                backbuffer.pixel_format, 
                                SDL_TEXTUREACCESS_STATIC, 
                                backbuffer.h, 
                                backbuffer.w);

    //size_t set_size = backbuffer.w * backbuffer.pitch;
    size_t set_size = 640 * sizeof(uint32_t) * 480;
    memset(backbuffer.pixels, 255, set_size);
    u32 instructions_run;

    while (running) {
        instructions_run = 0;  
        // @@Note: In dobiestation and chonkystation there is a random instruction limit in order to 
        // synch the ee and iop cycle rate by 1/8 it looks like an arbitrary number.      
        while (instructions_run < 500000) {
            /* Step Through Playstation 2 Pipeline */
            r5900_cycle(&ee);
            dmac_cycle();
            timer_tick();
            
            if (instructions_run % 8 == 0) {
                iop_cycle();
            }
            instructions_run++;            
        }
#if 0
        /* Emulator Step Through*/
        //SDL_UpdateTexture(texture, NULL, backbuffer.pixels, backbuffer.pitch);
        SDL_WaitEvent(&event);
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
            case SDL_MOUSEMOTION:
                if (left_down) {
                    int mouseX = event.motion.x;
                    int mouseY = event.motion.y;
                    backbuffer.pixels[mouseY * 640 + mouseX] = 0;
                }
                break;
        }
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
#endif
    }

    r5900_shutdown();
    
    free(_bios_memory_);
    free(_rdram_);
    delete[] backbuffer.pixels;
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}

