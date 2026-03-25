#ifndef DEBUG_GRAPHICS_H
#define DEBUG_GRAPHICS_H

static bool init_imgui_opengl(SDL_Window *window, SDL_GLContext *gl_context);
static void imgui_begin_frame();
static void imgui_end_frame();
static void imgui_render_frame();
static void imgui_shutdown();

#endif