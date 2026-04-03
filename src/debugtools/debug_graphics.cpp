float TEXT_BASE_HEIGHT = 1;

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
   
   TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();
   
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

bool view_ee_registers = false;

static void
imgui_render_frame(R5900_Core *ee)
{
   // imgui_begin_frame();

   ImGui_ImplOpenGL3_NewFrame();
   ImGui_ImplSDL2_NewFrame();
   ImGui::NewFrame();

   ImGuiStyle& style    = ImGui::GetStyle();
   style.WindowRounding = 8.0f;
   // style.FrameRounding     = 11.0f;


   if (ImGui::BeginMainMenuBar()) 
   {
      if (ImGui::BeginMenu("File")) 
      {
         if (ImGui::MenuItem("Open", "CTRL+O")) {}
         ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Edit")) 
      {
         if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
         ImGui::Separator();
         ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("View")) 
      {
         if (ImGui::MenuItem("View VRAM")) {}
         ImGui::Separator();
         if (ImGui::MenuItem("View EE Instructions")) {}
         if (ImGui::MenuItem("View EE Registers")) 
         {
            if (view_ee_registers == true) {
               view_ee_registers = false;
            } else {
               view_ee_registers = true;
            }
         }
         ImGui::Separator();
         ImGui::EndMenu();
      }
   }
   ImGui::EndMainMenuBar();
   
   if (view_ee_registers == true)
   {
      if (ImGui::Begin("EE Registers", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) 
      {
         ImGui::Text("$r0: [%08x] \n", dump_ee_register(ee, 0));
         ImGui::SameLine();
         ImGui::Text("$at: [%08x] \n", dump_ee_register(ee, 1));
         ImGui::Text("$v0: [%08x] \n", dump_ee_register(ee, 2));
         ImGui::SameLine(); 
         ImGui::Text("$v1: [%08x] \n", dump_ee_register(ee, 3));
         ImGui::Text("$a0: [%08x] \n", dump_ee_register(ee, 4));
         ImGui::SameLine(); 
         ImGui::Text("$a1: [%08x] \n", dump_ee_register(ee, 5));
         ImGui::Text("$a2: [%08x] \n", dump_ee_register(ee, 6));
         ImGui::SameLine(); 
         ImGui::Text("$a3: [%08x] \n", dump_ee_register(ee, 7));
         ImGui::Text("$t0: [%08x] \n", dump_ee_register(ee, 8));
         ImGui::SameLine(); 
         ImGui::Text("$t1: [%08x] \n", dump_ee_register(ee, 9));
         ImGui::Text("$t2: [%08x] \n", dump_ee_register(ee, 10));
         ImGui::SameLine(); 
         ImGui::Text("$t3: [%08x] \n", dump_ee_register(ee, 11));
         ImGui::Text("$t4: [%08x] \n", dump_ee_register(ee, 12));
         ImGui::SameLine(); 
         ImGui::Text("$t5: [%08x] \n", dump_ee_register(ee, 13));
         ImGui::Text("$t6: [%08x] \n", dump_ee_register(ee, 14));
         ImGui::SameLine(); 
         ImGui::Text("$t7: [%08x] \n", dump_ee_register(ee, 15));
         ImGui::Text("$s0: [%08x] \n", dump_ee_register(ee, 16));
         ImGui::SameLine(); 
         ImGui::Text("$s1: [%08x] \n", dump_ee_register(ee, 17));
         ImGui::Text("$s2: [%08x] \n", dump_ee_register(ee, 18));
         ImGui::SameLine(); 
         ImGui::Text("$s3: [%08x] \n", dump_ee_register(ee, 19));
         ImGui::Text("$s4: [%08x] \n", dump_ee_register(ee, 20));
         ImGui::SameLine(); 
         ImGui::Text("$s5: [%08x] \n", dump_ee_register(ee, 21));
         ImGui::Text("$s6: [%08x] \n", dump_ee_register(ee, 22));
         ImGui::SameLine(); 
         ImGui::Text("$s7: [%08x] \n", dump_ee_register(ee, 23));
         ImGui::Text("$t8: [%08x] \n", dump_ee_register(ee, 24));
         ImGui::SameLine(); 
         ImGui::Text("$t9: [%08x] \n", dump_ee_register(ee, 25));
         ImGui::Text("$k0: [%08x] \n", dump_ee_register(ee, 26));
         ImGui::SameLine(); 
         ImGui::Text("$k1: [%08x] \n", dump_ee_register(ee, 27));
         ImGui::Text("$gp: [%08x] \n", dump_ee_register(ee, 28));
         ImGui::SameLine(); 
         ImGui::Text("$sp: [%08x] \n", dump_ee_register(ee, 29));
         ImGui::Text("$fp: [%08x] \n", dump_ee_register(ee, 30));
      } 
      ImGui::End();
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