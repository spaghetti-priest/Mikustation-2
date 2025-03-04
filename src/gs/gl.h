#ifndef GL_H
#define GL_H

bool 	init_opengl(SDL_Context *context);
void 	shutdown_opengl();
void 	gl_swap_framebuffers(SDL_Context *context);
void 	gl_init_vram();
void 	gl_draw_point(v3 *pos, v4 *color);
void 	gl_draw_line(v3 *pos, v4 *color);
void 	gl_draw_sprite (v3 *vert1, v3 *vert2, v3 *vert3, v3 *vert4, v4 *color);
void 	gl_upload_transmission_buffer(u32 x, u32 y, u32 width, u32 height, u64 data);

#endif