static bool
init_imgui_opengl (SDL_Window *window, SDL_GLContext *gl_context)
{
   const char *glsl_version = "#version 330 core";
   IMGUI_CHECKVERSION();
   ImGui::CreateContext();

   ImGuiIO& io     = ImGui::GetIO(); (void)io;
   io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
   io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

   ImGui::StyleColorsDark();

   ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
   ImGui_ImplOpenGL3_Init(glsl_version);

    // const char *font_path0 = "C:\\pepsiman\\guigui\\extern\\imgui\\misc\\fonts\\MINGLIU.ttf";
   // @Incomplete: Using Absolute pathing for right now
   const char *font_path0 = "C:\\Mikustation-2\\data\\fonts\\liberation-mono.ttf";
   ImFont *font = io.Fonts->AddFontFromFileTTF(font_path0,
                                               18.0f,
                                               nullptr,
                                               io.Fonts->GetGlyphRangesJapanese());
   IM_ASSERT(font != nullptr);
   return true;
}

static void
imgui_begin_frame()
{
   // ImGui_ImplOpenGL3_NewFrame();
   // ImGui_ImplSDL2_NewFrame();
   // ImGui::NewFrame();
}

static void
imgui_end_frame()
{
   ImGui::Render();
   ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

static void
imgui_render_frame()
{
   // imgui_begin_frame();

   ImGui_ImplOpenGL3_NewFrame();
   ImGui_ImplSDL2_NewFrame();
   ImGui::NewFrame();

   ImGuiStyle& style    = ImGui::GetStyle();
   style.WindowRounding = 8.0f;
   // style.FrameRounding     = 11.0f;

   if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
         if (ImGui::MenuItem("Open", "CTRL+O")) {}
         ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Edit")) {
         if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
         ImGui::Separator();
         ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("View")) {
         if (ImGui::MenuItem("View VRAM")) {}
         ImGui::Separator();
         ImGui::EndMenu();
      }

      ImGui::EndMainMenuBar();
   }

   imgui_end_frame();
}

static void
imgui_shutdown()
{
   ImGui_ImplOpenGL3_Shutdown();
   ImGui_ImplSDL2_Shutdown();
   ImGui::DestroyContext();
}