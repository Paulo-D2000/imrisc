#include "gui.h"
#include "ram.h"
#include <algorithm>
#include <mutex>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

ImRiscGui::ImRiscGui(int Width, int Height)
    : show_demo_window(true), show_another_window(true),
      clear_color(0.45f, 0.55f, 0.60f, 1.00f), m_window(NULL), fb_width(Width),
      fb_height(Height)
{
    fb_image_data = new uint32_t[fb_width * fb_height];
}

bool ImRiscGui::Init()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 0;

// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char *glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char *glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);           // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    // Create m_window with graphics context
    m_window = glfwCreateWindow(1366, 768, "ImRisc-V GUI", NULL, NULL);
    if (m_window == nullptr)
        return 0;
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableGamepad;            // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Enable Docking

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    glGenTextures(1, &fb_texture);
    glBindTexture(GL_TEXTURE_2D, fb_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                    GL_CLAMP_TO_EDGE);                                   // This is required on WebGL for non
                                                                         // power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same
#else
    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
        GL_CLAMP);                                               // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP); // Same
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fb_width, fb_height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, fb_image_data);

    return true;
}

bool ImRiscGui::Run()
{
    bool ret = !glfwWindowShouldClose(m_window);
    if (ret)
    {
        glfwPollEvents();
        glfwSetKeyCallback(m_window, key_callback);

        // Start the Dear ImGui frame
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- Dock host ---
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        {
            ImGui::Begin("##DockHost",
                         nullptr,
                         ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoCollapse);

            ImGuiID dockspace_id = ImGui::GetID("MainDock");
            ImGui::DockSpace(dockspace_id);

            // Build layout ONCE
            static bool first = true;
            if (first)
            {
                first = false;

                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(
                    dockspace_id, ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(
                    dockspace_id, ImGui::GetIO().DisplaySize);
                
                ImGuiID main;
                ImGuiID bottom;
    
                // Split BOTTOM
                ImGui::DockBuilderSplitNode(
                    dockspace_id, ImGuiDir_Down, 0.17f, &bottom, &main
                );

                ImGuiID left;
                ImGuiID right;

                // Split main into LEFT / RIGHT
                ImGui::DockBuilderSplitNode(
                    main, ImGuiDir_Right, 0.5f, &right, &left);

                ImGuiID right_left;
                ImGuiID right_right;

                // Split RIGHT into LEFT / RIGHT
                ImGui::DockBuilderSplitNode(
                    right, ImGuiDir_Right, 0.65f, &right_right, &right_left);

                ImGui::DockBuilderDockWindow("ImRisc", left);
                ImGui::DockBuilderDockWindow("Registers", right_left);
                ImGui::DockBuilderDockWindow("History", right_right);
                ImGui::DockBuilderDockWindow("Memory", bottom);

                ImGui::DockBuilderFinish(dockspace_id);
            }

            ImGui::End();
        }

        // --- End Dock host ---
        {
            ImGui::Begin("ImRisc");
            ImGui::Text("ImRisc RV32IM Emulator");

#ifdef __EMSCRIPTEN__
            if (ImGui::Button("Load Program"))
            {
                EM_ASM({
                    var fileInput = document.createElement('input');
                    fileInput.type = 'file';
                    fileInput.accept = '.bin';
                    fileInput.onchange = function(e)
                    {
                        var file = e.target.files[0];
                        var reader = new FileReader();
                        reader.onload = function(event)
                        {
                            var data = new Uint8Array(event.target.result);
                            var size = data.byteLength;

                            // Get RAM pointer and size from C
                            var ramPtr = Module._getRAMPtr();
                            var ramSize = Module._getRAMSize();

                            if (size > ramSize)
                            {
                                console.error("File too large: " + size + " > " + ramSize);
                                return;
                            }

                            // Create view into WASM memory at RAM location
                            var wasmMemory =
                                new Uint8Array(Module.HEAPU8.buffer, ramPtr, size);
                            wasmMemory.set(data);

                            // Call C function to finalize loading
                            Module._loadProgramFromJS(size, ramPtr);

                            console.log("Program loaded: " + size + " bytes at 0x" +
                                        ramPtr.toString(16));
                        };
                        reader.readAsArrayBuffer(file);
                    };
                    fileInput.click();
                });
            }
#endif

            ImGui::Text("CPU Info:");
            ImGui::Text("  PC: %08x", m_stats.cpu_snapshot.pc);
            ImGui::Text("  Cycle Time: %.1f ns (mean)", m_stats.cpu_cycle_time_ns);
            ImGui::Text("  Clock: %.1f MHz (mean)", m_stats.cpu_clk_mhz);
            ImGui::Text("FrameBuffer:");
            ImGui::Image((void *)(intptr_t)fb_texture, ImVec2(640, 480));
            ImGui::End();
        }

        {
            ImGui::Begin("Registers", &show_another_window);
            {
                if(ImGui::BeginTable("CPURegisters", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
                {
                    ImGui::TableSetupColumn("Register Name");
                    ImGui::TableSetupColumn("Value");
                    ImGui::TableHeadersRow();

                    for (size_t i = 0; i < 32; i++)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("x%d (%s)", (int)i, reg_names[i]);
                        ImGui::TableNextColumn();
                        ImGui::Text("0x%08X", m_stats.cpu_snapshot.regs[i]);
                    }

                    ImGui::EndTable();
                }
            }
            ImGui::End();
        }

        {
            ImGui::Begin("History", &show_another_window);
            {
                if (ImGui::BeginTable("CPUExecutionHistory", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
                {
                    ImGui::TableSetupColumn("PC", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                    ImGui::TableSetupColumn("Instruction");
                    ImGui::TableHeadersRow();

                    for (size_t i = 0; i < DEBUG_HIST_SIZE; i++)
                    {
                        if (m_stats.history[i].valid == false)
                            break;

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("0x%08X", m_stats.history[i].cpu.pc);
                        ImGui::TableNextColumn();
                        ImGui::Text("%s",
                                    cpu_fmt_asm_internal(m_stats.history[i].op,
                                                         m_stats.history[i].params[0],
                                                         m_stats.history[i].params[1],
                                                         m_stats.history[i].params[2]));
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::End();
        }
        
        
        {
            ImGui::Begin("Memory");

            ImGui::Text("[EXPERIMENTAL] Contents of [RAM @ 0x%08x]", RAM_ADDR);
            
            static ImGuiTableFlags flags2 =  ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_ContextMenuInBody;
            if (ImGui::BeginTable("MemoryView", MEM_COLS+1, flags2))
            {
                ImGui::TableSetupScrollFreeze(0, 1);

                ImGui::TableSetupColumn("Address");
                for (int col = 0; col < MEM_COLS; col++)
                {
                    ImGui::TableSetupColumn(format("%02X", 4*col));
                }
                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin((int)MEM_TOTAL_ROWS);

                // Clipper
                while (clipper.Step())
                {
                    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                    {
                        int64_t base_byte = (int64_t)row * MEM_ROW_BYTES;
                        int64_t base_word = base_byte / MEM_WORD_SIZE;
                        ImGui::TableNextRow();

                        // Address column
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("0x%08llx", base_byte);

                        // Data columns
                        for (int col = 0; col < MEM_COLS; col++)
                        {
                            int64_t word_index = base_word + col;
                            if (word_index >= MEM_TOTAL_WORDS)
                                break;

                            ImGui::TableSetColumnIndex(col + 1);
                            uint32_t* ram_ptr = (uint32_t*)RAM;
                            ImGui::Text("%08X", ram_ptr[word_index]);
                        }
                    }
                }
                ImGui::EndTable();
            }
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(m_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w,
                     clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
    }
    return ret;
}

bool ImRiscGui::Exit()
{
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(m_window);
    glfwTerminate();
    return false;
}

void ImRiscGui::copyFrameBuffer(uint32_t *src_fb)
{
    memcpy(fb_image_data, src_fb, fb_width * fb_height * sizeof(uint32_t));

    glBindTexture(GL_TEXTURE_2D, fb_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fb_width, fb_height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, fb_image_data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void ImRiscGui::loadProgramData(uint8_t *data, size_t size)
{
    printf("[GUI] Loading program: %zu bytes\n", size);
    // This will be called from JavaScript when user uploads a file
    // The actual loading happens in main.cpp via extern "C"
}

ImRiscGui::~ImRiscGui() {}