#pragma once

#ifndef DMAC_H
#define DMAC_H

#include "../ps2types.h"

#define DMAC_CHANNEL_COUNT 10
enum Channels {
    DMA_VIF0        = 0x10008000, DMA_VIF1        = 0x10009000,
    DMA_GIF         = 0x1000A000, DMA_IPU_FROM    = 0x1000B000,
    DMA_IPU_TO      = 0x1000B400, DMA_SIF0        = 0x1000C000,
    DMA_SIF1        = 0x1000C400, DMA_SIF2        = 0x1000C800,
    DMA_SPR_FROM    = 0x1000D000, DMA_SPR_TO      = 0x1000D400
};

enum DMARegisters {
    D_CTRL      = 0x1000E000,  D_STAT      = 0x1000E010,  D_PCR       = 0x1000E020, 
    D_SQWC      = 0x1000E030,  D_RBSR      = 0x1000E040,  D_RBOR      = 0x1000E050,
    D_STADR     = 0x1000E060,  D_ENABLER   = 0x1000F520,  D_ENABLEW   = 0x1000F590
};

// Channel Control
union Dn_CHCR {
    struct {
        bool    dir;
        bool unused;
        u8      mode : 2;
        u16     stack_pointer : 2;
        bool    tag_transfer;
        bool    tag_interrupt;
        bool    start;
        bool    unused1[7];
        u16     TAG;        
    };
    u32 value;
};

// Channel tag address
union Dn_TADR {
    struct {
        u32 address : 30;
        bool memory_selection;
    };
    u32 value;
};

// Quadword count
union Dn_QWC {
    struct {
        u16 quadwords;
        u16 unused;
    };
    u32 value;
};

// Channel scratchpad address
union Dn_SADR {
    struct {
        u16 addess;
        u16 unused;
    };
    u32 value;
};

union D_CTRL {
    struct {
        bool    enable;  
        bool    cycle_stealing;  
        u8      memory_drain_channel : 2;
        u8      stall_source_channel : 2;
        u8      stall_drain_channel  : 2; 
        u8      release_cycle        : 3; 
        u32     unused               : 21;
    };
    u32 value;
};

union D_STAT {
    struct {
        bool channel_status[10];
        bool stall_status;
        bool mem_empty_status;
        bool BUSERR_status;
        bool channel_mask[10];
        bool stall_mask;
        bool mem_empty_mask;
    };
    u32 value;
};

union D_PCR {
    struct {
        bool cop_control[10];
        u16 unused : 6;
        bool channel_dma_enable[10];
        u16 unused2 : 5;
        bool control_enable;
    };
    u32 value;
};

union D_SQWC {
    struct {
        u8 skip_quadword_counter;
        u8 unused;
        u8 transfer_quadword_counter;
        u8 unused2;
    };
    u32 value;
};

union D_RBOR {
    struct {
        u32 buffer_address : 31;
        bool unused : 1;
    };
    u32 value;
};

union D_RBSR {
    struct {
        u8 unused : 4;
        u32 buffer_size : 27;
        bool unused2 : 1;
    };
    u32 value;
};

union D_STADR {
    struct {
        u32 address : 31;
        bool unused;
    };
    u32 value;
};

union D_ENABLEW {
    struct {
        u16 unused : 15;
        bool hold_dma_transfer;
        u16 unused2 : 15;      
    };
    u32 value;
};

union D_ENABLER {
    struct {
        u16 unused : 15;
        bool hold_dma_state;
        u16 unused2 : 15;      
    };
    u32 value;
};

union DMAtag {
    struct {
        u16 quadword_count;
        u16 unused0 : 10;
        u8 priority_control : 2;
        u8 tag_id : 3;
        bool interrupt_request;
        u32 address; // Lower 4 bits are 0
        bool memory_selection; // memory or spr;
        u64 data_to_transfer;
    };
    u128 value;
};

typedef struct _DMAChannelRegisters_ {
    Dn_CHCR control;
    u32     address; // MADR
    Dn_TADR tag_address;
    Dn_QWC  quadword_count;
    u32     save_tag0;
    u32     save_tag1;
    Dn_SADR scratchpad_address;
    bool tag_end;
} DMA_Channel;

typedef struct _DMAController_ {
    union D_CTRL      control;
    union D_STAT      interrupt_status;
    union D_PCR       priorty;
    union D_SQWC      skip_quadword;
    union D_RBSR      ringbuffer_size;  
    union D_RBOR      ringbuffer_offset;  
    union D_STADR     stall_address;   
    union D_ENABLEW   hold_control;   
    union D_ENABLER   hold_state;   
    DMA_Channel       channels[10];
} DMAC;

void dmac_reset();
void dmac_write(u32 address, u32 value);
u32 dmac_read(u32 address);
void dmac_cycle();

#endif
