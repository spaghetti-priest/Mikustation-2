/*
 * Copyright 2023 Xaviar Roach
 * SPDX-License-Identifier: MIT
 */

#include <iostream>
#include <vector>
#include "assert.h"

#include "../include/ee/gs.h"
#include "../include/common.h"
#include "../include/ee/intc.h"

alignas(16) GraphicsSynthesizer gs = {0};

static void vertex_kick(bool with_fog);
static void drawing_kick();

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a > b ? a : b)
#define MIN3(a, b, c) (MIN(MIN(a,b), c))
#define MAX3(a, b, c) (MAX(MAX(a,b), c))

static inline u32
pack_RGBA(u8 r, u8 b, u8 g, u8 a)
{
    u32 val = 0;
    val |= r;
    val |= b << 8;
    val |= g << 16;
    val |= a << 24;
    return val;
}

enum Primitive_Types : u8
{
    _POINT           = 0x0,  
    _LINE            = 0x1,
    _LINESTRIP       = 0x2,
    _TRIANGLE        = 0x3, 
    _TRIANGLESTRIP   = 0x4,
    _TRIANGLEFAN     = 0x5,
    _SPRITE          = 0x6,
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
    gs.vram = (u32*)malloc(sizeof(u32) * MEGABYTES(4));
    memset(gs.vram, 0, sizeof(u32) * MEGABYTES(4));
}

void 
gs_shutdown()
{
    free(gs.vram);
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
}

void gs_set_psm(u8 psm) {}

u32
get_output_resolution (s32 mode)
{
    switch(mode)
    {
        case CRT_MODE_NTSC:

        break;

        case CRT_MODE_PAL:
        
        break;

        case CRT_MODE_DTV_480P:

        break;
    }
    return 0;
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
gs_render_crt (SDL_Context *context) 
{
    PMODE *pmode        = &gs.pmode;
    DISPLAY *display1   = &gs.display1;
    DISPLAY *display2   = &gs.display2;
    DISPFB *dispfb1     = &gs.dispfb1;
    DISPFB *dispfb2     = &gs.dispfb2;
    
    // @Hack
    u16 framebuffer_width = ((dispfb2->value >> 9) & 0x3F) * 64; 

    // @Incomplete: Check which of the circuits are enabled then use the respective display buffer
    // But this is a low priority right now
    // if (pmode->is_circuit2) 

    u32 x_magh = display2->width / (display2->h_magnification + 1);
    u32 y_magv = display2->height / (display2->v_magnification + 1);
    // u32 y = dispfb2->y_position;
    // u32 x = dispfb2->x_position;

    for (u32 y = dispfb2->y_position; y < y_magv; ++y) {
        for (u32 x = dispfb2->x_position; x < x_magh; ++x) {
            // @Copypaste: Based off of Dobiestation output_crt just to get it working
            u32 frame_x = x;
            u32 frame_y = y;
            frame_x *= framebuffer_width;
            frame_x /= x_magh;
            context->backbuffer->pixels[x + (y * x_magh)] = gs.vram[dispfb2->base_pointer + frame_x + (frame_y * framebuffer_width)];
            // @Hack: Temporarily assume that MMOD and AMOD are true and this is the final output value
            context->backbuffer->pixels[x + (y * x_magh)] |= 0xFF000000;
        }
    }
}

void
gs_host_to_host_transmission (u64 data) {}

void
gs_write_hwreg (u64 data)
{
    TRXDIR *trxdir              = &gs.trxdir;
    TRXPOS *trxpos              = &gs.trxpos;
    TRXREG *trxreg              = &gs.trxreg;
    BITBLTBUF *bitbltbuf        = &gs.bitbltbuf;
    Transmission_Buffer *buffer = &gs.transmission_buffer;
    u32 max_pixels              = trxreg->width * trxreg->height;

    // assert(trxdir->direction == 0x00 && "Illegal write to HWREG register when trxdir is not 0x0");
    if (trxdir->direction != 0x0)
        return;

    // BPP is 4 for PSMCT32 but because of hwreg being written twice its actually 2
    u32 bytes_pp    = 2;
    u32 x           = trxpos->dest_x_coord + buffer->row;
    u32 y           = trxpos->dest_y_coord + buffer->pitch; 

    // @Incomplete: Only PSMCT32 and its calculation is represented here     
    {
        buffer->address             = (bitbltbuf->dest_base_pointer + ((x + 0) + (bitbltbuf->dest_buffer_width * y)));
        gs.vram[buffer->address]    = data;
        buffer->address             = (bitbltbuf->dest_base_pointer + ((x + 1) + (bitbltbuf->dest_buffer_width * y)));
        gs.vram[buffer->address]    = data >> 32;

        buffer->row                 += bytes_pp;
        buffer->pixel_count         += bytes_pp;
    }
    
    if (buffer->row >= trxreg->width) {
        buffer->pitch  += 1;
        buffer->row     = 0;
    }
  
    if (buffer->pixel_count == max_pixels) {
        buffer->pitch       = 0;
        buffer->row         = 0;
        buffer->pixel_count = 0;
        buffer->address     = 0;
        gs.trxdir.direction = 3;
        
        syslog("Ending: Host => Local Transmission\n");
        return;
    }
}

void gs_select_transmission_mode() {}

struct Vertex {
    XYZ pos;
    RGBAQ col;
    ST st;
    UV uv;
    f32 q;
    u8 f;
};

struct Vertex_Queue {
    // @Remove: std is slow, but remove once it works
    std::vector<Vertex> queue;
    u32 size;
};

typedef struct _Vector3_ {
    s32 x, y, z;
} v3;

typedef union _Vector2_ {
    struct {
        s32 x, y;
    };
    struct {
        s32 u, v;
    };
} v2;

// @Remove: Unnecessary Global Variable
static Vertex_Queue vertex_queue = {
    .queue  = {},
    .size   = 0,
};

static inline bool 
scissor_test_fail (v3 *pos)
{
    // @Incomplete: Only using context 1 for rendering for now
    SCISSOR *scissor    = &gs.scissor_1;
    bool test_x_fail    = (pos->x < scissor->min_x || pos->x > scissor->max_x);
    bool test_y_fail    = (pos->y < scissor->min_y || pos->y > scissor->max_y);
    
    return (test_x_fail || test_y_fail);
}

/*
    GS Drawing Processs:
    1.) The host processor transmits vertex information (XYOFFSET, ST, UV..etc) and the drawing environemnt(context 1 or 2)
        to the GS
    2.)  
*/
static void draw_pixel (v3 *pos, u32 color) 
{
    //@@Incomplete: Only using context 1 for rendering for now
    TEST *test          = &gs.test_1;
    FRAME *frame_1      = &gs.frame_1;
    ZBUF *zbuf_1        = &gs.zbuf_1; 

    // When a pixel fails a test, they are controlled by the GS in drawing 
    // (meaning they remain unchanged during buffer write)
    bool control_zb  = gs.zbuf_1.z_value_mask == 1; 
    bool control_rgb = false;
    bool control_a   = false; 

    u8 alpha            = (color >> 24) & 0xFF;
    u8 alpha_test_value = test->alpha_comparison_value;
    //bool test_fail   = true;
    bool test_succeded  = true;

    if (test->alpha_test) {
        switch(test->alpha_test_method)
        {
            case NEVER:     test_succeded = false;                      break;
            case ALWAYS:    test_succeded = true;                       break;
            case LESS:      test_succeded = alpha < alpha_test_value;   break;
            case LEQUAL:    test_succeded = alpha <= alpha_test_value;  break;
            case EQUAL:     test_succeded = alpha == alpha_test_value;  break;
            case GEQUAL:    test_succeded = alpha >= alpha_test_value;  break;
            case GREATER:   test_succeded = alpha > alpha_test_value;   break;
            case NOTEQUAL:  test_succeded = alpha != alpha_test_value;  break;
        }

        if (!test_succeded) {
            switch (test->alpha_fail_method)
            {
                case KEEP: 
                    control_zb  = true;
                    control_rgb = true;
                    control_a   = true;
                break;
                
                case FB_ONLY:   
                    control_zb  = true;  
                break;
                
                case ZB_ONLY:   
                    control_rgb = true;  
                    control_a   = true;  
                break;
                
                case RGB_ONLY:  
                    control_a   = true;  
                    control_zb  = true;  
                break;
            }
        }
    }

    u8 storage_format   = frame_1->storage_format;
    u8 i                = test->destination_test_mode == 1 ? 1 : 0;
    
    if (test->destination_test) {
        if (storage_format == PSMCT32) {
            test_succeded = (alpha & (i << 7));
        } else if (storage_format == PSMCT16) {
            test_succeded = alpha == i;
        }
    }

    // Here we shift pos.x and pos.y by 4 bits to the right because the vertex position are floating point numbers
    // represented as unsigned numbers. THe intial 4 bits of the register are fractional 
    int index       = ((pos->x >> 4) + (pos->y >> 4) * (frame_1->buffer_width * 64));
    //int index      = (pos->x  + pos->y) * (frame_1->buffer_width * 64);
    int zpointer    = zbuf_1->base_pointer + index;

    bool depth_test_success = true;
    if (test->depth_test) {
        switch(test->depth_test_method)
        {
            case NEVER:
                depth_test_success = false;
            break;

            case ALWAYS:
                depth_test_success = true;
            break;

            case GEQUAL:
                if (pos->z >= gs.vram[zpointer])
                    depth_test_success = true;
            break;

            case GREATER:
                if (pos->z > gs.vram[zpointer])
                    depth_test_success = true;
            break;
        }
    }

    // @Incomplete: No alpha blending
    if (gs.prim.do_alpha_blending) {}

    u8 buffer_alpha = gs.vram[frame_1->base_pointer + index] >> 24;
    
    if (!control_zb) {
        gs.vram[frame_1->base_pointer + index] = pos->z;
    } 

    if (!control_rgb) {
        if (!control_a) {
            // @Incomplete: there is no alpha blending yet so not writes to alpha here
        } 
        gs.vram[frame_1->base_pointer + index] = color;
    }
}


static void render_point (Vertex *vertex) 
{
    v3 pos;
    u32 color;

    //@@Incomplete: Only using context 1 for rendering for now
    XYOFFSET *offset = &gs.xyoffset_1;

    pos.x = vertex->pos.x - offset->x;
    pos.y = vertex->pos.y - offset->y;
    pos.z = vertex->pos.z;

    color = pack_RGBA(vertex->col.r, vertex->col.g, vertex->col.b, vertex->col.a);
    if(scissor_test_fail(&pos))
        return;
    
    draw_pixel(&pos, color);        
    // printf("Render Point\n");
}

// @Incomplete: Change to DDA drawing algorithm 
// @Copypasta: From pepsiman_render.cpp
// Bressenhams drawing algorithm: https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
#include <stdlib.h>
static void render_line (std::vector<Vertex> vertices) 
{
    v3 p[2];
    s32 x0, x1, y0, y1;
    s32 u0, u1, v0, v1;
    XYOFFSET *offset = &gs.xyoffset_1;
    
    x0  = int(vertices[0].pos.x - offset->x);
    y0  = int(vertices[0].pos.y - offset->y);

    x1  = int(vertices[1].pos.x - offset->x);
    y1  = int(vertices[1].pos.y - offset->y);

    p[0] = {x0, y0};
    p[1] = {x1, y1};

    if (scissor_test_fail(&p[0]) || scissor_test_fail(&p[1]))
        return;

    u32 color = pack_RGBA(vertices[0].col.r, 
                          vertices[0].col.g, 
                          vertices[0].col.b, 
                          vertices[0].col.a);

    s32 dx      = abs(x1 - x0);
    s32 sx      = x0  < x1 ? 1 : -1;
    s32 dy      = -abs(y1 - y0);
    s32 sy      = y0 < y1 ? 1 : -1;
    s32 error   = dx + dy;

    v3 pos = { x0 , y0, (int)vertices[0].pos.z };
    printf("Render line\n");
    while (true) 
    {
        draw_pixel (&pos, color);
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
    // @@Incomplete: No Support for triangle fans right now
    v3 verts[3];
    u32 color;
    u32 depth = 0;
    XYOFFSET *offset = &gs.xyoffset_1;

    verts[0].x = vertices[0].pos.x - offset->x;
    verts[0].y = vertices[0].pos.y - offset->y;
    verts[0].z = vertices[0].pos.z;

    verts[1].x = vertices[1].pos.x - offset->x;
    verts[1].y = vertices[1].pos.y - offset->y;
    verts[1].z = vertices[1].pos.z;

    verts[2].x = vertices[2].pos.x - offset->x;
    verts[2].y = vertices[2].pos.y - offset->y;
    verts[2].z = vertices[2].pos.z;

    // Do we do the scissoring test before we get the bounding box coordinates?
    if (scissor_test_fail(&verts[0]) || 
        scissor_test_fail(&verts[1]) ||
        scissor_test_fail(&verts[2]))
        return;

    u32 minx = MIN3(verts[0].x, verts[1].x, verts[2].x);
    u32 miny = MIN3(verts[0].y, verts[1].y, verts[2].y);
    u32 maxx = MAX3(verts[0].x, verts[1].x, verts[2].x);
    u32 maxy = MAX3(verts[0].y, verts[1].y, verts[2].y);

    printf("Render Triangle\n");
}

// @Copypaste: from pepsiman_renderer.cpp
inline bool 
is_top_left (v2 edge, float w) 
{
    bool overlaps = (w == 0 ? ((edge.y == 0 && edge.x > 0) ||  edge.y > 0) : (w > 0));
    return overlaps;
}

// @Copypaste: from pepsiman_render.cpp
static float 
orient_2d (v3 a, v3 b, v3 c) 
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static void 
render_sprite (std::vector<Vertex> vertices)
{
    //printf("Drawing Kick!\n");
    // @Incomplete: Texture mapping has not been implemented yet
    v3 pos[2]; 
    v2 uv[2];
    u32 color;
    u32 depth = 0;   
    XYOFFSET *offset = &gs.xyoffset_1;

    pos[0].x = vertices[0].pos.x - offset->x;
    pos[0].y = vertices[0].pos.y - offset->y;
    pos[0].z = vertices[0].pos.z;
    
    pos[1].x = vertices[1].pos.x - offset->x;
    pos[1].y = vertices[1].pos.y - offset->y;
    pos[1].z = vertices[1].pos.z;

    uv[0].u = vertices[0].uv.u;
    uv[0].v = vertices[0].uv.v;

    uv[1].u = vertices[1].uv.u;
    uv[1].v = vertices[1].uv.v;


    // @Incomplete: Assuming that psm is PSMCT32
    color = pack_RGBA(vertices[0].col.r, 
                      vertices[0].col.g, 
                      vertices[0].col.b, 
                      vertices[0].col.a);

    // @Hack: Doing this just so I can get things working
    pos[0].x = pos[0].x >> 4;
    pos[0].y = pos[0].y >> 4;
    pos[1].x = (pos[1].x - 1) >> 4;
    pos[1].y = (pos[1].y - 1) >> 4;

    // Do we do the scissoring test before we get the bounding box coordinates?
    if (scissor_test_fail(&pos[0]) || scissor_test_fail(&pos[1]))
        return;

    // Get the bounding box coordinates
    u32 minx = MIN(pos[0].x, pos[1].x);
    u32 miny = MIN(pos[0].y, pos[1].y);
    u32 maxx = MAX(pos[0].x, pos[1].x);
    u32 maxy = MAX(pos[0].y, pos[1].y);

    // @Incomplete: Check if pixels are top left 
    // is_top_left();

    v3 p; 
    p.z = pos[0].z; 
    for (p.y = miny; p.y < maxy; p.y++) {
        for (p.x = minx; p.x < maxx; p.x++) {
            draw_pixel(&p, color);
        }
    }

    printf("Render Sprite\n");
}

static void
vertex_kick (bool with_fog) 
{
    // printf("Push Vertex\n");
    // @@Incomplete: XYZ3 and XYZF3 dont exist yet so XYZ2 for now.
    // Also assuming that XYZF and XYZ are the same.
/*    Vertex vertex = {
        .pos.x  = 0,
        .pos.y  = 0,
        .pos.z  = 0,
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
    };*/

   Vertex vertex = {0};
    vertex.pos.x  = 0;
    vertex.pos.y  = 0;
    vertex.pos.z  = 0;
    vertex.col.r  = gs.rgbaq.r;
    vertex.col.g  = gs.rgbaq.g;
    vertex.col.b  = gs.rgbaq.b;
    vertex.col.a  = gs.rgbaq.a;
    vertex.st.s   = gs.st.s;
    vertex.st.t   = gs.st.t;
    vertex.uv.u   = gs.uv.u;
    vertex.uv.v   = gs.uv.v;
    vertex.q      = gs.rgbaq.q;
    vertex.f      = gs.xyzf2.f;

    if (with_fog) {
        vertex.pos.x  = gs.xyzf2.x;
        vertex.pos.y  = gs.xyzf2.y;
        vertex.pos.z  = gs.xyzf2.z;
    } else {
        vertex.pos.x  = gs.xyz2.x;
        vertex.pos.y  = gs.xyz2.y;
        vertex.pos.z  = gs.xyz2.z;
    }

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
    u16 type = gs.prim.primitive_type;
    /*
        @@Bug: The prim type sometimes switches during a drawing kick, creating a mismatch between the queue size 
        and the required vertex size for the prim type. This leads to a situations where the queue size exponentialy
        increases making it unable for any prim typ to render it
    */
    u16 queue_size = vertex_queue.size;
    switch(type)
    {
        case _POINT:
        {
            if (queue_size == 1) {
                render_point(&vertex_queue.queue[0]);
                vertex_queue.queue.clear();
                vertex_queue.size = 0;
            }
        } break;

        case _LINE:
        {
            if (queue_size == 2) {
                render_line(vertex_queue.queue);
                vertex_queue.queue.clear();
                vertex_queue.size = 0;
            }
        } break;

        case _LINESTRIP:
        {
            //@@Hack: not sure if this fixes the previously mentioned bug 
            if (queue_size >= 3) {
                //render_point();
                vertex_queue.queue.clear();
                vertex_queue.size = 0;
                //printf("Line Strip\n");
            }
        } break;

        case _TRIANGLE:
        {
            if (queue_size == 3) {
                render_triangle(vertex_queue.queue);
                vertex_queue.queue.clear();
                vertex_queue.size = 0;
            }
        } break;

        case _TRIANGLESTRIP:
        {
            //@@Hack: not sure if this fixes the previously mentioned bug 
            if (queue_size >= 3) {
                //render_point();
                vertex_queue.queue.clear();
                vertex_queue.size = 0;
               // printf("Triangle Strip\n");
            }
        } break;

        case _TRIANGLEFAN:
        {
            //@@Hack: not sure if this fixes the previously mentioned bug 
            if (queue_size >= 3) {
                //render_point();
                vertex_queue.queue.clear();
                vertex_queue.size = 0;
                //printf("Traigle Fan\n");
            }
        } break;
        
        case _SPRITE:
        {
            if (queue_size == 2) {
                render_sprite(vertex_queue.queue);
                vertex_queue.queue.clear();
                vertex_queue.size = 0;
            }
        } break;
    }    
}

bool vsync_interrupt = false;
bool vsync_generated = false;

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
            // @@Hack: Hardcode Set vsync to generate an interrupt
            vsync_interrupt = false;
            vsync_generated = true;
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
            vsync_interrupt = false;
            vsync_generated = true;
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
            if (value & 0x8) {
                vsync_interrupt = true;
                vsync_generated = false;
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
            gs.pmode.is_circuit1        = value & 0x1;
            gs.pmode.is_circuit2        = (value >> 1) & 0x1;
            gs.pmode.CRT                = (value >> 2) & 0x3;
            gs.pmode.value_selection    = (value >> 5) & 0x1;   
            gs.pmode.output_selection   = (value >> 6) & 0x1;  
            gs.pmode.blending_selection = (value >> 7) & 0x1;
            gs.pmode.alpha_value        = (value >> 8) & 0xFF;    
            gs.pmode.unused             = 0;
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
            // gs.dispfb1.buffer_width     = ((value >> 9) & 0x3F) * 64;
            gs.dispfb1.buffer_width     = ((value >> 9) & 0x3F);
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
            gs.display1.width           = ((value >> 32) & 0xFFF) + 1;
            gs.display1.height          = ((value >> 44) & 0x7FF) + 1;
            return;
        } break;
        
        case 0x12000090:
        {
            syslog("GS_WRITE_PRIVILEDGED: write to DISPFB2. Value: [{:#x}]\n", value);
            gs.dispfb2.base_pointer     = (value & 0x1FF) * 2048;
            gs.dispfb2.buffer_width     = ((value >> 9) & 0x3F) * 64;
            gs.dispfb2.buffer_width     = ((value >> 9) & 0x3F);
            gs.dispfb2.storage_formats  = (value >> 15) & 0x1F;
            gs.dispfb2.x_position       = (value >> 32) & 0x7FF;
            gs.dispfb2.y_position       = (value >> 43) & 0x7FF;
            gs.dispfb2.value            = value;
            return;
        } break;
        
        case 0x120000A0:
        {
            syslog("GS_WRITE_PRIVILEDGED: write to DISPLAY2. Value: [{:#x}]\n", value);
            gs.display2.value           = value;
            gs.display2.x_position      = value & 0xFFF;
            gs.display2.y_position      = (value >> 11) & 0x7FF;
            gs.display2.h_magnification = (value >> 23) & 0xF;
            gs.display2.v_magnification = (value >> 27) & 0x3;
            gs.display2.width           = ((value >> 32) & 0xFFF) + 1;
            gs.display2.height          = ((value >> 44) & 0x7FF) + 1;
            return;
        } break;
        
        case 0x120000B0:
        {
            syslog("GS_WRITE_PRIVILEDGED: write to EXTBUF. Value: [{:#x}]\n", value);
            gs.extbuf.value                 = value;
            gs.extbuf.base_pointer          = (value & 0x3FFF) * 64;
            // gs.extbuf.buffer_width          = ((value >> 14) & 0x3F) * 64;
            gs.extbuf.buffer_width          = ((value >> 14) & 0x3F);
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
                vsync_interrupt = true;
                vsync_generated = false;
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
gs_write_internal (u8 address, u64 value) 
{
    switch(address)
    {
        case 0x00:
        {
            gs.prim.primitive_type          = (value) & 0x7;
            gs.prim.shading_method          = (value >> 3) & 0x1;
            gs.prim.do_texture_mapping      = (value >> 4) & 0x1;
            gs.prim.do_fogging              = (value >> 5) & 0x1;
            gs.prim.do_alpha_blending       = (value >> 6) & 0x1;
            gs.prim.do_1_pass_antialiasing  = (value >> 7) & 0x1;
            gs.prim.mapping_method          = (value >> 8) & 0x1;
            gs.prim.context                 = (value >> 9) & 0x1;
            gs.prim.fragment_value_control  = (value >> 10) & 0x1;
            gs.prim.value                   = value;
            syslog("GS_WRITE: write to PRIM. Value: [{:#04x}]\n", value);
        } break;

        case 0x01:
        {
            gs.rgbaq.r = value & 0xFF;
            gs.rgbaq.g = (value >> 8) & 0xFF;
            gs.rgbaq.b = (value >> 16) & 0xFF;
            
            u32 a      = (value >> 24) & 0xFF;
            gs.rgbaq.a = a == 0x80 ? 1.0 : a;
            
            gs.rgbaq.q = (value >> 32);
            syslog("GS_WRITE: write to RGBAQ. Value: [{:#x}]\n", value);
        } break;

        case 0x02:
        {
            /* Mostly compliant with iEEE 754 */
            // lower 8 bits of the mantissa are rounded down
            gs.st.s = value & 0xFFFFFFFF;
            gs.st.t = (value >> 32);
            syslog("GS_WRITE: write to ST. Value: [{:#x}]\n", value);
        } break;
        
        case 0x03:
        {
            gs.uv.u = (value >> 14) & 0x3FFFF;
            gs.uv.v = (value >> 32) & 0x3FFFF;
            syslog("GS_WRITE: write to UV. Value: [{:#x}]\n", value);
        } break;

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
            syslog("GS_WRITE: write to XYZF2. Value: [{:#x}]\n", value);
            
            vertex_kick(true);
            drawing_kick();
        } break;

        case 0x05:
        {
            /*
                @@Note: I dont know if the game dev checks whether or not the register
                has a max X and max Y (4095.9375) so check if that is the case in the
                occasion there has to be a hardcoded solution
            */
            gs.xyz2.x = (value) & 0xFFFF;
            gs.xyz2.y = (value >> 16) & 0xFFFF;
            gs.xyz2.z = (value >> 32);
            
            syslog("GS_WRITE: write to XYZ2. Value: [{:#x}]\n", value);
            
            vertex_kick(false);
            drawing_kick();
        } break;

        case 0x06:
            gs.tex0_1.base_pointer            = (value & 0x3fff) * 64; 
            gs.tex0_1.buffer_width            = (value >> 14) & 0x3f; 
            gs.tex0_1.pixel_storage_format    = (value >> 20) & 0x3f; 
            gs.tex0_1.texture_width           = (value >> 26) & 0xf;
            gs.tex0_1.texture_height          = (value >> 30) & 0xf;
            gs.tex0_1.color_component         = (value >> 34) & 0x1;
            gs.tex0_1.texture_function        = (value >> 35) & 0x3;
            gs.tex0_1.clut_base_pointer       = ((value >> 37) & 0x3fff) * 64;
            gs.tex0_1.clut_storage_format     = (value >> 51) & 0xf;
            gs.tex0_1.clut_storage_mode       = (value >> 55) & 0x1;
            gs.tex0_1.clut_entry_offset       = (value >> 56) & 0x1f;
            gs.tex0_1.clut_load_control       = (value >> 61) & 0x7;
            //gs.tex0_1.value                   = value;
            syslog("GS_WRITE: write to TEX0_1. Value: [{:#x}]\n", value);
        break;

        case 0x0a:
            gs.fog.fog = (value >> 56);
        break;

        case 0x18:
        {
            gs.xyoffset_1.x = (value) & 0xFFFF;
            gs.xyoffset_1.y = (value >> 32) & 0xFFFF;
            syslog("GS_WRITE: write to XYOFFSET_1. Value: [{:#x}]\n", value);
        } break;

        case 0x1A: 
        {
            gs.prmodecont.specify_prim_register = value & 0x1;
            syslog("GS_WRITE: write to PRMODECONT. Value: [{:#x}]\n", value);
        } break;

        case 0x3f:
            gs.texflush.value = value;
        break;

        case 0x40:
        {
            gs.scissor_1.min_x = value & 0x7ff;
            gs.scissor_1.max_x = (value >> 16) & 0x7FF;
            gs.scissor_1.min_y = (value >> 32) & 0x7FF;
            gs.scissor_1.max_y = (value >> 48) & 0x7FF;
            syslog("GS_WRITE: write to SCISSOR_1. Value: [{:#x}]\n", value);
        } break;
        
        case 0x45:
        {
            gs.dthe.control = value & 0x1;
            syslog("GS_WRITE: write to DTHE. Value: [{:#x}]\n", value);
        } break;
        
        case 0x46:
        {
            gs.colclamp.clamp_method = value & 0x1;
            syslog("GS_WRITE: write to COLCLAMP. Value: [{:#x}]\n", value);
        } break;

        case 0x47:
        {
            gs.test_1.alpha_test                = (value) & 0x1;
            gs.test_1.alpha_test_method         = (value >> 1) & 0x7;
            gs.test_1.alpha_comparison_value    = (value >> 4) & 0xFF;
            gs.test_1.alpha_fail_method         = (value >> 11) & 0x3;
            gs.test_1.destination_test          = (value >> 14) & 0x1;
            gs.test_1.destination_test_mode     = (value >> 15) & 0x1;
            gs.test_1.depth_test                = (value >> 16) & 0x1;
            gs.test_1.depth_test_method         = (value >> 17) & 0x3;
            //gs.test_1.value                     = value;
            
            syslog("GS_WRITE: write to TEST_1. Value: [{:#x}]\n", value);
        } break;

        case 0x4C:
        {
            u8 test1  = ((value >> 16) & 0x3F);
            u16 test2 = ((value >> 16) & 0x3F);
            test1 *= 64;
            test2 *= 64;
            gs.frame_1.base_pointer     = (value & 0x1FF) * 2048;
            gs.frame_1.buffer_width     = ((value >> 16) & 0x3F) * 64;
            //gs.frame_1.buffer_width     = ((value >> 16) & 0x3F);
            gs.frame_1.storage_format   = (value >> 24) & 0x3F;
            gs.frame_1.drawing_mask     = (value >> 32);
            syslog("GS_WRITE: write to FRAME_1. Value: [{:#x}]\n", value);
        } break;

        case 0x4E:
        {
            gs.zbuf_1.base_pointer      = (value & 0x1FF) * 2048;
            gs.zbuf_1.storage_format    = (value >> 24) & 0xF;
            gs.zbuf_1.z_value_mask      = value >> 32 & 0x1;
            syslog("GS_WRITE: write to ZBUF_1. Value: [{:#x}]\n", value);
            return;
        } break;

        case 0x50: 
        {
            gs.bitbltbuf.src_base_pointer       = (value & 0x7fff) * 64;  
            gs.bitbltbuf.src_buffer_width       = ((value >> 16) & 0x7F) * 64;
            gs.bitbltbuf.src_storage_format     = (value >> 24) & 0x7F; 
            gs.bitbltbuf.dest_base_pointer      = ((value >> 32) & 0x7FFF) * 64;
            gs.bitbltbuf.dest_buffer_width      = ((value >> 48) & 0x7F) * 64;  
            gs.bitbltbuf.dest_storage_format    = (value >> 56) & 0x7F;
            syslog("GS_WRITE: write to BITBLTBUF. Value: [{:#x}]\n", value);
        } break;

        case 0x51: 
        {
            gs.trxpos.src_x_coord             = value & 0x7FF;
            gs.trxpos.src_y_coord             = (value >> 16) & 0x7FF;
            gs.trxpos.dest_x_coord            = (value >> 32) & 0x7FF;
            gs.trxpos.dest_y_coord            = (value >> 48) & 0x7FF;
            gs.trxpos.transmission_direction  = (value >> 59) & 0x3;
            syslog("GS_WRITE: write to TRXPOS. Value: [{:#x}]\n", value);
        } break;

        case 0x52: 
        {
            gs.trxreg.width  = value & 0xFFF;
            gs.trxreg.height = (value >> 32) & 0xFFF;
            syslog("GS_WRITE: write to TRXREG. Value: [{:#x}]\n", value);
        } break;

        case 0x53: 
        {
            gs.trxdir.direction = value & 0x3;
            syslog("GS_WRITE: write to TRXDIR. Value: [{:#x}]\n", value);
            switch(gs.trxdir.direction)
            {
                case 0: syslog("Executing Host => Local Transmission\n");  break;
                case 1: syslog("Executing Local => Host Transmission\n");  break;
                case 2: syslog("Executing Local => Local Transmission\n"); break;
            }
        } break;

        case 0x54: 
        {
            if (gs.trxdir.direction == 0)
                gs_write_hwreg(value);
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
