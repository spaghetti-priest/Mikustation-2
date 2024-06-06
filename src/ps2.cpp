/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include <thread>
#include <fstream>
#include <typeinfo>
#include "SDL2/include/SDL.h"

#include "../include/ps2.h"
#include "../include/loader.h"
#include "../include/r5900Interpreter.h"
#include "../include/gs.h"
#include "../include/gif.h"
#include "../include/dmac.h"
#include "../include/ps2types.h"
#include "../include/intc.h"
#include "../include/cop0.h"

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

alignas(16) R5900_Core ee = {0};
alignas(16) GIF gif;
// alignas(16) GraphicsSynthesizer gs;

/*******************************************
 * Load Functions
*******************************************/
u8 
ee_load_8 (u32 address) 
{
    uint8_t r;

    if (address >= 0x1FC00000 && address < 0x20000000) 
        return *(u8*)&_bios_memory_[address & 0x3FFFFF];
    if (address >= 0x00000000 && address < 0x02000000) 
        return *(u8*)&_rdram_[address];

    //errlog("[ERROR]: Could not read load_memory() at address [{:#09x}]\n", address);
    
    return r;
}

u16 
ee_load_16 (u32 address) 
{
    uint16_t r;

    if (address >= 0x1FC00000 && address < 0x20000000) 
        return *(u16*)&_bios_memory_[address & 0x3FFFFF];
    if (address >= 0x00000000 && address < 0x02000000) 
        return *(u16*)&_rdram_[address];

    //errlog("[ERROR]: Could not read load_memory() at address [{:#09x}]\n", address);
    
    return r;
}

u32 
ee_load_32 (u32 address) 
{
    uint32_t r;
    
    switch(address) 
    {
        printf("Hardware I/O read addr: {%#x}\n", address); 
        case 0x1000f130:
        {
            return 0;
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

    if (address == 0x1000f000 || address == 0x1000f010) 
        return intc_read(address);

    if (address & 0xFF000000 == 0x12000000)
        return gs_read_32_priviledged(address);

    if (address >= 0x10003000 && address <= 0x100030A0)
        return gif_read_32(&gif, address);

    if (address >= 0x10000000 && address < 0x1000f000)
        return dmac_read_32(address);



    if (address >= 0x1FC00000 && address < 0x20000000) return *(uint32_t*)&_bios_memory_[address & 0x3FFFFF];
    if (address >= 0x00000000 && address < 0x02000000) return *(uint32_t*)&_rdram_[address];

    
    return r;
}

u64 
ee_load_64 (u32 address) 
{
    uint64_t r;
    if (address >= 0x00000000 && address < 0x02000000) 
        return *(uint64_t*)&_rdram_[address];


    if (address >= 0x12000000 && address < 0x12002000) 
        return gs_read_64_priviledged(address);

    //errlog("[ERROR]: Could not read load_memory() at address [{:#09x}]\n", address);
    return r;
}

void ee_load_128() {return;}

/*******************************************
 * Store Functions
*******************************************/
void 
ee_store_8 (u32 address, u8 value) 
{
    if (address <= 0x02000000) {
        *(u8*)&_rdram_[address] = value;
        return;
    }
    if (address == 0x1000f180) {
        console << (char)value;
        console.flush();
        return;
    };
  
    //errlog("[ERROR]: Could not write store_memory() value: [{:#09x}] to address: [{:#09x}] \n", value, address);
}

void 
ee_store_16 (u32 address, u16 value) 
{
    if (address <= 0x02000000) {
        *(u16*)&_rdram_[address] = value;
        return;
    }
    if (address == 0x1000f180) {
        console << (char)value;
        console.flush();
        return;
    };
    
    //errlog("[ERROR]: Could not write store_memory() value: [{:#09x}] to address: [{:#09x}] \n", value, address);
}

void 
ee_store_32 (u32 address, u32 value) 
{
    switch (address)
    {
        printf("Hardware I/O write value: [%#08x], addr: [%#08x]\n", value, address ); 

        case 0x1000f180:
        {
            console << (char)value;
            console.flush();
        } break;

        case 0x1000f430:
        {
            uint8_t SA = (value >> 16) & 0xFFF;
            uint8_t SBC = (value >> 6) & 0xF;

            if (SA == 0x21 && SBC == 0x1 && ((MCH_DRD >> 7) & 1) == 0)
                _rdram_sdevid = 0;

            MCH_RICM = value & ~0x80000000;
        } break;

        case 0x1000f440:
        {
           //printf("Writing to MCH_DRD [%#08x]\n", value);
            MCH_DRD = value;
        } break;

        return;
    }

    if (address == 0x1000f000 || address == 0x1000f010) {
        intc_write(address, value);
        return;
    }

    if (address >= 0x12000000 && address < 0x12002000) {
        gs_write_32_priviledged(address, value);
        return;
    }

    if (address >= 0x10003000 && address <= 0x100030A0) {
        gif_write_32(&gif, address, value);
        return;
    }

    if (address >= 0x10000000 && address < 0x1000f000) {
       dmac_write_32(address, value);
       return;
    }

    if (address <= 0x02000000) {
        *(u32*)&_rdram_[address] = value;
        return;
    }
//    printf("[ERROR]: Could not write store_memory() value: [{%#09x}] to address: [{%#09x}] \n", value, address);
    //errlog("[ERROR]: Could not write store_memory() value: [{:#09x}] to address: [{:#09x}] \n", value, address);
}

void 
ee_store_64 (u32 address, u64 value) 
{
    if (address >= 0x11000000 && address < 0x11004040) {
        printf("READ: VU1 read to.....\n");
        return;
    }

    if (address <= 0x02000000) {
        *(u64*)&_rdram_[address] = value;
        return;
    }

    if (address >= 0x12000000 && address < 0x12002000) {
        gs_write_64_priviledged(address, value);
        return;
    }
 
    //errlog("[ERROR]: Could not write store_memory() value: [{:#09x}] to address: [{:#09x}] \n", value, address);
}

u32 
check_interrupt (bool value)
{
    Exception e = get_exception(V_INTERRUPT, __INTERRUPT);
    ee.cop0.cause.int0_pending = value;
    if (!value)
        return 0;

    bool interrupt_enable = (ee.cop0.status.IE && ee.cop0.status.EIE);
    bool check_exception_error_level = ee.cop0.status.ERL && ee.cop0.status.EXL;

    interrupt_enable = interrupt_enable && !check_exception_error_level;

    if (!interrupt_enable)
        return 0;

    handle_exception_level_1(&ee, &e);
    return 1;
}

//@@Remove: This should belong to loader file.... alongside with an elf loader.
// I dont want to deal with extern .obj conflicts right now. 
int 
read_bios (const char *filename) 
{
    size_t file_size = 0;
    unsigned char *c;

#if _WIN32 || _WIN64
    FILE* f = fopen(filename, "rb");
    if (!f) {
        printf("[ERROR]: Could not open BIOS file. \n");
        fclose(f);
        return 0;
    }
    printf("Found BIOS file...\n");

    fseek(f, 0, SEEK_END); // @Incomplete: Add 64 bit version compatibility
    file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if(fread(&c, sizeof(unsigned char), file_size, f) == 1) {
        printf("[ERROR]: Could not read BIOS file \n");
        fclose(f);
        return 0;
    }

    printf("Copying BIOS to main memory\n");
    // Copy BIOS to memory
    uint64_t addr = 0;
    while (fread(&c, sizeof(unsigned char), sizeof(unsigned char), f) == 1) 
        _bios_memory_[addr++] = reinterpret_cast<uint8_t>(c);

    fclose(f);
#else 
    std::ifstream reader;
    reader.open(filename, std::ios::in | std::ios::binary);
    if (!reader.is_open())
        printf("[ERROR]: Could not read BIOS file!\n");

    reader.seekg(0);
    reader.read((char*)_bios_memory_, 4 * 1024 * 1024);
    reader.close();
#endif

    printf("Sucessfully read BIOS file...\n");

    return 1;
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
#else
    const char *filename = "../data/bios/scph10000.bin";
#endif    

    const char* elf_filename = "..\\Mikustation-2\\data\\3stars\\3stars.elf";
    _bios_memory_   = (u8 *)malloc(sizeof(u8) * MEGABYTES(4));
    _rdram_         = (u8 *)malloc(sizeof(u8) * MEGABYTES(32));

    if (read_bios(filename) != 1) return 0;

    
    {
        printf("Resetting Emotion Engine Core\n");
        memset(&ee, 0, sizeof(R5900_Core));
        ee = {
            .pc            = 0xbfc00000,
            .cop1.fcr0     = 0x2e30,
            .current_cycle = 0,
        };
        ee.cop0.regs[15]    = 0x2e20;
//        _scratchpad_        = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));
        //_icache_            = (u8 *)malloc(sizeof(u8) * KILOBYTES(16));
        //_dcache_            = (u8 *)malloc(sizeof(u8) * KILOBYTES(8));
    }
    
    //r5900_reset(ee);
    dmac_reset();
    gs_reset();
    gif_reset(&gif);
    printf("\n=========================\nInintializing System\n=========================\n");

    // @@Note: Testing this file not loading it
    //load_elf(&ee, elf_filename);

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

    while (running) {

        /* Step Through Playstation 2 Pipeline */
        r5900_cycle(&ee);
        dmac_cycle();

        /* Emulator Step Through*/
        SDL_UpdateTexture(texture, NULL, backbuffer.pixels, backbuffer.pitch);
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

