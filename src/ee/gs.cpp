/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include <iostream>
#include "../include/ee/gs.h"

alignas(16) GraphicsSynthesizer gs = {0};

void 
gs_reset ()
{
    memset(&gs, 0, sizeof(gs));
    printf("Resetting Graphics Synthesizer\n");
    gs.vram = (u16*)malloc(sizeof(u16) * MEGABYTES(4));
}

void
gs_set_q (f32 value)
{
    gs.rgbaq.q = value;
//    printf("GS SET Q\n");
}

void
gs_set_rgbaq (u32 r, u32 g, u32 b, u32 a)
{
    gs.rgbaq.r = r;
    gs.rgbaq.g = g;
    gs.rgbaq.b = b;
    gs.rgbaq.a = a;
    printf("GS SET RGBAQ\n");
}

void 
gs_set_st (f32 s, f32 t)
{
    gs.st.s = s;
    gs.st.t = t;
    printf("GS SET ST\n");
}

void 
gs_set_uv (u32 u, u32 v)
{
    gs.uv.u = u;
    gs.uv.v = v;
    printf("GS SET UV\n");
}

void  
gs_set_fog (u8 fog)
{
    gs.fog.fog = fog;
    printf("GS SET FOG\n");
}

void 
gs_set_xyzf2 (s16 x, s16 y, u32 z, u8 f)
{
    gs.xyzf2.x = x;
    gs.xyzf2.y = y;
    gs.xyzf2.z = z;
    gs.xyzf2.f = f;

    // @Incomplete:
    // Add Drawing Vertex Kick
    printf("GS SET XYZF2\n");
}

void 
gs_set_xyzf3 (s16 x, s16 y, u32 z, u8 f)
{
    gs.xyzf3.x = x;
    gs.xyzf3.y = y;
    gs.xyzf3.z = z;
    gs.xyzf3.f = f;
    printf("GS SET XYZF3\n");
}

void 
gs_set_xyz2 (s16 x, s16 y, u32 z)
{
    gs.xyz2.x = x;
    gs.xyz2.y = y;
    gs.xyz2.z = z;

    /* @@Incomplete: Vertex Kick Here */
    printf("GS SET XYZ2\n");
}

void 
gs_set_xyz3 (s16 x, s16 y, u32 z)
{
    gs.xyz3.x = x;
    gs.xyz3.y = y;
    gs.xyz3.z = z;
    printf("GS SET XYZ3\n");
}

void
gs_set_crt (bool interlaced, s32 display_mode, bool ffmd)
{
    gs.smode2.interlace_mode    = interlaced;
    gs.smode2.interlace_setting = ffmd; 
    switch(display_mode)
    {
        case 0x02: gs.crt_mode = CRT_MODE_NTSC;     break;
        case 0x03: gs.crt_mode = CRT_MODE_PAL;      break;
        case 0x50: gs.crt_mode = CRT_MODE_DTV_480P; break;
    }
}

void
gs_write_hwreg(u64 data)
{
    gs.hwreg.data = data;
}

bool v_sync_interrupt = false;
bool v_sync_generated = false;

u32 
gs_read_32_priviledged (u32 address) 
{
    switch(address) 
    {
        case 0x12001000: 
        {
            //@@Incomplete: @@Hack This is 0x8 in order to fire off the vsync interrupt
            // im not sure if 3stars writes the interrupt flag in csr and so im
            // just doing this hack in order to continued onwards since it loops forever
            printf("GS_READ32: read from CSR value\n");
            return 0x8;
            //return gs.csr.value;
        } break;
        default: 
        {
             printf("ERROR: UNRECOGNIZED READ GS PRIVILEDGE32: address[%#x]\n", address);
             return 0;
        } break;

    }
    return 0;
}

u64 
gs_read_64_priviledged (u32 address) 
{
    switch(address) 
    {
        case 0x12001000: 
        {
            //@@Incomplete: @@Hack This is 0x8 in order to fire off the vsync interrupt
            // im not sure if 3stars writes the interrupt flag in csr and so im
            // just doing this hack in order to continued onwards since it loops forever
            printf("GS_READ: read from CSR\n");
            return 0x8;
            //return gs.csr.value;
        } break;
        default: 
        {
             printf("ERROR: UNRECOGNIZED READ GS PRIVILEDGE64: address[%#x]\n", address);
             return 0;
        } break;

    }
    return 0;
}

void 
gs_write_32_priviledged (u32 address, u32 value)
{
    //printf("WRITE PRIVILEDGE32: Graphics synthesizer value: [%#09x], addr[%#x]\n", value, address);
    switch(address)
    {
        case 0x12001000:
        {
        #if 0
            printf("GS_WRITE32: write to CSR. Value: [%#08x]\n", value);
            gs.csr.signal                   = value & 0x1;
            gs.csr.finish                   = (value >> 1) & 0x1;
            gs.csr.h_interrupt              = (value >> 2) & 0x1;
            gs.csr.v_interrupt              = (value >> 3) & 0x1;
            gs.csr.write_termination_control = (value >> 4) & 0x1;
            gs.csr.unused[0]                = 0;
            gs.csr.unused[1]                = 0;
            gs.csr.flush                    = (value >> 8) & 0x1;
            gs.csr.reset                    = (value >> 9) & 0x1;
            gs.csr.NFIELD_output            = (value >> 13) & 0x1;
            gs.csr.field_display            = (value >> 14) & 0x3;
            gs.csr.revision_num             = (value >> 16) & 0xFF;
            gs.csr.id                       = (value >> 24) & 0xFF;
            gs.csr.value = value;
            #endif
            printf("GS_WRITE32: write to CSR. Value: [%#08x]\n", value);
            if (value & 0x8) {//(gs.csr.v_interrupt) {
                v_sync_interrupt = true;
                v_sync_generated = false;
            }
            return;
        } break;

        default:
        {
             printf("ERROR: UNRECOGNIZED WRITE GS PRIVILEDGE32: address[%#x]\n", address);
            return;
        } break;
    }
}

void 
gs_write_64_priviledged (u32 address, u64 value)
{
    switch(address)
    {
        case 0x12000000:
        {
            printf("GS_WRITE_PRIVILEDGED: write to PMODE. Value: [%#08x]\n", value);
            gs.pmode.value              = value;
            gs.pmode.circuit1           = value & 0x1;
            gs.pmode.circuit2           = (value >> 1) & 0x1;
            gs.pmode.CRT                = (value >> 2) & 0x3;
            gs.pmode.value_selection    = (value >> 5) & 0x1;   
            gs.pmode.output_selection   = (value >> 6) & 0x1;  
            gs.pmode.blending_selection = (value >> 7) & 0x1;
            gs.pmode.alpha_value        = (value >> 8) & 0xFF;    
            return;
        } break;

        case 0x12000010:
        {
            printf("GS_WRITE_PRIVILEDGED: write to SMODE1. Value: [%#08x]\n", value);
            gs.smode1.value                 = value;
            return;
        } break;

        case 0x12000020:
        {
            printf("GS_WRITE_PRIVILEDGED: write to SMODE2. Value: [%#08x]\n", value);
            gs.smode2.value                 = value;
            gs.smode2.interlace_mode        = value & 0x1;
            gs.smode2.interlace_setting     = (value >> 1) & 0x1;
            gs.smode2.mode                  = (value >> 2) & 0x3;
            return;
        } break;

        case 0x12000030:
        {
            printf("GS_WRITE_PRIVILEDGED: write to SRFSH. Value: [%#08x]\n", value);
            gs.srfsh.value = value;
            return;
        } break;

        case 0x12000040:
        {
            printf("GS_WRITE_PRIVILEDGED: write to SYNCH1. Value: [%#08x]\n", value);
            gs.synch1.value = value;
            return;
        } break;

        case 0x12000050:
        {
            printf("GS_WRITE_PRIVILEDGED: write to SYNCH2. Value: [%#08x]\n", value);
            gs.synch2.value = value;
            return;
        } break;

        case 0x12000060:
        {
            printf("GS_WRITE_PRIVILEDGED: write to SYNCV. Value: [%#08x]\n", value);
            gs.syncv.value = value;
            return;
        } break;
        
        case 0x12000070:
        {
            printf("GS_WRITE_PRIVILEDGED: write to DISPFB1. Value: [%#08x]\n", value);
            gs.dispfb1.value            = value;
            gs.dispfb1.base_pointer     = (value & 0x1FF) * 2048;
            gs.dispfb1.buffer_width     = ((value >> 9) & 0x2F) * 48;
            gs.dispfb1.storage_formats  = (value >> 15) & 0x1F;
            gs.dispfb1.x_position       = (value >> 32) & 0x7FF;
            gs.dispfb1.x_position       = (value >> 43) & 0x7FF;
            return;
        } break;
        
        case 0x12000080:
        {
            printf("GS_WRITE_PRIVILEDGED: write to DISPLAY1. Value: [%#08x]\n", value);
            gs.display1.value           = value;
            gs.display1.x_position      = value & 0xFFF;
            gs.display1.y_position      = (value >> 11) & 0xFFF;
            gs.display1.h_magnification = (value >> 23) & 0xF;
            gs.display1.v_magnification = (value >> 27) & 0x3;
            gs.display1.area_width      = (value >> 32) & 0xFFF;
            gs.display1.area_height     = (value >> 44) & 0x7FF;
            return;
        } break;
        
        case 0x12000090:
        {
            printf("GS_WRITE_PRIVILEDGED: write to DISPFB2. Value: [%#08x]\n", value);
            gs.dispfb2.value            = value;
            gs.dispfb2.base_pointer     = (value & 0x1FF) * 2048;
            gs.dispfb2.buffer_width     = ((value >> 9) & 0x2F) * 48;
            gs.dispfb2.storage_formats  = (value >> 15) & 0x1F;
            gs.dispfb2.x_position       = (value >> 32) & 0x7FF;
            gs.dispfb2.x_position       = (value >> 43) & 0x7FF;
            return;
        } break;
        
        case 0x120000A0:
        {
            printf("GS_WRITE_PRIVILEDGED: write to DISPLAY2. Value: [%#08x]\n", value);
            gs.display2.value           = value;
            gs.display2.x_position      = value & 0xFFF;
            gs.display2.y_position      = (value >> 11) & 0xFFF;
            gs.display2.h_magnification = (value >> 23) & 0xF;
            gs.display2.v_magnification = (value >> 27) & 0x3;
            gs.display2.area_width      = (value >> 32) & 0xFFF;
            gs.display2.area_height     = (value >> 44) & 0x7FF;
            return;
        } break;
        
        case 0x120000B0:
        {
            printf("GS_WRITE_PRIVILEDGED: write to EXTBUF. Value: [%#08x]\n", value);
            gs.extbuf.value                 = value;
            gs.extbuf.base_pointer          = (value & 0x3FFF) * 64;
            gs.extbuf.buffer_width          = ((value >> 14) & 0x3F) * 64;
            gs.extbuf.input_selection       = (value >> 20) & 0x3;
            gs.extbuf.interlace_mode        = (value >> 22) * 0x1;
            gs.extbuf.input_alpha_method    = (value >> 23) & 0x3;
            gs.extbuf.input_color_method    = (value >> 25) & 0x3;
            gs.extbuf.x_coord               = (value >> 32) & 0x7FF;
            gs.extbuf.y_coord               = (value >> 43) & 0x7FF;
            return;
        } break;
        
        case 0x120000C0:
        {
            printf("GS_WRITE_PRIVILEDGED: write to EXTDATA. Value: [%#08x]\n", value);
            gs.extdata.value            = value;
            gs.extdata.x_coord          = value & 0xFFF;
            gs.extdata.y_coord          = (value >> 12) & 0x7FF;
            gs.extdata.h_sampling_rate  = (value >> 23) & 0xFF;
            gs.extdata.v_sampling_rate  = (value >> 27) & 0x3;
            gs.extdata.width            = ((value >> 32) & 0xFFF) + 1;
            gs.extdata.height           = ((value >> 44) & 0x7FF) + 1;
            return;
        } break;
        
        case 0x120000D0:
        {   
            printf("GS_WRITE_PRIVILEDGED: write to EXTWRITE. Value: [%#08x]\n", value);
            gs.extwrite.activated = value & 0x1;
            return;
        } break;
        
        case 0x120000E0:
        {
            printf("GS_WRITE_PRIVILEDGED: write to BGCOLOR. Value: [%#08x]\n", value);
            gs.bgcolor.red      = value & 0xFF;
            gs.bgcolor.green    = (value >> 8) & 0xFF;
            gs.bgcolor.blue     = (value >> 16) & 0xFF;
            return;
        } break;
        
        case 0x12001000:
        {
            #if 0
            printf("GS_WRITE_PRIVILEDGED: write to CSR. Value: [%#08x]\n", value);
            gs.csr.signal                   = value & 0x1;
            gs.csr.finish                   = (value >> 1) & 0x1;
            gs.csr.h_interrupt              = (value >> 2) & 0x1;
            gs.csr.v_interrupt              = (value >> 3) & 0x1;
            gs.csr.write_termination_control = (value >> 4) & 0x1;
            gs.csr.unused[0]                = 0;
            gs.csr.unused[1]                = 0;
            gs.csr.flush                    = (value >> 8) & 0x1;
            gs.csr.reset                    = (value >> 9) & 0x1;
            gs.csr.NFIELD_output            = (value >> 13) & 0x1;
            gs.csr.field_display            = (value >> 14) & 0x3;
            gs.csr.revision_num             = (value >> 16) & 0xFF;
            gs.csr.id                       = (value >> 24) & 0xFF;
            gs.csr.value = value;
            #endif
            printf("GS_WRITE_PRIVILEDGED: write to CSR. Value: [%#08x]\n", value);
            if (value & 0x8) {
                v_sync_interrupt = true;
                v_sync_generated = false;
            }
            return;
        } break;
        
        case 0x12001010:
        {
            printf("GS_WRITE_PRIVILEDGED: write IMR. Value: [%#08x]\n", value);
            gs.imr.value                    = value;
            gs.imr.signal_mask              = (value >> 8) & 0x1;
            gs.imr.finish_mask              = (value >> 9) & 0x1;
            gs.imr.hsync_mask               = (value >> 10) & 0x1;
            gs.imr.vsync_mask               = (value >> 11) & 0x1;
            gs.imr.write_termination_mask   = (value >> 12) & 0x1;
            gs.imr.unused[0]                = 1;
            gs.imr.unused[1]                = 1;
            return;
        } break;
        
        case 0x12001040:
        {
            printf("GS_WRITE_PRIVILEDGED: write to BUSDIR. Value: [%#08x]\n", value);
            gs.busdir.direction = value & 0x1;
            return;
        } break;
        
        case 0x12001080:
        {
            printf("GS_WRITE_PRIVILEDGED: write to SIGLBLID. Value: [%#08x]\n", value);
            gs.siglbid.value        = value;
            gs.siglbid.signal_id    = value & 0xFFFF;
            gs.siglbid.label_id     = (value >> 32) & 0xFFFF;
            return;
        } break;
        
        default: 
        {
             printf("ERROR: UNRECOGNIZED WRITE GS PRIVILEDGE64: value: [%#09x], address[%#x]\n", value, address);
        } break;
       }
    return;
}

void 
gs_set_primitive (u64 value) 
{
    gs.prim.primative_type          = (value) & 0x7;
    gs.prim.shading_method          = (value >> 3) & 0x1;
    gs.prim.texture_mapping         = (value >> 4) & 0x1;
    gs.prim.fogging                 = (value >> 5) & 0x1;
    gs.prim.alpha_blending          = (value >> 6) & 0x1;
    gs.prim.antialiasing            = (value >> 7) & 0x1;
    gs.prim.mapping_method          = (value >> 8) & 0x1;
    gs.prim.context                 = (value >> 9) & 0x1;
    gs.prim.fragment_value_control  = (value >> 10) & 0x1;
    gs.prim.value                   = value;
}

enum Pixel_Storage_Format
{

};

void 
gs_write_64_internal (u8 address, u64 value) 
{
    switch(address)
    {
        case 0x00:
        {
            gs_set_primitive(value);
            printf("GS_WRITE_INTERNAL: write to PRIM. Value: [%#08x]\n", value);
        } break;

        case 0x01:
        {
            gs.rgbaq.r = value & 0xFF;
            gs.rgbaq.g = (value >> 8) & 0xFF;
            gs.rgbaq.b = (value >> 16) & 0xFF;
            gs.rgbaq.a = (value >> 24) & 0xFF;
            gs.rgbaq.q = (value >> 32);
            printf("GS_WRITE_INTERNAL: write to RGBAQ. Value: [%#08x]\n", value);
        } break;

        case 0x02:
        {
            /* Mostly compliant with iEEE 754 */
            gs.st.s = value & 0xFFFFFFFF;
            gs.st.t = (value >> 32);
            printf("GS_WRITE_INTERNAL: write to ST. Value: [%#08x]\n", value);
        } break;
        
        case 0x03:
        {
            gs.uv.u = (value >> 14) & 0x3FFFF;
            gs.uv.v = (value >> 32) & 0x3FFFF;
            printf("GS_WRITE_INTERNAL: write to UV. Value: [%#08x]\n", value);
        }

        case 0x04:
        {
            /* 
                @@Note: I dont know if the game dev checks whether or not the register
                has a max X and max Y (4095.9375) so check if that is the case in the 
                occasion there has to be a hardcoded solution
            */
            gs.xyzf2.value  = value;
            gs.xyzf2.x      = (value) & 0xFFFF;
            gs.xyzf2.y      = (value >> 16) & 0xFFFF;
            gs.xyzf2.z      = (value >> 32) & 0xFFFF;
            gs.xyzf2.f      = (value >> 56) & 0xFF;
            /* @@Incomplete: Vertex Kick Here */
            printf("GS_WRITE_INTERNAL: write to XYZF2. Value: [%#08x]\n", value);
        } break;

        case 0x05:
        {
            gs.xyz2.x = (value) * 0xFFFF;
            gs.xyz2.y = (value >> 16) * 0xFFFF;
            gs.xyz2.z = (value >> 32);
            /* @@Incomplete: Vertex Kick Here */
            printf("GS_WRITE_INTERNAL: write to XYZ2. Value: [%#08x]\n", value);
        } break;

        case 0x18:
        {
            gs.xyoffset_1.offset_x = (value) & 0xFFFF;
            gs.xyoffset_1.offset_y = (value >> 32) & 0xFFFF;
            printf("GS_WRITE_INTERNAL: write to XYOFFSET_1. Value: [%#08x]\n", value);
        } break;

        case 0x1A: 
        {
            gs.prmodecont.primitive_register = value & 0x1;
            printf("GS_WRITE_INTERNAL: write to PRMODECONT. Value: [%#08x]\n", value);
        } break;

        case 0x46:
        {
            gs.colclamp.clamp_method = value & 0x1;
            printf("GS_WRITE_INTERNAL: write to COLCLAMP. Value: [%#08x]\n", value);
        } break;

        case 0x40:
        {
            gs.scissor_1.min_x = value & 0x7ff;
            gs.scissor_1.max_x = (value >> 16) & 0x7FF;
            gs.scissor_1.min_y = (value >> 32) & 0x7FF;
            gs.scissor_1.max_y = (value >> 48) & 0x7FF;
            printf("GS_WRITE_INTERNAL: write to SCISSOR_1. Value: [%#08x]\n", value);
        } break;
        
        case 0x4C:
        {
            gs.frame_1.base_pointer     = value & 0x1FF;
            gs.frame_1.buffer_width     = (value >> 16) & 0x3F;
            gs.frame_1.storage_format   = (value >> 24) & 0x3F;
            gs.frame_1.drawing_mask     = (value >> 32);
            printf("GS_WRITE_INTERNAL: write to FRAME_1. Value: [%#08x]\n", value);
        } break;

        case 0x4E:
        {
            //gs.zbuf_1.base_pointer      = (value & 0x1FF) / 2048;
            gs.zbuf_1.base_pointer      = (value & 0x1FF);
            gs.zbuf_1.storage_format    = (value >> 24) & 0xF;
            gs.zbuf_1.z_value_mask      = value >> 32 & 0x1;
            printf("GS_WRITE_INTERNAL: write to ZBUF_1. Value: [%#08x]\n", value);
            return;
        } break;

        case 0x50: 
        {
            gs.bitbltbuf.src_base_pointer       = value & 0x7fff;  
            gs.bitbltbuf.src_buffer_width       = (value >> 16) & 0x7F;
            gs.bitbltbuf.src_storage_format     = (value >> 24) & 0x7F; 
            gs.bitbltbuf.dest_base_pointer      = (value >> 32) & 0x7FFF;
            gs.bitbltbuf.dest_buffer_width      = (value >> 48) & 0x7F;  
            gs.bitbltbuf.dest_storage_format    = (value >> 56) & 0x7F;
            printf("GS_WRITE_INTERNAL: write to BITBLTBUF. Value: [%#08x]\n", value);
        } break;

        case 0x51: 
        {
            gs.trxpos.src_x_coord             = value & 0x7FF;
            gs.trxpos.src_y_coord             = (value >> 16) & 0x7FF;
            gs.trxpos.dest_x_coord            = (value >> 32) & 0x7FF;
            gs.trxpos.dest_y_coord            = (value >> 48) & 0x7FF;
            gs.trxpos.transmission_direction  = (value >> 59) & 0x3;
            printf("GS_WRITE_INTERNAL: write to TRXPOS. Value: [%#08x]\n", value);
        } break;

        case 0x52: 
        {
            gs.trxreg.width = value & 0xFFF;
            gs.trxreg.height = (value >> 32) & 0xFFF;
            printf("GS_WRITE_INTERNAL: write to TRXREG. Value: [%#08x]\n", value);
        } break;

        case 0x53: 
        {
            gs.trxdir.direction = value & 0x3;
            printf("GS_WRITE_INTERNAL: write to TRXDIR. Value: [%#08x]\n", value);
        } break;

        // shut up
        case 0x0E: { return; } break;
        
        default:
        {
            printf("UNRECOGNIZED WRITE GS INTERNAL64: value: [%#09x], address[%#08x]\n", value, address);
            return;
        }
    }
    return;
}
