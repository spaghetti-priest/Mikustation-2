#include <glad/glad.h>

struct Vram {
    unsigned int backbuffer;
    unsigned int framebuffer;
    unsigned int zbuffer;
    unsigned int texture;
    unsigned int clut;
};

struct OpenGL {
    SDL_GLContext gl_context;
    unsigned screen_shader;
    Vram vram;
};

// #define GL_CHECK(x) (if(x) printf("%"))
OpenGL opengl = {0};
static unsigned int gl_setup_screen_shader ();

bool 
init_opengl (SDL_Context *context)
{
    memset(&opengl, 0, sizeof(OpenGL));

	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

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

    // @Incomplete: Does not account for backbuffer resizing
    glViewport(0,0, context->backbuffer->w, context->backbuffer->h);
    glClearColor(0.1, 0.1, 0.1, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    
    opengl.screen_shader = gl_setup_screen_shader();

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
    
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vram->backbuffer, 0);
}

static unsigned int
gl_setup_screen_shader ()
{
    unsigned int vertex_shader = 0;
    unsigned int frag_shader = 0;
    unsigned int shader_program = 0;
    int success = 0;
    char log[1024] = "\0";

    // @Incopmlete: There should be a uniform of what the current vram "resolution" is to calculate the vram to NDC transform
    const char *vertex_source = "#version 330 core\n"
    "layout (location = 0) in vec3 in_pos;\n"
    "layout (location = 1) in vec4 in_color;\n"
    "out vec4 out_color;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4((in_pos.x / 511.0) - 1 , 1 - (in_pos.y / 255.0), in_pos.z, 1.0);\n"
    "   out_color = vec4(in_color.x / 255.f,  in_color.y / 255.f, in_color.z / 255.f, in_color.w);\n"
    "}\0";

    const char *fragment_source = "#version 330 core\n"
    "in  vec4 out_color;\n"
    "out vec4 frag_color;\n"
    "void main()\n"
    "{\n"
    "   frag_color = out_color;\n"
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

void 
gl_draw_point(v3 *position, v4 *color)
{
    OpenGL *gl = &opengl;
    float data[] = 
    {
        (float)position->x, (float)position->y, (float)position->z,    (float)color->x, (float)color->y, (float)color->z, (float)color->w
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);  
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);  
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0); // position
    glEnableVertexAttribArray(1); // color

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 7, (void*)0 );
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 7, (void*)(sizeof(float) * 3) );

    glUseProgram(gl->screen_shader);
    glBindVertexArray(VAO);
    glDrawArrays(GL_POINTS, 0, 1);
    glBindVertexArray(0);
}

unsigned int VAO, VBO;

void 
gl_draw_line(v3 *position, v4 *color)
{
    OpenGL *gl = &opengl;
    float data[] = 
    {
        (float)position[0].x, (float)position[0].y, (float)position[0].z,    (float)color->x, (float)color->y, (float)color->z, (float)color->w,
        (float)position[1].x, (float)position[1].y, (float)position[1].z,    (float)color->x, (float)color->y, (float)color->z, (float)color->w
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);  
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);  
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0); // position
    glEnableVertexAttribArray(1); // color

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 7, (void*)0 );
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 7, (void*)(sizeof(float) * 3) );

    glUseProgram(gl->screen_shader);
    glBindVertexArray(VAO);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
}

void
gl_draw_sprite (v3 *vert1, v3 *vert2, v3 *vert3, v3 *vert4, v4 *color)
{
    OpenGL *gl = &opengl;
    
    unsigned int EBO;
    glGenBuffers(1, &EBO);

    float data[] = 
    {
        (float)vert1->x, (float)vert1->y, (float)vert1->z,    (float)color->x, (float)color->y, (float)color->z, (float)color->w,
        (float)vert2->x, (float)vert2->y, (float)vert2->z,    (float)color->x, (float)color->y, (float)color->z, (float)color->w,
        (float)vert3->x, (float)vert3->y, (float)vert3->z,    (float)color->x, (float)color->y, (float)color->z, (float)color->w,
        (float)vert4->x, (float)vert4->y, (float)vert4->z,    (float)color->x, (float)color->y, (float)color->z, (float)color->w,
    };

    unsigned int indices[] = 
    {
        0, 2, 1,
        0, 1, 3
    };
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);  
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);  
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0); // position
    glEnableVertexAttribArray(1); // color

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 7, (void*)0 );
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 7, (void*)(sizeof(float) * 3) );

    glUseProgram(gl->screen_shader);
    glBindVertexArray(VAO);
    // glDrawArrays(GL_TRIANGLES, 0, 6);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void 
gl_upload_transmission_buffer (u32 x, u32 y, u32 width, u32 height, u64 data)
{
    // glActiveTexture(GL_TEXTURE0);
    Vram *vram = &opengl.vram;
    glBindTexture(GL_TEXTURE_2D, vram->backbuffer);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA8, GL_UNSIGNED_BYTE, (void*)data);
}

void
gl_swap_framebuffers(SDL_Context *context)
{
    SDL_GL_SwapWindow(context->window);
}

void 
shutdown_opengl ()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(opengl.screen_shader);
	SDL_GL_DeleteContext(&opengl.gl_context);
}