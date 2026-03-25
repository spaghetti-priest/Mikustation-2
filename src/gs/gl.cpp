#include "../debugtools/debug_graphics.h"
#include "../debugtools/debug_graphics.cpp"

typedef struct Vram Vram;
struct Vram {
   u32 backbuffer;
   u32 framebuffer;
   u32 zbuffer;
   u32 texture;
   u32 clut;
};

typedef struct OpenGL OpenGL;
struct OpenGL {
   SDL_GLContext gl_context;
   u32 screen_shader;
   Vram vram;
};

/*
   -- OpenGL sprite clearing is weird
   -- OpenGL must accept uniforms for the backbuffer screen dimensions
   -- OpenGL has weird screen dimensions
   -- Implement OpenGL triangle rendering
   -- Implement Draw Queue for opengl (cleanup rendering code)
*/

// #define GL_CHECK(x) (if(x) printf("%"))
OpenGL opengl = {0};
static unsigned int gl_setup_screen_shader ();

// @Incomplete: The gl viewport is not synced with the backbuffer
// that is provided with sdl_backbuffer
bool
init_opengl (SDL_Context *context, int screen_w, int screen_h)
{
   memset(&opengl, 0, sizeof(OpenGL));

	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
   SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

   SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
   SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
   SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

 	opengl.gl_context = SDL_GL_CreateContext(context->window);

 	if (!gladLoadGLLoader((SDL_GL_GetProcAddress))) {
      printf("Failed to initialize OpenGL extensions loader");
      return 0;
   }

   printf("Vendor:   %s\n", glGetString(GL_VENDOR));
   printf("Renderer: %s\n", glGetString(GL_RENDERER));
   printf("Version:  %s\n", glGetString(GL_VERSION));

   if (SDL_GL_SetSwapInterval(0) != 0)
   {
      errlog("[ERROR]: Failed to get GL_SwapInterval {:s}\n", SDL_GetError());
      return 0;
   };

   //glEnable(GL_DEPTH_TEST);
   //glEnable(GL_MULTISAMPLE);

   // @Incomplete: The gl viewport is not synced with the backbuffer
   // that is provided with sdl_backbuffer
   // @Todo: When the backbuffer is resized the bb should retrieve
   // the new screen w and h
   glViewport(0,0, screen_w, screen_h);
   glClearColor(0.1, 0.1, 0.1, 1.0);
   glClear(GL_COLOR_BUFFER_BIT);

   opengl.screen_shader = gl_setup_screen_shader();

   bool success = init_imgui_opengl(context->window, &opengl.gl_context);
   return 1;
}

void
gl_init_vram ()
{
   Vram *vram = &opengl.vram;
   glGenFramebuffers(1, &vram->framebuffer);
   glBindFramebuffer(GL_FRAMEBUFFER, vram->framebuffer);

   glGenTextures(1, &vram->backbuffer);
   glBindTexture(GL_TEXTURE_2D, vram->backbuffer);
   glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2048, 2048);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vram->backbuffer, 0);

   unsigned int attachments[1] = {GL_COLOR_ATTACHMENT0};
   glDrawBuffers(1, attachments);

   // glGenRenderbuffers(1, &vram->backbuffer);
   // glBindRenderbuffer(GL_RENDERBUFFER, vram->backbuffer);
   // glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 2048, 2048);
   // glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, vram->backbuffer);

   GLenum framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
   if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE)
      printf("[ERROR]: Framebuffer is not complete");

   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   // @Temporary: This uniform is for input resolution
   //glUniform2fv(glGetUniformLocation(ID, name), 1, &value[0]);
}

static unsigned int
gl_setup_screen_shader ()
{
   unsigned int vertex_shader    = 0;
   unsigned int frag_shader      = 0;
   unsigned int shader_program   = 0;
   int success                   = 0;
   char log[1024] = "\0";

   // @Incopmlete: There should be a uniform of what the current vram "resolution" is to calculate the vram to NDC transform
   // const char *vertex_source = "#version 330 core\n"
   // "layout (location = 0) in vec3 in_pos;\n"
   // "layout (location = 1) in vec4 in_color;\n"
   // "uniform vec2 input_resolution;\n"
   // "out vec4 out_color;\n"
   // "void main()\n"
   // "{\n"
   // "   gl_Position = vec4(in_pos.x, in_pos.y, in_pos.z, 1.0);\n"
   // "   out_color = vec4(in_color.x, in_color.y, in_color.z, in_color.w);\n"
   // "}\0";

   // const char *fragment_source = "#version 330 core\n"
   // "in  vec4 out_color;\n"
   // "out vec4 frag_color;\n"
   // "void main()\n"
   // "{\n"
   // "   frag_color = out_color;\n"
   // "}\0";

   const char *vertex_source = "#version 330 core\n"
   "layout (location = 0) in vec3 in_pos;\n"
   "layout (location = 1) in vec4 in_color;\n"
   "uniform vec2 input_resolution;\n"
   "out vec4 out_color;\n"
   "out vec2 out_texcoords;\n"
   "void main()\n"
   "{\n"
   "   gl_Position   = vec4(in_pos.x, in_pos.y, in_pos.z, 1.0);\n"
   "   out_texcoords = input_resolution;\n"
   "   out_color     = vec4(in_color.x, in_color.y, in_color.z, in_color.w);\n"
   "}\0";

   const char *fragment_source = "#version 330 core\n"
   "in  vec4 out_color;\n"
   "in  vec2 out_texcoords;\n"
   "out vec4 frag_color;\n"
   "uniform sampler2D vram_texture;\n"
   "void main()\n"
   "{\n"
   "   frag_color = texture(vram_texture, out_texcoords);\n"
   "}\0";

   vertex_shader = glCreateShader(GL_VERTEX_SHADER);
   glShaderSource(vertex_shader, 1, (const GLchar**)&vertex_source, NULL);
   glCompileShader(vertex_shader);
   glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);

   frag_shader = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(frag_shader, 1, (const GLchar**)&fragment_source, NULL);
   glCompileShader(frag_shader);
   glGetShaderiv(frag_shader, GL_COMPILE_STATUS, &success);

   shader_program = glCreateProgram();
   glAttachShader(shader_program, vertex_shader);
   glAttachShader(shader_program, frag_shader);
   glBindFragDataLocation(shader_program, 0, "frag_color");
   glLinkProgram(shader_program);
   glGetProgramiv(shader_program, GL_LINK_STATUS, &success);

   if (!success) {
      glGetProgramInfoLog(shader_program, 1024, NULL, log);
      errlog("[ERROR]: Failed to compile shader: \n{:s}\n", (const char*)log);
   }

   glDeleteShader(vertex_shader);
   glDeleteShader(frag_shader);

   return shader_program;
}

typedef struct GLVertex GLVertex;
struct GLVertex
{
   V3 pos;
   V4 color;
   // V2 texcoords;
   // f32 q;
};

inline GLVertex
gs_to_gl_vertex (V3 position, V4 color)
{
   GLVertex res   = {};
   res.pos        = position;
   res.color      = color;
   return res;
}

// @Note: Make sure to grab the current resolution from gs_get_crt()
inline V3
window_to_raster_transform (V3 position)
{
   V3 out_pos{};
   out_pos.x = ((position.x / 511.0) * 2) - 1;
   out_pos.y = 1 - ((position.y / 255.0) * 2);
   out_pos.z = position.z;
   return out_pos;
//    out_pos.x = (position.x / 511.0) - 1;
//    out_pos.y = 1 - (position.y / 255.0);
}

int draw_calls = 0;
typedef enum OpenGLPrimitiveTypes OpenGLPrimitiveTypes;
enum OpenGLPrimitiveTypes {
   PRIM_POINT,
};

typedef struct GLPacket GLPacket;
struct GLPacket
{
   GLVertex vertex;
   OpenGLPrimitiveTypes primitive_type;
};

struct GLPacketQueue {
   size_t draw_calls;
   std::vector<GLPacket> packets;
};

static GLPacketQueue packet_queue = {
   .draw_calls    = 0,
   .packets       = {},
};

unsigned int VAO, VBO;

static void
gl_draw_point (V3 *position, V4 color)
{
   OpenGL *gl = &opengl;

   position[0] = window_to_raster_transform(position[0]);

   GLVertex data = gs_to_gl_vertex(position[0], color);

   glGenVertexArrays(1, &VAO);
   glGenBuffers(1, &VBO);
   glBindVertexArray(VAO);

   glBindBuffer(GL_ARRAY_BUFFER, VBO);
   glBufferData(GL_ARRAY_BUFFER, sizeof(GLVertex) * 1, &data, GL_DYNAMIC_DRAW);

   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);

   glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex), (void*)offsetof(GLVertex, pos));
   glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GLVertex), (void*)offsetof(GLVertex, color));

   //gl_write_to_vram(gl, VAO, _POINT, vertex_count, index_count);

   // glUseProgram(gl->screen_shader);
   // glBindVertexArray(VAO);
   // glDrawArrays(GL_POINTS, 0, 1);
   // glBindVertexArray(0);

   GLPacket new_packet = {
      .vertex           = data,
      .primitive_type   = PRIM_POINT,
   };

   //packet_queue.packets                          = (GLPacket*)malloc(sizeof(GLPacket) * 1);
   packet_queue.packets.push_back(new_packet);
   packet_queue.draw_calls += 1;

   printf("");
}

static void
gl_draw_line (V3 *position, V4 color)
{
   OpenGL *gl = &opengl;
   position[0] = window_to_raster_transform(position[0]);
   position[1] = window_to_raster_transform(position[1]);

   GLVertex data[2];
   data[0] = gs_to_gl_vertex(position[0], color);
   data[1] = gs_to_gl_vertex(position[1], color);

   glGenVertexArrays(1, &VAO);
   glGenBuffers(1, &VBO);
   glBindVertexArray(VAO);

   glBindBuffer(GL_ARRAY_BUFFER, VBO);
   glBufferData(GL_ARRAY_BUFFER, sizeof(GLVertex) * 2, &data,  GL_DYNAMIC_DRAW);

   glEnableVertexAttribArray(0);
   glEnableVertexAttribArray(1);

   // glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 7, (void*)0 );
   // glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 7, (void*)(sizeof(float) * 3));

   // @Bug: Cannot tell if theres color blending or any other problem with marshaling the data. Just know that this is where most of the problems
   // lie
   glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex), (void*)offsetof(GLVertex, pos));
   glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GLVertex), (void*)offsetof(GLVertex, color));

   glUseProgram(gl->screen_shader);
   glBindVertexArray(VAO);
   glDrawArrays(GL_LINES, 0, 2);
   glBindVertexArray(0);
}

static void
gl_draw_sprite (V3 *position, V4 color)
{
#if 1
   OpenGL *gl = &opengl;

   position[0] = window_to_raster_transform(position[0]);
   position[1] = window_to_raster_transform(position[1]);
   position[2] = window_to_raster_transform(position[2]);
   position[3] = window_to_raster_transform(position[3]);

   V4 debug_red_color = v4(1, 0, 0, 1);
   GLVertex data[4];
   data[0] = gs_to_gl_vertex(position[0], color);
   data[1] = gs_to_gl_vertex(position[1], color);
   data[2] = gs_to_gl_vertex(position[2], color);
   data[3] = gs_to_gl_vertex(position[3], color);

   const u32 index_count = 6;
   unsigned int indices[index_count] =
   {
      0, 2, 1,
      0, 1, 3
   };

   // unsigned int indices[index_count] =
   // {
   //    0, 1, 3,
   //    1, 2, 3
   // };

   unsigned int EBO;
   glGenVertexArrays(1, &VAO);
   glGenBuffers(1, &VBO);
   glBindVertexArray(VAO);
   glGenBuffers(1, &EBO);

   glBindBuffer(GL_ARRAY_BUFFER, VBO);
   glBufferData(GL_ARRAY_BUFFER, sizeof(GLVertex) * 4, data, GL_DYNAMIC_DRAW);

   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
   glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

   glEnableVertexAttribArray(0); // position
   glEnableVertexAttribArray(1); // color

   // @Bug: Cannot tell if theres color blending or any other problem with marshaling the data. Just know that this is where most of the problems
   // lie
   glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex), (void*)offsetof(GLVertex, pos));
   glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GLVertex), (void*)offsetof(GLVertex, color));

   glUseProgram(gl->screen_shader);
   glBindVertexArray(VAO);
   // glDrawArrays(GL_TRIANGLES, 0, 6);
   glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, 0);
   glBindVertexArray(0);
   #endif
}

static void
gl_upload_transmission_buffer (u32 x, u32 y, u32 width, u32 height, u64 data)
{
   // glActiveTexture(GL_TEXTURE0);
   // Vram *vram = &opengl.vram;
   // glBindTexture(GL_TEXTURE_2D, vram->backbuffer);
   // glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA8, GL_UNSIGNED_BYTE, (void*)data);
}

void
gl_swap_framebuffers(SDL_Window *window, SDL_Backbuffer *backbuffer)
{
   draw_calls = 0;
   // glViewport(0, 0, backbuffer.w, backbuffer.h);
   SDL_GL_SwapWindow(window);
}

static void
gl_render_frame()
{
   OpenGL *gl = &opengl;
   Vram *vram = &gl->vram;

   glClear(GL_COLOR_BUFFER_BIT);
   imgui_render_frame();

   glBindFramebuffer(GL_FRAMEBUFFER, vram->backbuffer);
   glClearColor(0.1, 0.1, 0.1, 1.0);
   glClear(GL_COLOR_BUFFER_BIT);
   glUseProgram(gl->screen_shader);
   glActiveTexture(GL_TEXTURE0);
   
   for (int i = 0; i < packet_queue.draw_calls; ++i) {
      GLPacket packet = packet_queue.packets[i];
      switch(packet.primitive_type)
      {
         case PRIM_POINT:
         {
            glBindVertexArray(VAO);
            glDrawArrays(GL_POINTS, 0, 1);
            glBindVertexArray(0);
         } break;
      }
   }

   glUniform1i(glGetUniformLocation(gl->screen_shader, "vram_texture"), 1);
   glBindTexture(GL_TEXTURE_2D, vram->backbuffer);
   glActiveTexture(GL_TEXTURE0);
   packet_queue.packets.clear();
   packet_queue.draw_calls = 0;

   glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

void
shutdown_opengl ()
{
   glDeleteVertexArrays(1, &VAO);
   glDeleteBuffers(1, &VBO);
   glDeleteProgram(opengl.screen_shader);
	SDL_GL_DeleteContext(&opengl.gl_context);
}