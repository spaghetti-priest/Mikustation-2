#ifndef GL_H
#define GL_H

typedef struct Vram Vram;
struct Vram 
{
   u32 backbuffer;
   u32 framebuffer;
   u32 zbuffer;
   u32 texture;
   u32 clut;
};

typedef struct OpenGL OpenGL;
struct OpenGL 
{
   SDL_GLContext gl_context;
   u32 screen_shader;
   Vram vram;
};

bool 	init_opengl(SDL_Context *context);
void 	shutdown_opengl();
void 	gl_swap_framebuffers(SDL_Window *window, SDL_Backbuffer *backbuffer);
static void 	gl_init_vram();
static void 	gl_draw_point(V3 *position, V4 color);
static void 	gl_draw_line(V3 *position, V4 color);
static void 	gl_draw_sprite(V3 *position, V4 color);
static void 	gl_upload_transmission_buffer(u32 x, u32 y, u32 width, u32 height, u64 data);
static void    gl_render_frame();
#endif