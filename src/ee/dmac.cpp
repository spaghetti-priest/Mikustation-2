/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include "../include/ee/dmac.h"
#include "../include/ee/gif.h"
#include "../include/ps2.h"
#include <iostream>

DMAC dmac = {0};
const bool stop_dma_transfer = 0x100;

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

static inline void
set_dmac_channel_values (u32 address, u32 index, u32 value)
{
    u32 ad = address & 0x000000FF;
    switch (ad) {
        case 0x00:
        {
            dmac.channels[index].control.dir            = (value >> 0) & 0x1;
            dmac.channels[index].control.mode           = value >> 2 & 0x3;
            dmac.channels[index].control.stack_pointer  = value >> 4 & 0x3;
            dmac.channels[index].control.tag_transfer   = (value >> 6) & 0x1;
            dmac.channels[index].control.tag_interrupt  = (value >> 7) & 0x1;
            dmac.channels[index].control.start          = (value >> 8) & 0x1;
            dmac.channels[index].control.TAG            = value >> 16;  
            //printf("_CHCR, value: [%#08x]\n", value);
        } break;
        case 0x10:
        {
            dmac.channels[index].address = value & 0x7FFFFFF0;
            //printf("_MADR, value: [%#08x]\n", value);
        } break;
        case 0x20:
        {
            dmac.channels[index].quadword_count.quadwords = value & 0xFFFF;
            //printf("_QWC, value: [%#08x]\n", value);
        } break;
        case 0x30:
        {
            dmac.channels[index].tag_address.address            = value & 0x7FFFFFF0;
            dmac.channels[index].tag_address.memory_selection   = (value >> 31) & 0x1;
            //printf("_TADR, value: [%#08x]\n", value);
        } break;
        case 0x40:
        {
            dmac.channels[index].save_tag0 = value;
            //printf("_ASR0, value: [%#08x]\n", value);
        } break;
        case 0x50:
        {
            dmac.channels[index].save_tag1 = value;
            //printf("_ASR1, value: [%#08x]\n", value);
        } break;
        case 0x80:
        {
            dmac.channels[index].scratchpad_address.addess = value & 0xFFFF;
            //printf("_SADR, value: [%#08x]\n", value);
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

static inline void
process_normal_transfer (int index, bool is_burst)
{
    /*
    dmac.channels[index].start = 1;
    if (is_burst) {

    } else {

    }

    dmac.channels[index].STR = 0;
    */
}

//@@Incomplete: This only works for GIF channel aswell as normal and interleave transfer mode
static void
end_of_transfer()
{
    if (dmac.channels[2].quadword_count.value == 0) {
        dmac.channels[2].control.start = false;

        dmac.interrupt_status.channel_status[2] = true;
        bool int1 = (dmac.interrupt_status.channel_status[2] & dmac.interrupt_status.channel_mask[2]) != 0;
        check_interrupt(int1, false, true);
        //printf("End of transfer\n");
    }
}

static inline DMAtag
initialize_dma_tag (u64 value)
{
    DMAtag dma_tag = {
        .quadword_count      = (u16)(value & 0xFFFF),
        .priority_control    = (u8)((value >> 26) & 0x3),
        .tag_id              = (u8)((value >> 28) & 0x7),
        .interrupt_request   = (bool)((value >> 31) & 0x1),
        .address             = (u32)((value >> 32) & 0xFFFFFFF0),
        .memory_selection    = (bool)((value >> 63) & 0x1),
        .value.lo            = value,
    };

    return dma_tag;
}

//@@Incomplete: This only works for GIF channel
static void
source_chain_mode()
{
    u32 tag_address_value                       = ee_load_64(dmac.channels[2].tag_address.address);
    DMAtag dma_tag                              = initialize_dma_tag(tag_address_value);
    dmac.channels[2].quadword_count.quadwords   = dma_tag.quadword_count;
    dmac.channels[2].control.TAG                = dma_tag.value.lo & 0xFFFF0000;

    // cut down on verbosity
    u16 quadwords = dmac.channels[2].quadword_count.quadwords;
    switch (dma_tag.tag_id)
    {
        /*refe*/
        case 0x0: 
        {
            dmac.channels[2].address = dma_tag.address;
            dmac.channels[2].tag_address.address += 16;
            dmac.channels[2].tag_end = true;
            printf("refe\n");
        } break; 
        /*cnt*/
        case 0x1: 
        {
            dmac.channels[2].address = dmac.channels[2].tag_address.address + 16;
            dmac.channels[2].tag_address.address = dmac.channels[2].address + (quadwords * 16);
            printf("cnt\n");
        } break; 
        /*next*/
        case 0x2: 
        {
            dmac.channels[2].address = dmac.channels[2].tag_address.address + 16;
            dmac.channels[2].tag_address.address = dma_tag.address;
            printf("next\n");
        } break; 
        /*ref*/
        case 0x3: 
        {
            dmac.channels[2].address = dma_tag.address;
            dmac.channels[2].tag_address.address += 16;
            printf("ref\n");
        } break; 
        /*refs*/
        case 0x4: 
        {
            dmac.channels[2].address = dma_tag.address;
            dmac.channels[2].tag_address.address += 16;
            printf("refs\n");
        } break; 
        /*call*/
        case 0x5: 
        {
            u32 temp = dmac.channels[2].address;
            dmac.channels[2].address = dmac.channels[2].tag_address.address + 16;

            if (dmac.channels[2].control.stack_pointer == 0) {
                dmac.channels[2].save_tag0 = temp + (quadwords * 16);
            } else if (dmac.channels[2].control.stack_pointer == 1) {
                dmac.channels[2].save_tag1 = temp + (quadwords * 16);
            }

            dmac.channels[2].tag_address.address = dma_tag.address;
            dmac.channels[2].control.stack_pointer++;
            printf("call\n");
        } break; 
        /*ret*/
        case 0x6: 
        {
            dmac.channels[2].address = dmac.channels[2].tag_address.address + 16;
           
            if (dmac.channels[2].control.stack_pointer == 2) {
                dmac.channels[2].tag_address.address = dmac.channels[2].save_tag1;
                dmac.channels[2].control.stack_pointer--;
            } else if (dmac.channels[2].control.stack_pointer == 1) {
                dmac.channels[2].tag_address.address = dmac.channels[2].save_tag0;
                dmac.channels[2].control.stack_pointer--;
            } else {
                dmac.channels[2].tag_end = true;
            }

            printf("ret\n");
        } break; 
        /*end*/
        case 0x7: 
        {
            dmac.channels[2].address = dmac.channels[2].tag_address.address + 16;
            dmac.channels[2].tag_end = true;
            printf("end\n");
        } break; 

        default:
        {
            printf("Unknwown DMA tag id\n");
        } break;
    }
    printf("New Tag Address: [%#08x]\n", dmac.channels[2].tag_address.value);
    printf("New Address: [%#08x]\n", dmac.channels[2].address);
}

//@@Incomplete @@Refactor: Rewrite this function because it sucks and is broken
void
dmac_cycle ()
{
    u128 data = { 0 };

    if (!dmac.control.enable) return;
    if (!dmac.channels[2].control.start) return;
    
    if (dmac.channels[2].quadword_count.quadwords) {
        data.lo = ee_load_64(dmac.channels[2].address);
        data.hi = ee_load_64(dmac.channels[2].address + 8);

        u8 mode = dmac.channels[2].control.mode;
        switch(mode)
        {
            case 0x0: 
            {
               // process_normal_transfer();
                //printf("Normal DMAC Transfer\n");
            } break; 

            case 0x1: 
            {
                printf("Chain DMAC Transfer\n");
            } break; 

            case 0x2: 
            {
                printf("Interleave DMAC Transfer\n");
            } break; 
        }

        /* GIF Cycle */
        gif_process_path3(data);

        dmac.channels[2].address += 16;
        dmac.channels[2].quadword_count.quadwords -= 1;
        
        // @Incomplete: Transfers channels when they eventually exist:
        //vu1_send_path1()
        //vif_send_path2()

    } else if (dmac.channels[2].tag_end) {
            end_of_transfer();
    } else {
            source_chain_mode();
    }
}

void 
dmac_write_32 (u32 address, u32 value)
{
    u32 reg = address;
    //printf("DMAC write value: [%#08x], addr: [%#x]\n", value, address);
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
            printf("WRITE: DMAC control value: [%#08x]\n", value);
        } break;

        case D_STAT:
        {
            for (int i = 0; i < DMAC_CHANNEL_COUNT; ++i) {
                if (value & (1 << i))           dmac.interrupt_status.channel_status[i] = false;
                if (value & (1 << (i + 16)))    dmac.interrupt_status.channel_mask[i]   ^= 1;
            }
            dmac.interrupt_status.stall_status        &= ~(value & ( 1 << 13));
            dmac.interrupt_status.mem_empty_status    &= ~(value & ( 1 << 13)); 
            dmac.interrupt_status.BUSERR_status       &= ~(value & ( 1 << 13)); 
            dmac.interrupt_status.stall_mask          = (value >> 29) & 0x1; 
            dmac.interrupt_status.mem_empty_mask      = (value >> 30) & 0x1; 
            //dmac.interrupt_status.value               = value;
            printf("WRITE: DMAC interrupt status value: [%#08x]\n", value);
        } break;

        case D_PCR:
        {
            for (int i = 0; i <= 9; ++i) {
                dmac.priorty.cop_control[i]         = (value >> i) & 0x1;
                dmac.priorty.channel_dma_enable[i]  = (value >> (i+16)) & 0x1;
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
            printf("WRITE: DMAC ringbuffer offset value: [%#08x]\n", value);
        } break;

        case D_RBSR:
        {
            dmac.ringbuffer_size.buffer_size    = (value >> 4) & 0x7FFFFFF;
            dmac.ringbuffer_size.value          = value;
            printf("WRITE: DMAC ringbuffer size value: [%#08x]\n", value);
        } break;

        case D_STADR:
        {
            dmac.stall_address.address  = value & 0x7FFFFFFF;
            dmac.stall_address.value    = value;
            printf("WRITE: DMAC stall address value: [%#08x]\n", value);
        } break;
        #if 0
        default:
        {
            printf("Unhandled Write32 to DMAC addr: [%#08x] value: [%#08x]\n", address, value);
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
            printf("WRITE: VIF0");
            set_dmac_channel_values(address, 0, value);
        } break;

        case DMA_VIF1:
        {
            printf("WRITE: VIF1");
            set_dmac_channel_values(address, 1, value);
        } break;
        
        case DMA_GIF:
        {
            //printf("WRITE: GIF");
            set_dmac_channel_values(address, 2, value);
        } break;
       
        case DMA_IPU_FROM:
        {
            printf("WRITE: IPU_FROM");
            set_dmac_channel_values(address, 3, value);
        } break;
       
       case DMA_IPU_TO:
        {
            printf("WRITE: IPU_TO");
            set_dmac_channel_values(address, 4, value);
        } break;
        
        case DMA_SIF0:
        {
            // from IOP
            printf("WRITE: SIF0");
            set_dmac_channel_values(address, 5, value);
        } break;
       
        case DMA_SIF1:
        {
            // to IOP
            printf("WRITE: SIF1");
            set_dmac_channel_values(address, 6, value);
        } break;
        
        case DMA_SIF2:
        {
            // bidirectiondal
            printf("WRITE: SIF2");
            set_dmac_channel_values(address, 7, value);
        } break;
        
        case DMA_SPR_FROM:
        {
            printf("WRITE: SPR_FROM");
            set_dmac_channel_values(address, 8, value);
        } break;
       
        case DMA_SPR_TO:
        {
            printf("WRITE: SPR_TO");
            set_dmac_channel_values(address, 9, value);
        } break;
#if 0
        default:
        {
            printf("Unhandled Write32 to DMAC addr: [%#08x] value: [%#08x]\n", address, value);
        } break;
        #endif
    }
    return;
}

u32
dmac_read_32 (u32 address)
{
    u32 reg = address;
    //printf("DMAC READ addr: [%#08x]\n", address);
    switch(reg)
    {
    /******************
    * DMA Registers
    *******************/
        case D_CTRL:
        {
            printf("READ: DMAC control\n");
            return dmac.control.value;
        } break;

        case D_STAT:
        {
            printf("READ: DMAC interrupt status\n");
            return dmac.interrupt_status.value;
        } break;
        
        case D_PCR:
        {
            printf("READ: DMAC priority control\n");
            return dmac.priorty.value;
        } break;
        
        case D_SQWC:
        {
            printf("READ: DMAC skipquadword\n");
            return dmac.skip_quadword.value;
        } break;
        
        case D_RBSR:
        {
            printf("READ: DMAC ringbuffer size\n");
            return dmac.ringbuffer_size.value;
        } break;
        
        case D_RBOR:
        {
            printf("READ: DMAC ringbuffer offset\n");
            return dmac.ringbuffer_offset.value;
        } break;
        
        case D_STADR:
        {
            printf("READ: DMAC stall address\n");
            return dmac.stall_address.value;
        } break;

         case D_ENABLER:
        {
            printf("READ: DMAC ENABLER\n");
            return dmac.disabled.value;
        } break;
       #if 0 
        default:
        {
            printf("Unhandled Read32 to DMAC [%#08x]\n", address);
        } break;
        #endif
    }

    u32 channel = address &= 0xFFFFFF00;
    switch(channel)
    {
        case DMA_VIF0:
        {
            printf("READ: DMAC read to VIF0\n");
            return read_dmac_channel_values(address, 0);
        } break;

        case DMA_VIF1:
        {
            printf("READ: DMAC READ to VIF1(PATH2 to GIF)\n");
            return read_dmac_channel_values(address, 1);         
        } break;

        case DMA_GIF:
        {
            //printf("READ: DMAC READ to GIF(PATH3) address\n");
            return read_dmac_channel_values(address, 2);
        } break;

        case DMA_IPU_FROM:
        {
            printf("READ: DMAC READ to IPU_FROM\n");
            return read_dmac_channel_values(address, 3);
        } break;

        case DMA_IPU_TO:
        {
            printf("READ: DMAC READ to IPU_TO\n");
            return read_dmac_channel_values(address, 4);
        } break;

        case DMA_SIF0:
        {
            printf("READ: DMAC READ to SIF0(from IOP)\n");
            return read_dmac_channel_values(address, 5);
        } break;

        case DMA_SIF1:
        {
            printf("READ: DMAC READ to SIF1(to IOP)\n");
            return read_dmac_channel_values(address, 6);
        } break;

        case DMA_SIF2:
        {
            printf("READ: DMAC READ to SIF2(bidirectional)\n");
            return read_dmac_channel_values(address, 7);
        } break;

        case DMA_SPR_FROM:
        {
            printf("READ: DMAC READ to SPR_FROM\n");
            return read_dmac_channel_values(address, 8);
        } break;

        case DMA_SPR_TO:
        {
            printf("READ: DMAC READ to SPR_TO\n");
            return read_dmac_channel_values(address, 9);
        } break;
        #if 0
        default:
        {
            printf("Unhandled Read32 to DMAC [%#08x]\n", address);
        } break;
        #endif
    }

    return 0;
}