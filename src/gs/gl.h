#ifndef GL_H
#define GL_H

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