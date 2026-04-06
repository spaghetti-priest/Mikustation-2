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

bool view_ee_registers     = false;
bool view_ee_timers        = false;
bool open_gpr_registers    = true;
bool open_cop1_registers   = true;

static const char *gpr_register_table[32] = {
   "$r0", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
   "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
   "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
   "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra"
};

static const char *cop1_register_table[32] = {
"$f0",  "$f1",  "$f2",  "$f3",  "$f4",  "$f5",  "$f6",  "$f7", 
"$f8",  "$f9",  "$f10", "$f11", "$f12", "$f13", "$f14", "$f15", 
"$f16", "$f17", "$f18", "$f19", "$f20", "$f21", "$f22", "$f23", 
"$f24", "$f25", "$f26", "$f27", "$f28", "$f29", "$f30", "$f31" 
};


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
         if (ImGui::MenuItem("View EE Timers")) 
         {
            if (view_ee_timers == true) {
               view_ee_timers = false;
            } else {
               view_ee_timers = true;
            }            
         }

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
   
   const char* names[2] = { "GPR", "COP1" };
   static ImGuiTabBarFlags tab_bar_flags =   ImGuiTabBarFlags_Reorderable | 
                                             ImGuiTabBarFlags_DrawSelectedOverline;   

   // @Incomplete: Add a refresh rate for the timer viewer 
   if (view_ee_registers == true)
   {
      ImGui::Begin("EE Registers");
      if (ImGui::BeginTabBar("EE Register Tabs", tab_bar_flags))
      {
         if (ImGui::BeginTabItem("GPR"))
         {
            ImGui::Text("%s: [%08x] \n", "$pc",  get_pc_register(ee));
            ImGui::Text("%s: [%08x] \n", "$sa",  get_sa_register(ee));
            ImGui::Text("%s: [%08x] \n", "$hi",  get_HI_register(ee));
            ImGui::SameLine();
            ImGui::Text("%s: [%08x] \n", "$lo",  get_LO_register(ee));
            ImGui::Text("%s: [%08x] \n", "$hi1", get_HI1_register(ee));
            ImGui::SameLine();
            ImGui::Text("%s: [%08x] \n", "$lo1", get_LO1_register(ee));
            ImGui::Separator();
            
            for (int i = 0; i < 32; ++i)
            {
               ImGui::Text("%s: [%08x] \n", gpr_register_table[i], dump_ee_register(ee, i));
               ImGui::SameLine();
               i++;
               ImGui::Text("%s: [%08x] \n", gpr_register_table[i], dump_ee_register(ee, i));
            }
            ImGui::EndTabItem();
         }
         
         if (ImGui::BeginTabItem("C0P1"))
         {
            for (int i = 0; i < 32; ++i)
            {
               // @Incomplete: Allow to view in float or hexadecimal
               ImGui::Text("%s: [%f] \n", cop1_register_table[i], dump_cop1_register(i));
               ImGui::SameLine();
               i++;
               ImGui::Text("%s: [%f] \n", cop1_register_table[i], dump_cop1_register(i));
            }    
            ImGui::EndTabItem();
         }
         ImGui::EndTabBar();
      }
      ImGui::End();
   }

   if (view_ee_timers == true)
   {
      ImGui::Begin("EE Timers");
      {
         for (int i = 0; i < 4; i++)
         {
            Timer r = dump_ee_timer(i);
            ImGui::PushID(i);
            if (ImGui::TreeNode("", "Timer %d", i))
            {
               ImGui::Separator();
               ImGui::Text("%s: [%08x] \n", "Count", r.count.count);
               ImGui::Separator();
               ImGui::Text("%s: [%08x] \n", "Clock selection",        r.mode.clock_selection);
               ImGui::Text("%s: [%08x] \n", "Gate function enable",   r.mode.gate_function_enable);
               ImGui::Text("%s: [%08x] \n", "Gate selection",         r.mode.gate_selection);
               ImGui::Text("%s: [%08x] \n", "Gate mode",              r.mode.gate_mode);
               ImGui::Text("%s: [%08x] \n", "Zero return",            r.mode.zero_return);
               ImGui::Text("%s: [%08x] \n", "Count enable",           r.mode.count_enable);
               ImGui::Text("%s: [%08x] \n", "Compare interrupt",      r.mode.compare_interrupt);
               ImGui::Text("%s: [%08x] \n", "Overflow interrupt",     r.mode.overflow_interrupt);
               ImGui::Text("%s: [%08x] \n", "Equal flag",             r.mode.equal_flag);
               ImGui::Text("%s: [%08x] \n", "Overflow flag",          r.mode.overflow_flag);
               ImGui::Separator();
               ImGui::Text("%s: [%08x] \n", "Compare", r.comp.compare);
               ImGui::Separator();

               if (i <= 1) ImGui::Text("%s: [%08x] \n", "Hold", r.hold.hold);

               ImGui::TreePop();
            }
            ImGui::PopID();
         }
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