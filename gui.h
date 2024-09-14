#pragma once

/* Code based on thirparty/imgui/examples/glfw-opengl3/main.cpp */

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

struct cpu_stats_t{
    uint32_t cpu_pc_addr;
    double cpu_cycle_time_ns;
    double cpu_clk_mhz;
};

class ImRiscGui
{
private:
    bool show_demo_window;
    bool show_another_window;
    ImVec4 clear_color;
    GLFWwindow* m_window;
    int fb_width;
    int fb_height;
    uint32_t* fb_image_data;
    GLuint fb_texture = 0;
    
public:
    cpu_stats_t m_stats;

    ImRiscGui(int Width = 320, int Height = 200);
    bool Init();
    bool Run();
    bool Exit();
    void copyFrameBuffer(uint32_t* src_fb);
    GLFWwindow* getWindow() {return m_window;}
    void (*key_callback)(GLFWwindow* window, int key, int scancode, int action, int mods);
    ~ImRiscGui();
};
