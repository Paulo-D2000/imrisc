#include "gui.h"
#include <algorithm>
#include <string>

ImRiscGui::ImRiscGui(int Width, int Height): show_demo_window(true), show_another_window(true),
clear_color(0.45f, 0.55f, 0.60f, 1.00f), m_window(NULL), fb_width(Width), fb_height(Height){
    fb_image_data = new uint32_t[fb_width * fb_height];
}

bool ImRiscGui::Init()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 0;

// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    // Create m_window with graphics context
    m_window = glfwCreateWindow(975, 735, "ImRisc-V GUI", NULL, NULL);
    if (m_window == nullptr) return 0;
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking

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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP); // Same

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fb_width, fb_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, fb_image_data);

    return true;
}

bool ImRiscGui::Run()
{
    bool ret = !glfwWindowShouldClose(m_window);
    if(ret){
        glfwPollEvents();
        glfwSetKeyCallback(m_window, key_callback);

        // Start the Dear ImGui frame
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Draw the dockspace
        {
            ImGui::Begin("ImRisc");
            ImGui::Text("ImRisc RV32IM Emulator");
            ImGui::Text("CPU Info:");
            ImGui::Text("  PC: %08x", m_stats.cpu_pc_addr);
            ImGui::Text("  Cycle Time: %.1f ns (mean)", m_stats.cpu_cycle_time_ns);
            ImGui::Text("  Clock: %.1f MHz (mean)", m_stats.cpu_clk_mhz);
            ImGui::Text("FrameBuffer:");
            ImGui::Image((void*)(intptr_t)fb_texture, ImVec2(640, 480));
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(m_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
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
    memcpy(fb_image_data, src_fb, fb_width*fb_height*sizeof(uint32_t));

    glBindTexture(GL_TEXTURE_2D, fb_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fb_width, fb_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, fb_image_data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

ImRiscGui::~ImRiscGui() {}