/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include "../include/ee/dmac.h"
#include "../include/ee/gif.h"
#include "../include/ps2.h"
#include <iostream>

DMAC dmac = {0};

void 
dmac_reset ()
{
    printf("Resetting DMAC Controller\n");

    dmac.control.enable = true;
    for (int i = 0; i < DMAC_CHANNEL_COUNT; ++i) {
        dmac.interrupt_status.channel_status[i]     = 0;
        dmac.interrupt_status.channel_mask[i]       = 0;
        dmac.channels[i].control.value              = 0;
        dmac.channels[i].address                    = 0;
        dmac.channels[i].tag_address.value          = 0;
        dmac.channels[i].quadword_count.value       = 0;
        dmac.channels[i].save_tag0                  = 0;
        dmac.channels[i].save_tag1                  = 0;
        dmac.channels[i].scratchpad_address.value   = 0;
    }
}

static inline u32
read_dma_channel_control (DMA_Channel *channel) 
{
    u32 r;

    r |= channel->control.dir            << 0;
    r |= channel->control.mode           << 2;
    r |= channel->control.stack_pointer  << 4;
    r |= channel->control.tag_transfer   << 6;
    r |= channel->control.tag_interrupt  << 7;
    r |= channel->control.start          << 8;
    r |= channel->control.TAG            << 16;
    
    printf("Dn_CHCR [%#08x] \n", r);
    return r;
}

static inline void 
set_dma_channel_control (DMA_Channel *channel, u32 value)
{
    channel->control.dir            = (value >> 0)  & 0x1 ? true : false;
    channel->control.mode           = (value >> 2)  & 0x3;
    channel->control.stack_pointer  = (value >> 4)  & 0x3;
    channel->control.tag_transfer   = (value >> 6)  & 0x1 ? true : false;
    channel->control.tag_interrupt  = (value >> 7)  & 0x1 ? true : false;
    channel->control.start          = (value >> 8)  & 0x1 ? true : false;
    channel->control.TAG            = (value >> 16) & 0xFFFF;

    channel->control.value = value;
}

static inline u32
read_dma_interrupt_status (union D_STAT *status) 
{
    u32 r = 0;

    for (int i = 0; i < DMAC_CHANNEL_COUNT; ++i) {
        r |= status->channel_status[i] << i;
        r |= status->channel_mask[i]   << (i + 16);
    }
    r |= status->stall_status       << 13;
    r |= status->mem_empty_status   << 14;
    r |= status->BUSERR_status      << 15;
    r |= status->stall_mask         << 29;
    r |= status->mem_empty_mask     << 30;

    printf("D_STAT [%#08x] \n", r);
    return r;
}

// @@Bug: This is wrong
static inline void
set_dma_interrupt_status (union D_STAT *status, u32 value)
{
    for (int i = 0; i < DMAC_CHANNEL_COUNT; ++i) {
        status->channel_status[i] = ((value >> i) & 0x1)         ? true : false; 
        status->channel_mask[i]   = ((value >> (i + 16)) & 0x1)  ? true : false; 
    }
    status->stall_status     = (value >> 13) & 0x1;
    status->mem_empty_status = (value >> 14) & 0x1;
    status->BUSERR_status    = (value >> 15) & 0x1;
    status->stall_mask       = (value >> 29) & 0x1;
    status->mem_empty_mask   = (value >> 30) & 0x1;

    status->value           = value;
}
/*
static inlnie void
process_normal_transfer()
{
}

static inlnie void
process_source_chain()
{
}
*/

void
dmac_cycle ()
{
    u128 data = { 0 };
    bool stop_dma_transfer = ~0x100;

    if (!dmac.control.enable) return;

    if (dmac.channels[2].control.start) {
        if (!dmac.channels[2].quadword_count.value) {
            if (dmac.channels[2].tag_end) 
                dmac.channels[2].control.start &= stop_dma_transfer;
            for (int i = 0; i < dmac.channels[2].quadword_count.value; i++) {
                data.lo = ee_load_64(dmac.channels[2].address * i);
                data.hi = ee_load_64(dmac.channels[2].address + 8 * i);
            }
            dmac.channels[2].address += 16;
            dmac.channels[2].quadword_count.value -= 1;

            //u8 mode = (dmac.channels[2].control.mode >> 2) & 0x3;
            u8 mode = dmac.channels[2].control.mode;
            switch(mode)
            {
                /* Normal */
                case 0x0: {
                    printf("Normal DMAC Transfer\n");
                } break; 
                /* Chain */
                case 0x1: {
                    printf("Chain DMAC Transfer\n");
                } break; 
                /* Interleave */
                case 0x2: {
                    printf("Interleave DMAC Transfer\n");
                } break; 
            }
            
            /* GIF Cycle */
            gif_send_path3(data);

            // @Incomplete: Transfers channels when they eventually exist:
            //vu1_send_path1()
            //vif_send_path2()
        }
    }
}

static inline u32
read_dmac_channel_value(u32 address, u32 index)
{
    u32 ad = address & 0x000000FF;
    switch (ad) {
        case 0x00:
        {
            return dmac.channels[index].control.value;
        } break;
        case 0x10:
        {
            return dmac.channels[index].address;
        } break;
        case 0x20:
        {
            return dmac.channels[index].quadword_count.value;
        } break;
        case 0x30:
        {
            return dmac.channels[index].tag_address.value;
        } break;
        case 0x40:
        {
            return dmac.channels[index].save_tag0;
        } break;
        case 0x50:
        {
            return dmac.channels[index].save_tag1;
        } break;
        case 0x80:
        {
            return dmac.channels[index].scratchpad_address.value;
        } break;
    }

    return 0;
}

void 
dmac_write_32 (u32 address, u32 value)
{
    address &= 0xFFFFFF00;
    //printf("DMAC write value: [%#08x], addr: [%#x]\n", value, address);
    switch (address) {
    /******************
    * DMA Registers
    *******************/
        case D_CTRL:
        {
            dmac.control.enable                 = (value >> 0) & 0x1;  
            dmac.control.release_signal         = (value >> 1) & 0x1;    
            dmac.control.memory_drain_channel   = (value >> 2) & 0x3;  
            dmac.control.stall_source_channel   = (value >> 4) & 0x3;  
            dmac.control.stall_drain_channel    = (value >> 6) & 0x3;  
            dmac.control.release_cycle          = (value >> 8) & 0x7;
            dmac.control.value                  = value;
            printf("WRITE: DMAC control value: [%#08x]\n", value);
        } break;

        case D_STAT:
        {
            printf("WRITE: DMAC interrupt status\n");
            set_dma_interrupt_status (&dmac.interrupt_status, value);
        } break;

        case D_PCR:
        {
            for (int i = 0; i <= 9; ++i) {
                dmac.priorty.cop_control[i] = (value >> i) & 0x1;
                dmac.priorty.channel_dma_enable[i] = (value >> (i+16)) & 0x1;
            }
            dmac.priorty.control_enable = (value >> 31) & 0x1;
            dmac.priorty.value          = value;
            printf("WRITE: DMAC priority control value: [%#08x]\n", value);
        } break;

        case D_SQWC:
        {
            dmac.skip_quadword.skip_quadword_counter        = (value >> 0) & 0xFF;
            dmac.skip_quadword.transfer_quadword_counter    = (value >> 16) & 0xFF;
            dmac.skip_quadword.value                        = value;
            printf("WRITE: DMAC skip quadword value: [%#08x]\n", value);
        } break;

        case D_RBOR:
        {
            dmac.ringbuffer_offset.buffer_address   = value & 0x7FFFFFFF;
            dmac.ringbuffer_offset.value            = value;
            printf("WRITE: DMAC ringbuffer offset\n");
        } break;

        case D_RBSR:
        {
            dmac.ringbuffer_size.buffer_size    = (value >> 4) & 0x7FFFFFF;
            dmac.ringbuffer_size.value          = value;
            printf("WRITE: DMAC ringbuffer size\n");
        } break;

        case D_STADR:
        {
            dmac.stall_address.address  = value & 0x7FFFFFFF;
            dmac.stall_address.value    = value;
            printf("WRITE: DMAC stall address\n");
        } break;

    /******************
    * DMA Channels
    *******************/
        case DMA_VIF0:
        {
            printf("WRITE: DMAC write to VIF0 [%#08x]\n", value);
            set_dma_channel_control(&dmac.channels[0], value);
        } break;

        case DMA_VIF1:
        {
            printf("WRITE: DMAC write to VIF1(PATH2 to GIF) [%#08x]\n", value);
            set_dma_channel_control(&dmac.channels[1], value);
        } break;
        
        case DMA_GIF:
        {
            //printf("WRITE: DMAC write to GIF(PATH3)\n");
            u32 ad = address & 0x000000FF;
            if (ad == 0x00) {
                set_dma_channel_control(&dmac.channels[2], value);
                printf("WRITE: GIF_CHCR, value: [%#08x]\n", value);
                return;                
            } 
            if (ad == 0x10) {
                dmac.channels[2].address = value;
                printf("WRITE: GIF_MADR, value: [%#08x]\n", value);
                return;
            }
            if (ad == 0x20) {
                dmac.channels[2].quadword_count.quadwords = value & 0xFFFF;
                printf("WRITE: GIF_QWC, value: [%#08x]\n", value);
                return;
            }
            if (ad == 0x30) {
                dmac.channels[2].tag_address.address            = value & 0x3FFFFFFF;
                dmac.channels[2].tag_address.memory_selection   = (value >> 31) & 0x1;
                printf("WRITE: GIF_TADR, value: [%#08x]\n", value);
                return;
            }
        } break;
       
        case DMA_IPU_FROM:
        {
            printf("WRITE: DMAC write to IPU_FROM\n");
            set_dma_channel_control(&dmac.channels[3], value);
        } break;
       
       case DMA_IPU_TO:
        {
            printf("WRITE: DMAC write to IPU_TO \n");
            set_dma_channel_control(&dmac.channels[4], value);
        } break;
        
        case DMA_SIF0:
        {
            printf("WRITE: DMAC write to SIF0(from IOP)\n");
            set_dma_channel_control(&dmac.channels[5], value);
        } break;
       
        case DMA_SIF1:
        {
            printf("WRITE: DMAC write to SIF1(to IOP)\n");
            set_dma_channel_control(&dmac.channels[6], value);
        } break;
        
        case DMA_SIF2:
        {
            printf("WRITE: DMAC write to SIF2(bidirectional)\n");
            set_dma_channel_control(&dmac.channels[7], value);
        } break;
        
        case DMA_SPR_FROM:
        {
            printf("WRITE: DMAC write to SPR_FROM\n");
            set_dma_channel_control(&dmac.channels[8], value);
        } break;
       
        case DMA_SPR_TO:
        {
            printf("WRITE: DMAC write to SPR_TO\n");
            set_dma_channel_control(&dmac.channels[9], value);
        } break;

        default:
        {
            printf("Unhandled Write32 to DMAC addr: [%#08x] value: [%#08x]\n", address, value);
        } break;
    }
    return;
}

u32
dmac_read_32 (u32 address)
{
    address &= 0xFFFFFF00;
    //printf("DMAC READ addr: [%#08x]\n", address);
    switch(address)
    {
    /******************
    * DMA Registers
    *******************/
        case D_CTRL:
        {
            printf("READ: DMAC control value: [%#08x]\n", dmac.control.value);
            return dmac.control.value;
            //read_dma_channel_control(dmac.channels);
        } break;

        case D_STAT:
        {
            printf("READ: DMAC interrupt status value: [%#08x]", dmac.interrupt_status.value);
            return dmac.interrupt_status.value;
            //read_dma_interrupt_status(&dmac.interrupt_status);
        } break;

        case DMA_VIF0:
        {
            printf("READ: DMAC read to VIF0\n");
            return read_dmac_channel_value(address, 0);
        } break;

        case DMA_VIF1:
        {
            printf("READ: DMAC READ to VIF1(PATH2 to GIF)\n");
            return read_dmac_channel_value(address, 1);         
        } break;

        case DMA_GIF:
        {
            printf("READ: DMAC READ to GIF(PATH3) [%#08x]\n", read_dmac_channel_value(address, 2));
            return read_dmac_channel_value(address, 2);
        } break;

        case DMA_IPU_FROM:
        {
            printf("READ: DMAC READ to IPU_FROM\n");
            return read_dmac_channel_value(address, 3);
        } break;

        case DMA_IPU_TO:
        {
            printf("READ: DMAC READ to IPU_TO\n");
            return read_dmac_channel_value(address, 4);
        } break;

        case DMA_SIF0:
        {
            printf("READ: DMAC READ to SIF0(from IOP)\n");
            return read_dmac_channel_value(address, 5);
        } break;

        case DMA_SIF1:
        {
            printf("READ: DMAC READ to SIF1(to IOP)\n");
            return read_dmac_channel_value(address, 6);
        } break;

        case DMA_SIF2:
        {
            printf("READ: DMAC READ to SIF2(bidirectional)\n");
            return read_dmac_channel_value(address, 7);
        } break;

        case DMA_SPR_FROM:
        {
            printf("READ: DMAC READ to SPR_FROM\n");
            return read_dmac_channel_value(address, 8);
        } break;

        case DMA_SPR_TO:
        {
            printf("READ: DMAC READ to SPR_TO\n");
            return read_dmac_channel_value(address, 9);
        } break;
        
        default:
        {
            printf("Unhandled Read32 to DMAC [%#08x]\n", address);
        } break;
    }

    return 0;
}