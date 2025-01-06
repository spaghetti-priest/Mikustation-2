/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include "../include/ee/dmac.h"
#include "../include/ee/gif.h"
#include "../include/ps2.h"
#include "../include/common.h"
#include <iostream>

DMAC dmac = {0};
const bool stop_dma_transfer = ~0x100;

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
        dmac.channels[i].quadword_count.quadwords   = 0;
        dmac.channels[i].save_tag0                  = 0;
        dmac.channels[i].save_tag1                  = 0;
        dmac.channels[i].scratchpad_address.value   = 0;
    }
}

// @Incomplete: This only works for GIF channel aswell as normal and interleave transfer mode
static void
end_of_transfer()
{
    if (dmac.channels[2].quadword_count.quadwords == 0) {
        dmac.channels[2].control.start = false;
        dmac.channels[2].control.value &= stop_dma_transfer;

        dmac.interrupt_status.channel_status[2] = true;
        bool int1 = (dmac.interrupt_status.channel_status[2] & dmac.interrupt_status.channel_mask[2]) != 0;
        check_interrupt(int1, false, true);
        syslog("End of transfer\n");
    }
}

static inline DMAtag
initialize_dma_tag (u64 value)
{
    DMAtag dma_tag              = {0};
    dma_tag.quadword_count      = (value & 0xFFFF);
    dma_tag.priority_control    = (value >> 26) & 0x3;
    dma_tag.tag_id              = (value >> 28) & 0x7;
    dma_tag.interrupt_request   = (value >> 31) & 0x1;
    dma_tag.address             = (value >> 32) & 0xFFFFFFF0;
    dma_tag.memory_selection    = (value >> 63) & 0x1;
    dma_tag.value.lo            = value;

    return dma_tag;
}

static void
source_chain_mode()
{
    // @Incomplete: This only works for GIF channel
    DMA_Channel *gif_channel                = &dmac.channels[2];
    u64 tag_data                            = ee_load_64(gif_channel->tag_address.address);
    
    // @Hack: find out a less hacky way to check tag_data
    if (tag_data == 0xcdcdcdcdcdcdcdcd) return;

    DMAtag dma_tag                          = initialize_dma_tag(tag_data);
    gif_channel->quadword_count.quadwords   = dma_tag.quadword_count;
    gif_channel->control.TAG                = dma_tag.value.lo & 0xFFFF0000;
    u16 quadwords                           = gif_channel->quadword_count.quadwords; // shorthand to reduce text vomit

    switch (dma_tag.tag_id)
    {
        case 0x0: 
        {
            gif_channel->address                = dma_tag.address;
            gif_channel->tag_address.address    += 16;
            //gif_channel->start                  = false;
            gif_channel->tag_end                = true;
            syslog("refe\n");
        } break; 

        case 0x1: 
        {
            gif_channel->address                = gif_channel->tag_address.address + 16;
            gif_channel->tag_address.address    = gif_channel->address + (quadwords * 16);
            syslog("cnt\n");
        } break; 

        case 0x2: 
        {
            gif_channel->address                = gif_channel->tag_address.address + 16;
            gif_channel->tag_address.address    = dma_tag.address;
            syslog("next\n");
        } break; 

        case 0x3: 
        {
            gif_channel->address                = dma_tag.address;
            gif_channel->tag_address.address    += 16;
            syslog("ref\n");
        } break; 

        case 0x4: 
        {
            gif_channel->address                = dma_tag.address;
            gif_channel->tag_address.address    += 16;
            syslog("refs\n");
        } break; 

        case 0x5: 
        {
            u32 temp                = gif_channel->address;
            gif_channel->address    = gif_channel->tag_address.address + 16;
            u32 stack_pointer       = gif_channel->control.stack_pointer;

            if (stack_pointer == 0) {
                gif_channel->save_tag0 = temp + (quadwords * 16);
            } else if (stack_pointer == 1) {
                gif_channel->save_tag1 = temp + (quadwords * 16);
            }

            gif_channel->tag_address.address = dma_tag.address;
            gif_channel->control.stack_pointer++;
            syslog("call\n");
        } break; 

        case 0x6: 
        {
            gif_channel->address    = gif_channel->tag_address.address + 16;
            u32 stack_pointer       = gif_channel->control.stack_pointer;
           
            if (stack_pointer == 2) {
                gif_channel->tag_address.address = gif_channel->save_tag1;
            } else if (stack_pointer == 1) {
                gif_channel->tag_address.address = gif_channel->save_tag0;
            }
            
            gif_channel->control.stack_pointer--;

            if (stack_pointer == 0) {
                gif_channel->tag_end = true;
            }

            syslog("ret\n");
        } break; 

        case 0x7: 
        {
            gif_channel->address = gif_channel->tag_address.address + 16;
            gif_channel->tag_end = true;
            syslog("end\n");
        } break; 

        default:
        {
            syslog("Unknwown DMA tag id\n");
        } break;
    }

    syslog("New Tag Address: [{:#08x}]\n", gif_channel->tag_address.value);
    syslog("New Address: [{:#08x}]\n", gif_channel->address);
}

void
dmac_cycle ()
{
    u128 data = { 0 };

    if (!dmac.control.enable) return;
    if (!dmac.channels[2].control.start) return;
    
    if (dmac.channels[2].quadword_count.quadwords) {
        data.lo = ee_load_64(dmac.channels[2].address);
        data.hi = ee_load_64(dmac.channels[2].address + 8);

        // u8 mode = dmac.channels[2].control.mode;
        gif_process_path3(data);

        dmac.channels[2].address += 16;
        // dmac.channels[2].quadword_count.value -= 1;
        dmac.channels[2].quadword_count.quadwords -= 1;
        
        // @Incomplete: Transfers channels when they eventually exist:
        //vu1_send_path1()
        //vif_send_path2()

    } else {
        if (dmac.channels[2].tag_end) {
            end_of_transfer();
        } else {
            source_chain_mode();
        }
    } 
}

static inline void
set_dmac_channel_values (u32 address, u32 index, u32 value)
{
    u32 ad = address & 0x000000FF;
        switch (ad) {
            case 0x00:
            {
                dmac.channels[index].control.dir            = (value >> 0) & 0x1;
                dmac.channels[index].control.mode           = (value >> 2) & 0x3;
                dmac.channels[index].control.stack_pointer  = (value >> 4) & 0x3;
                dmac.channels[index].control.tag_transfer   = (value >> 6) & 0x1;
                dmac.channels[index].control.tag_interrupt  = (value >> 7) & 0x1;
                dmac.channels[index].control.start          = (value >> 8) & 0x1;
                dmac.channels[index].control.TAG            = value >> 16;  
                dmac.channels[index].control.value          = value;  
                // @Cleanup: Move to a dma kick function
                if (dmac.channels[index].control.start == 1)
                    dmac.channels[index].tag_end = (dmac.channels[index].control.mode == 0);
                syslog("_CHCR, value: [{:#08x}]\n", value);
            } break;
            case 0x10:
            {
                dmac.channels[index].address = value & 0x7FFFFFF0;
                syslog("_MADR, value: [{:#08x}]\n", value);
            } break;
            case 0x20:
            {
                dmac.channels[index].quadword_count.quadwords = value & 0xFFFF;
                dmac.channels[index].quadword_count.value     = value;
                // Just printing the exact decimal number of quadwords for debugging
                syslog("_QWC, value: [{:d}]\n", value);
            } break;
            case 0x30:
            {
                dmac.channels[index].tag_address.address            = value & 0x7FFFFFF0;
                dmac.channels[index].tag_address.memory_selection   = (value >> 31) & 0x1;
                dmac.channels[index].tag_address.value              = value;
                syslog("_TADR, value: [{:#08x}]\n", value);
            } break;
            case 0x40:
            {
                dmac.channels[index].save_tag0 = value;
                syslog("_ASR0, value: [{:#08x}]\n", value);
            } break;
            case 0x50:
            {
                dmac.channels[index].save_tag1 = value;
                syslog("_ASR1, value: [{:#08x}]\n", value);
            } break;
            case 0x80:
            {
                dmac.channels[index].scratchpad_address.addess  = value & 0xFFFF;
                dmac.channels[index].scratchpad_address.value   = value;
                syslog("_SADR, value: [{:#08x}]\n", value);
            } break;
        }
    return;
}

static inline u32
read_dmac_channel_values (u32 address, u32 index)
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
            return (u32)dmac.channels[index].quadword_count.quadwords;
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
dmac_write (u32 address, u32 value)
{
    u32 reg = address;
    switch (reg) {
    /******************
    * DMA Registers
    *******************/
        case D_CTRL:
        {
            dmac.control.enable                 = (value >> 0) & 0x1;
            dmac.control.cycle_stealing         = (value >> 1) & 0x1;    
            dmac.control.memory_drain_channel   = (value >> 2) & 0x3;  
            dmac.control.stall_source_channel   = (value >> 4) & 0x3;  
            dmac.control.stall_drain_channel    = (value >> 6) & 0x3;  
            dmac.control.release_cycle          = (value >> 8) & 0x7;
            dmac.control.value                  = value;
            syslog("DMAC_WRITE: to D_CTRL value: [{:#08x}]\n", value);
        } break;

        case D_STAT:
        {
            for (int i = 0; i < DMAC_CHANNEL_COUNT; ++i) {
                if (value & (1 << i))           dmac.interrupt_status.channel_status[i] = false;
                if (value & (1 << (i + 16)))    dmac.interrupt_status.channel_mask[i]   ^= 1;
            }
            dmac.interrupt_status.stall_status        &= ~(value & ( 1 << 13));
            dmac.interrupt_status.mem_empty_status    &= ~(value & ( 1 << 14)); 
            dmac.interrupt_status.BUSERR_status       &= ~(value & ( 1 << 15)); 
            dmac.interrupt_status.stall_mask          = (value >> 29) & 0x1; 
            dmac.interrupt_status.mem_empty_mask      = (value >> 30) & 0x1; 
            //dmac.interrupt_status.value               = value;
            syslog("DMAC_WRITE: to D_STAT value: [{:#08x}]\n", value);
        } break;

        case D_PCR:
        {
            for (int i = 0; i <= 9; ++i) {
                dmac.priorty.cop_control[i]         = (value >> i) & 0x1;
                dmac.priorty.channel_dma_enable[i]  = (value >> (i+16)) & 0x1;
            }
            dmac.priorty.control_enable = (value >> 31) & 0x1;
            dmac.priorty.value          = value;
            syslog("DMAC_WRITE: to D_PCR value: [{:#08x}]\n", value);
        } break;

        case D_SQWC:
        {
            dmac.skip_quadword.skip_quadword_counter        = (value >> 0) & 0xFF;
            dmac.skip_quadword.transfer_quadword_counter    = (value >> 16) & 0xFF;
            dmac.skip_quadword.value                        = value;
            syslog("DMAC_WRITE: to D_SQWC value: [{:#08x}]\n", value);
        } break;

        case D_RBOR:
        {
            dmac.ringbuffer_offset.buffer_address   = value & 0x7FFFFFFF;
            dmac.ringbuffer_offset.value            = value;
            syslog("DMAC_WRITE: to D_RBOR value: [{:#08x}]\n", value);
        } break;

        case D_RBSR:
        {
            dmac.ringbuffer_size.buffer_size    = (value >> 4) & 0x7FFFFFF;
            dmac.ringbuffer_size.value          = value;
            syslog("DMAC_WRITE: to D_RBSR value: [{:#08x}]\n", value);
        } break;

        case D_STADR:
        {
            dmac.stall_address.address  = value & 0x7FFFFFFF;
            dmac.stall_address.value    = value;
            syslog("DMAC_WRITE: to D_STADR value: [{:#08x}]\n", value);
        } break;
        
        case D_ENABLEW:
        {
            dmac.hold_control.hold_dma_transfer = (value >> 16) & 0x1;
            syslog("DMAC_WRITE: to D_ENABLEW value: [{:#08x}]\n", value);
        } break;
        #if 0
        default:
        {
            errlog("[ERROR]Unhandled Write32 to DMAC addr: [{:#08x}] value: [{:#08x}]\n", address, value);
        } break;
        #endif
    }
    /******************
    * DMA Channels
    *******************/
    u32 channel = address & 0xFFFFFF00;
    switch(channel)
    {
        case DMA_VIF0:
        {
            syslog("DMAC_WRITE: to VIF0");
            set_dmac_channel_values(address, 0, value);
        } break;

        case DMA_VIF1:
        {
            syslog("DMAC_WRITE: to VIF1");
            set_dmac_channel_values(address, 1, value);
        } break;
        
        case DMA_GIF:
        {
            syslog("DMAC_WRITE: to GIF");
            set_dmac_channel_values(address, 2, value);
        } break;
       
        case DMA_IPU_FROM:
        {
            syslog("DMAC_WRITE: to IPU_FROM");
            set_dmac_channel_values(address, 3, value);
        } break;
       
       case DMA_IPU_TO:
        {
            syslog("DMAC_WRITE: to IPU_TO");
            set_dmac_channel_values(address, 4, value);
        } break;
        
        case DMA_SIF0:
        {
            // from IOP
            syslog("DMAC_WRITE: from SIF0");
            set_dmac_channel_values(address, 5, value);
        } break;
       
        case DMA_SIF1:
        {
            // to IOP
            syslog("DMAC_WRITE: to SIF1");
            set_dmac_channel_values(address, 6, value);
        } break;
        
        case DMA_SIF2:
        {
            // bidirectiondal
            syslog("DMAC_WRITE: to SIF2");
            set_dmac_channel_values(address, 7, value);
        } break;
        
        case DMA_SPR_FROM:
        {
            syslog("DMAC_WRITE: to SPR_FROM");
            set_dmac_channel_values(address, 8, value);
        } break;
       
        case DMA_SPR_TO:
        {
            syslog("DMAC_WRITE: to SPR_TO");
            set_dmac_channel_values(address, 9, value);
        } break;
#if 0
        default:
        {
            printf("Unhandled Write32 to DMAC addr: [{:#08x}] value: [{:#08x}]\n", address, value);
        } break;
#endif
    }
    return;
}

u32
dmac_read (u32 address)
{
    u32 reg = address;
    switch(reg)
    {
    /******************
    * DMA Registers
    *******************/
        case D_CTRL:
        {
            syslog("DMAC_READ: from D_CTRL\n");
            return dmac.control.value;
        } break;

        case D_STAT:
        {
            syslog("DMAC_READ: from D_STAT\n");
            u32 r = 0;
            // r |= 0;
            for (int i = 0; i < DMAC_CHANNEL_COUNT; ++i) {
                r |= dmac.interrupt_status.channel_status[i];
                r |= dmac.interrupt_status.channel_mask[i];
            }
            r |= dmac.interrupt_status.stall_status;        
            r |= dmac.interrupt_status.mem_empty_status;    
            r |= dmac.interrupt_status.BUSERR_status;       
            r |= dmac.interrupt_status.stall_mask;     
            r |= dmac.interrupt_status.mem_empty_mask;      
            return r;
        } break;
        
        case D_PCR:
        {
            syslog("DMAC_READ: from D_PCR\n");
            return dmac.priorty.value;
        } break;
        
        case D_SQWC:
        {
            syslog("DMAC_READ: from D_SQWC\n");
            return dmac.skip_quadword.value;
        } break;
        
        case D_RBSR:
        {
            syslog("DMAC_READ: from D_RBSR\n");
            return dmac.ringbuffer_size.value;
        } break;
        
        case D_RBOR:
        {
            syslog("DMAC_READ: from D_RBOR\n");
            return dmac.ringbuffer_offset.value;
        } break;
        
        case D_STADR:
        {
            syslog("DMAC_READ: from D_STADR\n");
            return dmac.stall_address.value;
        } break;

        case D_ENABLER:
        {
            syslog("DMAC_READ: from D_ENABLER\n");
            return dmac.hold_state.value;
        } break;
       #if 0 
        default:
        {
            printf("Unhandled Read32 to DMAC [{:#08x}]\n", address);
        } break;
        #endif
    }

    u32 channel = address &= 0xFFFFFF00;
    switch(channel)
    {
        case DMA_VIF0:
        {
            syslog("DMAC_READ: from VIF0\n");
            return read_dmac_channel_values(address, 0);
        } break;

        case DMA_VIF1:
        {
            syslog("DMAC_READ: from VIF1(PATH2 to GIF)\n");
            return read_dmac_channel_values(address, 1);         
        } break;

        case DMA_GIF:
        {
            // syslog("DMAC_READ: from GIF(PATH3) address\n");
            return read_dmac_channel_values(address, 2);
        } break;

        case DMA_IPU_FROM:
        {
            syslog("DMAC_READ: from IPU_FROM\n");
            return read_dmac_channel_values(address, 3);
        } break;

        case DMA_IPU_TO:
        {
            syslog("DMAC_READ: from IPU_TO\n");
            return read_dmac_channel_values(address, 4);
        } break;

        case DMA_SIF0:
        {
            syslog("DMAC_READ: from SIF0(from IOP)\n");
            return read_dmac_channel_values(address, 5);
        } break;

        case DMA_SIF1:
        {
            syslog("DMAC_READ: from SIF1(to IOP)\n");
            return read_dmac_channel_values(address, 6);
        } break;

        case DMA_SIF2:
        {
            syslog("DMAC_READ: from SIF2(bidirectional)\n");
            return read_dmac_channel_values(address, 7);
        } break;

        case DMA_SPR_FROM:
        {
            syslog("DMAC_READ: from SPR_FROM\n");
            return read_dmac_channel_values(address, 8);
        } break;

        case DMA_SPR_TO:
        {
            syslog("DMAC_READ: from SPR_TO\n");
            return read_dmac_channel_values(address, 9);
        } break;
        #if 0
        default:
        {
            printf("Unhandled Read32 to DMAC [{:#08x}]\n", address);
        } break;
        #endif
    }

    return 0;
}