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
        //printf("SIO registers address [%#x]\n", address);
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
    
    printf("[ERROR]: Could not write load_memory8() address: [{%#09x}] \n", address);

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
    
    printf("[ERROR]: Could not write load_memory16() address: [{%#09x}] \n", address);

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

    if (GS_REGISTERS.contains(address))
        return gs_read_32_priviledged(address);

    if (address && 0xFFFFF000 == 0x10003000 || address == 0x10006000)
        return gif_read_32(address);

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

    printf("[ERROR]: Could not write load_memory32() address: [{%#09x}] \n", address);
    
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
    printf("[ERROR]: Could not write load_memory64() address: [{%#09x}] \n", address);
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

    printf("[ERROR]: Could not write store_memory8() value: [{%#09x}] to address: [{%#09x}] \n", value, address);
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

    if (GS_REGISTERS.contains(address)) {
        gs_write_32_priviledged(address, value);
        return;
    }

    if (address && 0xFFFFF000 == 0x10003000 || address == 0x10006000) {
        gif_write_32(address, value);
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

//From: Ps2SDK
//Used as argument for CreateThread, ReferThreadStatus
typedef struct t_ee_thread_param
{
    int status;           // 0x00
    void *func;           // 0x04
    void *stack;          // 0x08
    int stack_size;       // 0x0C
    void *gp_reg;         // 0x10
    int initial_priority; // 0x14
    int current_priority; // 0x18
    u32 attr;             // 0x1C
    u32 option;           // 0x20 Do not use - officially documented to not work.
} thread_param;

typedef struct t_ee_sema_param
{
    int count,
        max_count,
        init_count,
        wait_threads;
    u32 attr,
        option;
} semaphore_param;

struct TCB //Internal thread structure
{
    struct TCB *prev;
    struct TCB *next;
    int status;
    void *func;
    void *current_stack;
    void *gp_reg;
    short current_priority;
    short init_priority;
    int wait_type; //0=not waiting, 1=sleeping, 2=waiting on semaphore
    int sema_id;
    int wakeup_count;
    int attr;
    int option;
    void *_func; //???
    int argc;
    char **argv;
    void *initial_stack;
    int stack_size;
    int *root; //function to return to when exiting thread?
    void *heap_base;
};

struct thread_context //Stack context layout
{
    u32 sa_reg;  // Shift amount register
    u32 fcr_reg;  // FCR[fs] (fp control register)
    u32 unkn;
    u32 unused;
    u128 at, v0, v1, a0, a1, a2, a3;
    u128 t0, t1, t2, t3, t4, t5, t6, t7;
    u128 s0, s1, s2, s3, s4, s5, s6, s7, t8, t9;
    u64 hi0, hi1, lo0, lo1;
    u128 gp, sp, fp, ra;
    u32 fp_regs[32];
};

struct semaphore //Internal semaphore structure
{
    struct sema *free; //pointer to empty slot for a new semaphore
    int count;
    int max_count;
    int attr;
    int option;
    int wait_threads;
    struct TCB *wait_next, *wait_prev;
};

//@@Incomplete: My own incomplete interpretation of a BIOS thread
struct Thread
{
    int status;
    void *stack;
    s32 stack_size;
    s32 init_priority;
    s32 current_priority;
    u32 thread_id;
    void *heap_base;
};

/** Thread status */
#define THREAD_RUN         0x01
#define THREAD_READY       0x02
#define THREAD_WAIT        0x04
#define THREAD_SUSPEND     0x08
#define THREAD_WAITSUSPEND 0x0c
#define THREAD_DORMANT     0x10

/** Thread WAIT Status */
#define THREAD_NONE  0 // Thread is not in WAIT state
#define THREAD_SLEEP 1
#define THREAD_SEMA  2

#define MAX_THREADS 256
#define MAX_SEMAPHORES 256
#define MAX_PRIORITY_LEVELS 128

void
SetGsCrt (bool interlaced, int display_mode, bool ffmd)
{
    gs_set_crt(interlaced, display_mode, ffmd);
    printf("SYSCALL: SetGsCrt \n");
}

std::vector<Thread> threads(MAX_THREADS);
u32 current_thread_id;

//AKA: RFU060 or SetupThread
void
InitMainThread (u32 gp, void *stack, s32 stack_size, char *args, s32 root)
{
    u32 stack_base = (u32)stack;
    u32 rdram_size = RDRAM.start + RDRAM.size;
    u32 stack_addr = 0;   

    if (stack_base == -1) {
        stack_addr = rdram_size - stack_size; 
    } else {
        stack_addr = stack_base + stack_size;
    }

    //@Note: ??? Find out what this is 
    stack_addr -= 0x2A0;

    current_thread_id = 0;

    Thread main_thread = {
        .status             = THREAD_RUN,
        .stack              = (void*)stack_addr,
        .stack_size         = stack_size,
        .init_priority      = 0,
        .current_priority   = 0,
        .thread_id          = current_thread_id,
    };
    
    threads.push_back(main_thread);

    //Return
    ee.reg.r[2].UD[0] = stack_addr;
    printf("InitMainThread \n");
}

//AKA: RFU061
void
InitHeap (void *heap, s32 heap_size)
{
    auto thread = &threads[0];
    u32 heap_start = (u32)heap;

    if (heap_start == -1) {
        thread->heap_base = thread->stack;
    } else {
        thread->heap_base = (void*)(heap_start + heap_size);
    }

    // Return
    ee.reg.r[2].UD[0] = (u64)thread->heap_base;
    printf("InitHeap\n");
}

void
FlushCache() 
{
    printf("FlushCache\n");
}

void 
GsPutIMR (u64 imr)
{
    gs_write_64_priviledged(0x12001010, imr);
}

void
dump_all_ee_registers(R5900_Core *ee)
{
    for (int i = 0; i < 32; ++i) {
        printf("EE Register [%d] contains [%08x]\n", i, ee->reg.r[i].UD[0]);
    }
}

/* 
From:   https://www.psdevwiki.com/ps2/Syscalls 
        https://psi-rockin.github.io/ps2tek/#bioseesyscalls
*/
void 
bios_hle_syscall (R5900_Core *ee, u32 syscall)
{
    void *return_   = (void *)ee->reg.r[2].UD[0];
    void *param0    = (void *)ee->reg.r[4].UD[0];
    void *param1    = (void *)ee->reg.r[5].UD[0];
    void *param2    = (void *)ee->reg.r[6].UD[0];
    void *param3    = (void *)ee->reg.r[7].UD[0];
    void *param4    = (void *)ee->reg.r[8].UD[0];

    switch(syscall)
    {
        case 0x02: SetGsCrt((bool)param0, (s32)param1, (bool)param2); break;
        case 0x3C: InitMainThread((u32)param0, param1, (s32)param2, (char*)param3, (s32)param4); break;
        case 0x3D: InitHeap(param0, (s32)param1);
        case 0x64: FlushCache(); break;
        case 0x71: GsPutIMR((u64)param0); break;
        default:
        {
            printf("Unknown Syscall: [%#x]\n", syscall);
            return;
        }
    }
}

typedef struct _SDL_Context_ {
    SDL_Event       event;
    SDL_Window      *window;
    SDL_Renderer    *renderer;
    SDL_Texture     *texture;
    SDL_Backbuffer  backbuffer;

    bool running;
    bool left_down;
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

    u32 instructions_run;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    printf("Mikustation 2: A Playstation 2 Emulator and Debugger\n");
    printf("\n=========================\n...Reseting...\n=========================\n");

#if _WIN32 || _WIN64
    const char *filename = "..\\Mikustation-2\\data\\bios\\scph10000.bin";
    //const char* elf_filename = "..\\Mikustation-2\\data\\3stars\\3stars.elf";
    const char* elf_filename = "..\\Mikustation-2\\data\\ps2tut\\ps2tut_01\\demo1.elf";
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
    gif_reset();
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
        instructions_run = 0;  
        // @@Note: In dobiestation and chonkystation there is a random instruction limit in order to 
        // synch the ee and iop cycle rate by 1/8 it looks like an arbitrary number.      
        while (instructions_run < 500000) {
            /* Step Through Playstation 2 Pipeline */
            r5900_cycle(&ee);

            if (instructions_run % 2 == 0){
                dmac_cycle();  
            } 

            timer_tick();
            
            if (instructions_run % 8 == 0) {
                iop_cycle();
            }
            instructions_run++;            

            if(INTC_MASK & 0x4) 
                request_interrupt(INT_VB_ON);

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
        //SDL_RenderClear(renderer);
        //SDL_RenderCopy(renderer, texture, NULL, NULL);
        //SDL_RenderPresent(renderer);
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

