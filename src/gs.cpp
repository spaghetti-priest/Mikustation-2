/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include <iostream>
#include "../include/gs.h"

alignas(16) GraphicsSynthesizer gs = {0};

//gs_initialize_q (GraphicsSynthesizer *gs)
void
gs_set_q (f32 value)
{
    gs.rgbaq.q = value;
}

//gs_set_rgbaq (GraphicsSynthesizer *gs, u32 value)
void
gs_set_rgbaq (u32 r, u32 g, u32 b, u32 a)
{
    gs.rgbaq.r = r;
    gs.rgbaq.g = g;
    gs.rgbaq.b = b;
    gs.rgbaq.a = a;
}

void 
gs_set_st (f32 s, f32 t)
{
    gs.st.s = s;
    gs.st.t = t;
}

void 
gs_set_uv (u32 u, u32 v)
{
    gs.uv.u = u;
    gs.uv.v = v;
}

void  
gs_set_fog (u8 fog)
{
    gs.fog.fog = fog;
}

void 
gs_set_xyzf2(s16 x, s16 y, u32 z, u8 f)
{
    gs.xyzf2.x = x;
    gs.xyzf2.y = y;
    gs.xyzf2.z = z;
    gs.xyzf2.f = f;

    // @Incomplete:
    // Add Drawing Vertex Kick
}

void 
gs_set_xyzf3(s16 x, s16 y, u32 z, u8 f)
{
    gs.xyzf3.x = x;
    gs.xyzf3.y = y;
    gs.xyzf3.z = z;
    gs.xyzf3.f = f;
}

void 
gs_set_xyz2(s16 x, s16 y, u32 z)
{
    gs.xyz2.x = x;
    gs.xyz2.y = y;
    gs.xyz2.z = z;

    // @Incomplete:
    // Add Drawing Vertex Kick
}

void 
gs_set_xyz3(s16 x, s16 y, u32 z)
{
    gs.xyz3.x = x;
    gs.xyz3.y = y;
    gs.xyz3.z = z;
}

void 
gs_reset ()
{
	printf("Resetting Graphics Synthesizer\n");
	gs.vram = (u16*)malloc(sizeof(u16) * MEGABYTES(4));
}

u32 
gs_read_32_priviledged (u32 address) 
{
    printf("READ PRIVILEDGE32: Graphics synthesizer addr[%#x]\n", address);
    return 0;
}

u64 
gs_read_64_priviledged (u32 address) 
{
    printf("READ PRIVILEDGE64: Graphics synthesizer addr[%#x]\n", address);
    return 0;
}

void 
gs_write_32_priviledged (u32 address, u32 value)
{
    printf("WRITE PRIVILEDGE32: Graphics synthesizer value: [%#09x], addr[%#x]\n", value, address);

	// Clip upper 32 bits of the address
    switch(address)
    {
        default:
        {
            return;
        } break;
    }
}

void 
gs_write_64_priviledged (u32 address, u64 value)
{
   // printf("WRITE PRIVILEDGE64: Graphics synthesizer value: [%#09x], addr[%#x]\n", value, address);

    switch(address)
    {
        case 0x12000000:
        {
            printf("GS_WRITE: write to PMODE. Value: [%#08x]\n", value);
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
        
        case 0x12000020:
        {
            printf("GS_WRITE: write to SMODE2. Value: [%#08x]\n", value);
            gs.smode2.value             = value;
            gs.smode2.interlace_mode    = value & 0x1;
            gs.smode2.interlace_setting = (value >> 1) & 0x1;
            gs.smode2.mode               = (value >> 2) & 0x3;
            return;
        } break;
        
        case 0x12000070:
        {
            printf("GS_WRITE: write to DISPFB1. Value: [%#08x]\n", value);
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
            printf("GS_WRITE: write to DISPLAY1. Value: [%#08x]\n", value);
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
            printf("GS_WRITE: write to DISPFB2. Value: [%#08x]\n", value);
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
            printf("GS_WRITE: write to DISPLAY2. Value: [%#08x]\n", value);
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
            printf("GS_WRITE: write to EXTBUF. Value: [%#08x]\n", value);
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
            printf("GS_WRITE: write to EXTDATA. Value: [%#08x]\n", value);
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
            printf("GS_WRITE: write to EXTWRITE. Value: [%#08x]\n", value);
            gs.extwrite.activated = value & 0x1;
            return;
        } break;
        
        case 0x120000E0:
        {
            printf("GS_WRITE: write to BGCOLOR. Value: [%#08x]\n", value);
            gs.bgcolor.red      = value & 0xFF;
            gs.bgcolor.green    = (value >> 8) & 0xFF;
            gs.bgcolor.blue     = (value >> 16) & 0xFF;
            return;
        } break;
        
        case 0x12001000:
        {
            printf("GS_WRITE: write to CSR. Value: [%#08x]\n", value);
            gs.csr.value                    = value;
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
            return;
        } break;
        
        case 0x12001010:
        {
            printf("GS_WRITE: write IMR. Value: [%#08x]\n", value);
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
            printf("GS_WRITE: write to BUSDIR. Value: [%#08x]\n", value);
            gs.busdir.direction = value & 0x1;
            return;
        } break;
        
        case 0x12001080:
        {
            printf("GS_WRITE: write to SIGLBLID. Value: [%#08x]\n", value);
            gs.siglbid.value        = value;
            gs.siglbid.signal_id    = value & 0xFFFF;
            gs.siglbid.label_id     = (value >> 32) & 0xFFFF;
            return;
        } break;
        
       }
    return;
}

void 
gs_set_primitive (u16 value) 
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
    gs.prim.value                   = (u64)value;
}

void 
gs_write_64_internal (u32 address, u64 value) 
{
    switch(address)
    {
        case 0x00:
        {
            gs_set_primitive(value);
            return;
        } break;

        default:
        {
            return;
        }
    }
    return;
}
