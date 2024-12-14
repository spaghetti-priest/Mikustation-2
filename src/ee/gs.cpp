/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include <iostream>
#include <vector>
#include "../include/ee/gs.h"
#include "../include/common.h"

alignas(16) GraphicsSynthesizer gs = {0};

static void vertex_kick();
static void drawing_kick();

static inline u32
pack_RGBA(u8 r, u8 b, u8 g, u8 a)
{
    u32 val;
    val |= r;
    val |= b << 8;
    val |= g << 16;
    val |= a << 24;
    return r;
}

enum Primitive_Types : u8
{
    POINT           = 0x0,  
    LINE            = 0x1,
    LINESTRIP       = 0x2,
    TRIANGLE        = 0x3, 
    TRIANGLESTRIP   = 0x4,
    TRIANGLEFAN     = 0x5,
    SPRITE          = 0x6,
};

enum Alpha_Test_Methods : u8
{
    NEVER       = 0x0,
    ALWAYS      = 0x1,
    LESS        = 0x2,
    LEQUAL      = 0x3,
    EQUAL       = 0x4,
    GEQUAL      = 0x5,
    GREATER     = 0x6,
    NOTEQUAL    = 0x7,
};

enum Alpha_Fail_Settings : u8
{
    KEEP        = 0x0,
    FB_ONLY     = 0x1,
    ZB_ONLY     = 0x2,
    RGB_ONLY    = 0x3,
};

enum Pixel_Storage_Mode : u8
{
    PSMCT32     = 0x0,
    PSMCT24     = 0x1,
    PSMCT16     = 0x2,
    PSMCT16S    = 0xA,

    PSMZ32      = 0x30,
    PSMZ24      = 0x31,
    PSMZ16      = 0x32,
    PSMZ16S     = 0x3A,
};

void 
gs_reset ()
{
    memset(&gs, 0, sizeof(gs));
    syslog("Resetting Graphics Synthesizer\n");
    gs.vram = (u16*)malloc(sizeof(u16) * MEGABYTES(4));
}

/*
========================
GIF PACKED FUNCTIONS
========================
*/
void
gs_set_q (f32 value)
{
    gs.rgbaq.q = value;
//    printf("GS SET Q\n");
}

void
gs_set_rgbaq (u8 r, u8 g, u8 b, u8 a)
{
    gs.rgbaq.r = r;
    gs.rgbaq.g = g;
    gs.rgbaq.b = b;
    gs.rgbaq.a = a;
    //printf("GS SET RGBAQ\n");
}

void 
gs_set_st (f32 s, f32 t)
{
    gs.st.s = s;
    gs.st.t = t;
    //printf("GS SET ST\n");
}

void 
gs_set_uv (u32 u, u32 v)
{
    gs.uv.u = u;
    gs.uv.v = v;
    //printf("GS SET UV\n");
}

void  
gs_set_fog (u8 fog)
{
    gs.fog.fog = fog;
}

void 
gs_set_xyzf2 (s16 x, s16 y, u32 z, u8 f)
{
    gs.xyzf2.x = x;
    gs.xyzf2.y = y;
    gs.xyzf2.z = z;
    gs.xyzf2.f = f;

    vertex_kick();
    drawing_kick();
    //printf("GS SET XYZF2\n");
}

void 
gs_set_xyzf3 (s16 x, s16 y, u32 z, u8 f)
{
    gs.xyzf3.x = x;
    gs.xyzf3.y = y;
    gs.xyzf3.z = z;
    gs.xyzf3.f = f;
    vertex_kick();
    //printf("GS SET XYZF3\n");
}

void 
gs_set_xyz2 (s16 x, s16 y, u32 z)
{
    gs.xyz2.x = x;
    gs.xyz2.y = y;
    gs.xyz2.z = z;

    vertex_kick();
    drawing_kick();
    //printf("GS SET XYZ2\n");
}

void 
gs_set_xyz3 (s16 x, s16 y, u32 z)
{
    gs.xyz3.x = x;
    gs.xyz3.y = y;
    gs.xyz3.z = z;
    vertex_kick();
    //printf("GS SET XYZ3\n");
}

void
gs_set_crt (bool interlaced, s32 display_mode, bool ffmd)
{
    gs.smode2.interlace_mode    = interlaced;
    gs.smode2.interlace_setting = ffmd; 
    switch (display_mode)
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

struct Vertex {
    XYZ pos;
    RGBAQ col;
    ST st;
    UV uv;
    f32 q;
    u8 f;
};

struct Vertex_Queue {
    //@@Remove: std is slow, but remove once it works
    std::vector<Vertex> queue;
    u32 size;
};

typedef struct _Vector3_ {
    s32 x, y, z;
} v3;


static Vertex_Queue vertex_queue = {
    .queue  = {},
    .size   = 0,
};

static inline bool 
scissoring_test (v3 pos)
{
    //@@Incomplete: Only using context 1 for rendering for now
    SCISSOR scissor     = gs.scissor_1;
    bool test_x_fail    = (pos.x < scissor.min_x || pos.x > scissor.max_x);
    bool test_y_fail    = (pos.y < scissor.min_y || pos.y > scissor.max_y);
    
    return (test_x_fail || test_y_fail);
}

/*
    GS Drawing Processs:
    1.) The host processor transmits vertex information (XYOFFSET, ST, UV..etc) and the drawing environemnt(context 1 or 2)
        to the GS
    2.)  
*/
static void draw_pixel (v3 pos, u32 color, u8 alpha) 
{
    bool write_fb_only = false, write_zb_only = false, write_rgb_only = false;

    /* Alpha Test */
    //@@Incomplete: Only using context 1 for rendering for now
    TEST test           = gs.test_1;
    u8 alpha_test_value = test.alpha_comparison_value;
    bool test_fail      = true;

    if (test.alpha_test) {
        switch(test.alpha_test_method)
        {
            case NEVER:  test_fail = true;  break;
            case ALWAYS: test_fail = false; break;

            case LESS:
            {
                if (alpha < alpha_test_value)
                    test_fail = false;
            } break;

            case LEQUAL:
            {
                if (alpha <= alpha_test_value)
                    test_fail = false;
            } break;

            case EQUAL:
            {
                if (alpha == alpha_test_value)
                    test_fail = false;
            } break;

            case GEQUAL:
            {
                if (alpha >= alpha_test_value)
                    test_fail = false;
            } break;

            case GREATER:
            {
                if (alpha > alpha_test_value)
                    test_fail = false;
            } break;

            case NOTEQUAL:
            {
                if (alpha != alpha_test_value)
                    test_fail = false;
            } break;
        }

        if (test_fail) {
            switch (test.alpha_fail_method)
            {
                case KEEP: {}                           break;
                case FB_ONLY:   write_fb_only = true;   break;
                case ZB_ONLY:   write_zb_only = true;   break;
                case RGB_ONLY:  write_rgb_only = true;  break;
            }
        }
    }

    /* Destination Alpha Test */
    u8 storage_format = gs.frame_1.storage_format;
    if (test.destination_test) {
        if (test.destination_test_mode == 1) {
            if      (storage_format == PSMCT32) { test_fail = (alpha & (1 << 7)) ? false : true; } 
            else if (storage_format == PSMCT16) { test_fail = (alpha == 1) ? false : false; } 
        } else {
            if      (storage_format == PSMCT32) { test_fail = (alpha & (0 << 7)) ? false : true; } 
            else if (storage_format == PSMCT16) { test_fail = (alpha == 0) ? false : false; } 
        }
    }

    // @@Note: Here we shift pos.x and pos.y by 4 bits to the right because the vertex position are floating point numbers
    // represented as unsigned numbers. THe intial 4 bits of the register are fractional 
    int index = gs.zbuf_1.base_pointer + ((pos.x >> 4) + (pos.y >> 4) * gs.frame_1.buffer_width);

    /* Depth Test */
    bool depth_test_fail = true;
    if (test.depth_test) {
        switch(test.depth_test_method)
        {
            case NEVER:
            {
                depth_test_fail = true;
            } break;
            case ALWAYS:
            {
                depth_test_fail = false;
            } break;
            case GEQUAL:
            {
                if (pos.z >= gs.vram[index])
                    depth_test_fail = false;
            } break;
            case GREATER:
            {
                if (pos.z > gs.vram[index])
                    depth_test_fail = false;
            } break;
        }

        if (depth_test_fail) gs.zbuf_1.z_value_mask = 0;
    }

    /* Alpha Blending */
    if (gs.prim.alpha_blending)
    {
    }

    /* Update whatever needs to be updated */
    if (write_fb_only) {
    } else if (write_rgb_only) {
    }

    if(write_zb_only) {}
}


static void render_point (std::vector<Vertex> vertices) 
{
    v3 pos;
    u32 color;

    //@@Incomplete: Only using context 1 for rendering for now
    XYOFFSET offset = gs.xyoffset_1;

    pos.x = vertices[0].pos.x - offset.x;
    pos.y = vertices[0].pos.y - offset.y;
    pos.z = vertices[0].pos.z;

    if(scissoring_test(pos))
        return;

    color = pack_RGBA(vertices[0].col.r, 
                      vertices[0].col.g, 
                      vertices[0].col.b, 
                      vertices[0].col.a);
    
    draw_pixel(pos, color, vertices[0].col.a);        
    //printf("Render Point\n");
}

#if 0
static void 
draw_line (Win32_Backbuffer *buffer, s32 x0, s32 x1, s32 y0, s32 y1, u32 color) 
{
    if (x0 < 0)                 x0 = 0;
    if (y0 < 0)                 y0 = 0;
    if (x1 > buffer->width)     x1 = buffer->width;
    if (y1 > buffer->height)    y1 = buffer->height;

    s32 dx      = std::abs(x1 - x0);
    s32 sx      = x0 < x1 ? 1 : -1;
    s32 dy      = -std::abs(y1 - y0);
    s32 sy      = y0 < y1 ? 1 : -1;
    s32 error   = dx + dy;
    while ( true ) {
        draw_pixel(buffer, x0, y0, color);
        if ( x0 == x1 && y0 == y1 ) break;
        int e2 = 2 * error;

        if ( e2 >= dy ) {
            if ( x0 == x1 ) break;
            error = error + dy;
            x0    = x0 + sx;
        }

        if ( e2 <= dx ) {
            if ( y0 == y1) break;
            error = error + dx;
            y0    = y0 + sy;
        }
    }
}
#endif

// @@Copypasta: From pepsiman_render.cpp
// Bressenhams drawing algorithm: https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
#include <stdlib.h>
static void render_line(std::vector<Vertex> vertices) 
{
    printf("Render line\n");

    v3 p[2];
    s32 x0, x1, y0, y1;
    s32 u0, u1, v0, v1;
    XYOFFSET offset = gs.xyoffset_1;
    
    x0  = int(vertices[0].pos.x - offset.x);
    y0  = int(vertices[0].pos.y - offset.y);

    x1  = int(vertices[1].pos.x - offset.x);
    y1  = int(vertices[1].pos.y - offset.y);

    p[0] = {x0, y0};
    p[1] = {x1, y1};
    if (scissoring_test(p[0]) || scissoring_test(p[1]))
        return;

    u32 color = pack_RGBA(vertices[0].col.r, 
                          vertices[0].col.g, 
                          vertices[0].col.b, 
                          vertices[0].col.a);

    s32 dx      = abs(x1 - x0 );
    s32 sx      = x0  < x1 ? 1 : -1;
    s32 dy      = -abs(y1 - y0);
    s32 sy      = y0 < y1 ? 1 : -1;
    s32 error   = dx + dy;

    v3 pos = { x0 , y0, (int)vertices[0].pos.z };
    while (true) 
    {
        draw_pixel (pos, color, 0);
        if (pos.x == x1 && pos.y == x1) break;
        int e2 = 2 * error;

        if (e2 >= dy) {
            if (pos.x == x1) break;
            error   = error + dy;
            pos.x  = pos.x + sx;
        }

        if (e2 <= dx) {
            if (pos.y == x1) break;
            error   = error + dx;
            pos.y  = pos.y + sy;
        }
    }
}

static void render_triangle (std::vector<Vertex> vertices) 
{
    printf("Drawing Kick!\n");

    // @@Incomplete: Only using context 1 for rendering for now
    v3 verts[3];
    u32 color;
    u32 depth = 0;
    XYOFFSET offset = gs.xyoffset_1;

    verts[0].x = vertices[0].pos.x - offset.x;
    verts[0].y = vertices[0].pos.y - offset.y;
    verts[0].z = vertices[0].pos.z;

    verts[1].x = vertices[1].pos.x - offset.x;
    verts[1].y = vertices[1].pos.y - offset.y;
    verts[1].z = vertices[1].pos.z;

    verts[2].x = vertices[2].pos.x - offset.x;
    verts[2].y = vertices[2].pos.y - offset.y;
    verts[2].z = vertices[2].pos.z;

    // @@Incomplete: No Support for triangle fans right now
    printf("Render Triangle\n");
}

static void render_sprite() 
{
    printf("Drawing Kick!\n");
    printf("Render Sprite\n");
}

static void
vertex_kick() 
{
    // @@Incomplete: XYZ3 and XYZF3 dont exist yet so XYZ2 for now.
    // Also assuming that XYZF and XYZ are the same.
    Vertex vertex = {
        .pos.x  = gs.xyz2.x,
        .pos.y  = gs.xyz2.y,
        .pos.z  = gs.xyz2.z,
        .col.r  = gs.rgbaq.r,
        .col.g  = gs.rgbaq.g,
        .col.b  = gs.rgbaq.b,
        .col.a  = gs.rgbaq.a,
        .st.s   = gs.st.s,
        .st.t   = gs.st.t,
        .uv.u   = gs.uv.u,
        .uv.v   = gs.uv.v,
        .q      = gs.rgbaq.q,
        .f      = gs.xyzf2.f,
    };

    vertex_queue.queue.push_back(vertex);
    vertex_queue.size += 1;
}

static void
drawing_kick()
{
    /*
        @@Incomplete: Check if the drawing attributes changed - PRMODE register
        and check the prim register for context change ex: using XOFFSET_1 or XOFFSET_2
    */
    u16 type = gs.prim.primative_type;
    /*
        @@Bug: The prim type sometimes switches during a drawing kick, creating a mismatch between the queue size 
        and the required vertex size for the prim type. This leads to a situations where the queue size exponentialy
        increases making it unable for any prim typ to render it
    */
    u16 queue_size = vertex_queue.size;
    switch(type)
    {
        case POINT:
        {
            if (queue_size == 1) {
                render_point(vertex_queue.queue);
                vertex_queue.queue.clear();
                vertex_queue.size = 0;
            }
        } break;

        case LINE:
        {
            if (queue_size == 2) {
                render_line(vertex_queue.queue);
                vertex_queue.queue.clear();
                vertex_queue.size = 0;
            }
        } break;

        case LINESTRIP:
        {
            //@@Hack: not sure if this fixes the previously mentioned bug 
            if (queue_size >= 3) {
                //render_point();
                vertex_queue.queue.clear();
                vertex_queue.size = 0;
                printf("Line Strip\n");

            }
        } break;

        case TRIANGLE:
        {
            if (queue_size == 3) {
                render_triangle(vertex_queue.queue);
                vertex_queue.queue.clear();
                vertex_queue.size = 0;
            }
        } break;

        case TRIANGLESTRIP:
        {
            //@@Hack: not sure if this fixes the previously mentioned bug 
            if (queue_size >= 3) {
                //render_point();
                vertex_queue.queue.clear();
                vertex_queue.size = 0;
                printf("Triangle Strip\n");

            }
        } break;

        case TRIANGLEFAN:
        {
            //@@Hack: not sure if this fixes the previously mentioned bug 
            if (queue_size >= 3) {
                //render_point();
                vertex_queue.queue.clear();
                vertex_queue.size = 0;
                printf("Traigle Fan\n");
            }
        } break;

        case SPRITE:
        {
            if (queue_size == 2) {
                render_sprite();
                vertex_queue.queue.clear();
                vertex_queue.size = 0;
            }
        } break;
    }    
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
            /* 
                @@Incomplete: @@Hack This is 0x8 in order to fire off the vsync interrupt
                im not sure if 3stars writes the interrupt flag in csr and so im
                just doing this hack in order to continued onwards since it loops forever
             */
            syslog("GS_READ32: read from CSR value\n");
            return 0x8;
            //return gs.csr.value;
        } break;
        default: 
        {
            errlog("ERROR: UNRECOGNIZED READ GS PRIVILEDGE32: address [{:#x}]\n", address);
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
            /* 
                @@Incomplete: @@Hack This is 0x8 in order to fire off the vsync interrupt
                im not sure if 3stars writes the interrupt flag in csr and so im
                just doing this hack in order to continued onwards since it loops forever
            */
            syslog("GS_READ: read from CSR\n");
            return 0x8;
            //return gs.csr.value;
        } break;
        default: 
        {
             errlog("ERROR: UNRECOGNIZED READ GS PRIVILEDGE32: address [{:#x}]\n", address);
             return 0;
        } break;

    }
    return 0;
}

void 
gs_write_32_priviledged (u32 address, u32 value)
{
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
            syslog("GS_WRITE32: write to CSR. Value: [{:#x}]\n", value);
            if (value & 0x8) {//(gs.csr.v_interrupt) {
                v_sync_interrupt = true;
                v_sync_generated = false;
            }
            return;
        } break;

        case 0x12000070:
        {
            syslog("GS_WRITE_PRIVILEDGED: write to DISPFB1. Value: [{:#x}]\n", value);
            gs.dispfb1.base_pointer     = (value & 0x1FF) * 2048;
            gs.dispfb1.buffer_width     = ((value >> 9) & 0x2F) * 48;
            gs.dispfb1.storage_formats  = (value >> 15) & 0x1F;
            return;
        } break;
        

        default:
        {
            errlog("ERROR: UNRECOGNIZED WRITE GS PRIVILEDGE32: address [{:#x}]\n", address);
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
            syslog("GS_WRITE_PRIVILEDGED: write to PMODE. Value: [{:#08x}]\n", value);
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
            syslog("GS_WRITE_PRIVILEDGED: write to SMODE1. Value: [{:#x}]\n", value);
            gs.smode1.value                 = value;
            return;
        } break;

        case 0x12000020:
        {
            syslog("GS_WRITE_PRIVILEDGED: write to SMODE2. Value: [{:#x}]\n", value);
            gs.smode2.value                 = value;
            gs.smode2.interlace_mode        = value & 0x1;
            gs.smode2.interlace_setting     = (value >> 1) & 0x1;
            gs.smode2.mode                  = (value >> 2) & 0x3;
            return;
        } break;

        case 0x12000030:
        {
            syslog("GS_WRITE_PRIVILEDGED: write to SRFSH. Value: [{:#x}]\n", value);
            gs.srfsh.value = value;
            return;
        } break;

        case 0x12000040:
        {
            syslog("GS_WRITE_PRIVILEDGED: write to SYNCH1. Value: [{:#x}]\n", value);
            gs.synch1.value = value;
            return;
        } break;

        case 0x12000050:
        {
            syslog("GS_WRITE_PRIVILEDGED: write to SYNCH2. Value: [{:#x}]\n", value);
            gs.synch2.value = value;
            return;
        } break;

        case 0x12000060:
        {
            syslog("GS_WRITE_PRIVILEDGED: write to SYNCV. Value: [{:#x}]\n", value);
            gs.syncv.value = value;
            return;
        } break;
        
        case 0x12000070:
        {
            syslog("GS_WRITE_PRIVILEDGED: write to DISPFB1. Value: [{:#x}]\n", value);
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
            syslog("GS_WRITE_PRIVILEDGED: write to DISPLAY1. Value: [{:#x}]\n", value);
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
            syslog("GS_WRITE_PRIVILEDGED: write to DISPFB2. Value: [{:#x}]\n", value);
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
            syslog("GS_WRITE_PRIVILEDGED: write to DISPLAY2. Value: [{:#x}]\n", value);
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
            syslog("GS_WRITE_PRIVILEDGED: write to EXTBUF. Value: [{:#x}]\n", value);
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
            syslog("GS_WRITE_PRIVILEDGED: write to EXTDATA. Value: [{:#x}]\n", value);
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
            syslog("GS_WRITE_PRIVILEDGED: write to EXTWRITE. Value: [{:#x}]\n", value);
            gs.extwrite.activated = value & 0x1;
            return;
        } break;
        
        case 0x120000E0:
        {
            syslog("GS_WRITE_PRIVILEDGED: write to BGCOLOR. Value: [{:#x}]\n", value);
            gs.bgcolor.red      = value & 0xFF;
            gs.bgcolor.green    = (value >> 8) & 0xFF;
            gs.bgcolor.blue     = (value >> 16) & 0xFF;
            return;
        } break;
        
        case 0x12001000:
        {
            #if 0
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
            syslog("GS_WRITE_PRIVILEDGED: write to CSR. Value: [{:#x}]\n", value);
            if (value & 0x8) {
                v_sync_interrupt = true;
                v_sync_generated = false;
            }
            return;
        } break;
        
        case 0x12001010:
        {
            syslog("GS_WRITE_PRIVILEDGED: write IMR. Value: [{:#x}]\n", value);
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
            syslog("GS_WRITE_PRIVILEDGED: write to BUSDIR. Value: [{:#x}]\n", value);
            gs.busdir.direction = value & 0x1;
            return;
        } break;
        
        case 0x12001080:
        {
            syslog("GS_WRITE_PRIVILEDGED: write to SIGLBLID. Value: [{:#x}]\n", value);
            gs.siglbid.value        = value;
            gs.siglbid.signal_id    = value & 0xFFFF;
            gs.siglbid.label_id     = (value >> 32) & 0xFFFF;
            return;
        } break;
        
        default: 
        {
            errlog("ERROR: UNRECOGNIZED WRITE GS PRIVILEDGE64: value: [{:#x}], address[{:#08x}]\n", value, address);
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

void 
gs_write_64_internal (u8 address, u64 value) 
{
    switch(address)
    {
        case 0x00:
        {
            gs_set_primitive(value);
            syslog("GS_WRITE_INTERNAL: write to PRIM. Value: [{:#04x}]\n", value);
        } break;

        case 0x01:
        {
            gs.rgbaq.r = value & 0xFF;
            gs.rgbaq.g = (value >> 8) & 0xFF;
            gs.rgbaq.b = (value >> 16) & 0xFF;
            gs.rgbaq.a = (value >> 24) & 0xFF;
            gs.rgbaq.q = (value >> 32);
            syslog("GS_WRITE_INTERNAL: write to RGBAQ. Value: [{:#x}]\n", value);
        } break;

        case 0x02:
        {
            /* Mostly compliant with iEEE 754 */
            gs.st.s = value & 0xFFFFFFFF;
            gs.st.t = (value >> 32);
            syslog("GS_WRITE_INTERNAL: write to ST. Value: [{:#x}]\n", value);
        } break;
        
        case 0x03:
        {
            gs.uv.u = (value >> 14) & 0x3FFFF;
            gs.uv.v = (value >> 32) & 0x3FFFF;
            syslog("GS_WRITE_INTERNAL: write to UV. Value: [{:#x}]\n", value);
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
            vertex_kick();
            drawing_kick();
            syslog("GS_WRITE_INTERNAL: write to XYZF2. Value: [{:#x}]\n", value);
        } break;

        case 0x05:
        {
            gs.xyz2.x = (value) & 0xFFFF;
            gs.xyz2.y = (value >> 16) & 0xFFFF;
            gs.xyz2.z = (value >> 32);
            
            XYZ xyz2 = {};
            xyz2.x = (value) & 0xFFFF;
            xyz2.y = (value >> 16) & 0xFFFF;
            xyz2.z = (value >> 32);
            gs.context[1].xyz = xyz2;

            vertex_kick();
            drawing_kick();
            syslog("GS_WRITE_INTERNAL: write to XYZ2. Value: [{:#x}]\n", value);
        } break;

        case 0x18:
        {
            XYOFFSET xyoffset = {};
            xyoffset.x = (value) & 0xFFFF;
            xyoffset.y = (value >> 32) & 0xFFFF;
            gs.context[1].xyoffset = xyoffset;

            gs.xyoffset_1.x = (value) & 0xFFFF;
            gs.xyoffset_1.y = (value >> 32) & 0xFFFF;
            syslog("GS_WRITE_INTERNAL: write to XYOFFSET_1. Value: [{:#x}]\n", value);
        } break;

        case 0x1A: 
        {
            gs.prmodecont.primitive_register = value & 0x1;
            syslog("GS_WRITE_INTERNAL: write to PRMODECONT. Value: [{:#x}]\n", value);
        } break;

        case 0x40:
        {
            gs.scissor_1.min_x = value & 0x7ff;
            gs.scissor_1.max_x = (value >> 16) & 0x7FF;
            gs.scissor_1.min_y = (value >> 32) & 0x7FF;
            gs.scissor_1.max_y = (value >> 48) & 0x7FF;
            syslog("GS_WRITE_INTERNAL: write to SCISSOR_1. Value: [{:#x}]\n", value);
        } break;
        
        case 0x46:
        {
            gs.colclamp.clamp_method = value & 0x1;
            syslog("GS_WRITE_INTERNAL: write to COLCLAMP. Value: [{:#x}]\n", value);
        } break;

        case 0x47:
        {
            gs.test_1.alpha_test                = (value >> 0) & 0x1;
            gs.test_1.alpha_test_method         = (value >> 1) & 0x7;
            gs.test_1.alpha_comparison_value    = (value >> 4) & 0xFF;
            gs.test_1.alpha_fail_method         = (value >> 11) & 0x3;
            gs.test_1.destination_test          = (value >> 14) & 0x1;
            gs.test_1.destination_test_mode     = (value >> 15) & 0x1;
            gs.test_1.depth_test                = (value >> 16) & 0x1;
            gs.test_1.depth_test_method         = (value >> 17) & 0x3;
            gs.test_1.value                     = value;
            
            syslog("GS_WRITE_INTERNAL: write to TEST_1. Value: [{:#x}]\n", value);
        } break;

        case 0x4C:
        {
            gs.frame_1.base_pointer     = (value & 0x1FF) * 2048;
            gs.frame_1.buffer_width     = ((value >> 16) & 0x3F) * 64;
            gs.frame_1.storage_format   = (value >> 24) & 0x3F;
            gs.frame_1.drawing_mask     = (value >> 32);
            syslog("GS_WRITE_INTERNAL: write to FRAME_1. Value: [{:#x}]\n", value);
        } break;

        case 0x4E:
        {
            gs.zbuf_1.base_pointer      = (value & 0x1FF) * 2048;
            gs.zbuf_1.storage_format    = (value >> 24) & 0xF;
            gs.zbuf_1.z_value_mask      = value >> 32 & 0x1;
            syslog("GS_WRITE_INTERNAL: write to ZBUF_1. Value: [{:#x}]\n", value);
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
            syslog("GS_WRITE_INTERNAL: write to BITBLTBUF. Value: [{:#x}]\n", value);
        } break;

        case 0x51: 
        {
            gs.trxpos.src_x_coord             = value & 0x7FF;
            gs.trxpos.src_y_coord             = (value >> 16) & 0x7FF;
            gs.trxpos.dest_x_coord            = (value >> 32) & 0x7FF;
            gs.trxpos.dest_y_coord            = (value >> 48) & 0x7FF;
            gs.trxpos.transmission_direction  = (value >> 59) & 0x3;
            syslog("GS_WRITE_INTERNAL: write to TRXPOS. Value: [{:#x}]\n", value);
        } break;

        case 0x52: 
        {
            gs.trxreg.width = value & 0xFFF;
            gs.trxreg.height = (value >> 32) & 0xFFF;
            syslog("GS_WRITE_INTERNAL: write to TRXREG. Value: [{:#x}]\n", value);
        } break;

        case 0x53: 
        {
            gs.trxdir.direction = value & 0x3;
            syslog("GS_WRITE_INTERNAL: write to TRXDIR. Value: [{:#x}]\n", value);
        } break;

        // shut up
        case 0x0E: { return; } break;
        
        default:
        {
            errlog("UNRECOGNIZED WRITE GS INTERNAL64: value: [{:#x}], address[{:#x}]\n", value, address);
            return;
        }
    }
    return;
}
