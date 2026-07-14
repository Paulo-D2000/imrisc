#ifndef GUI_H
#define GUI_H

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
#include "debugging.h"
#include "isa.h"
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

static void glfw_error_callback(int error, const char *description) {
  fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

struct cpu_stats_t {
  CPU cpu_snapshot;
  cpu_history_t history[DEBUG_HIST_SIZE];
  size_t history_ptr;
  double cpu_cycle_time_ns;
  double cpu_clk_mhz;
};

class ImRiscGui {
private:
  bool show_demo_window;
  bool show_another_window;
  ImVec4 clear_color;
  GLFWwindow *m_window;
  int fb_width;
  int fb_height;
  uint32_t *fb_image_data;
  GLuint fb_texture = 0;

public:
  cpu_stats_t m_stats;
  static constexpr int MEM_COLS = 16;
  static constexpr int MEM_WORD_SIZE = 4;
  static constexpr int MEM_ROW_BYTES = MEM_COLS * MEM_WORD_SIZE;
  static constexpr int64_t MEM_SIZE_BYTES = RAM_SIZE;
  static constexpr int64_t MEM_TOTAL_WORDS = MEM_SIZE_BYTES / MEM_WORD_SIZE;
  static constexpr int64_t MEM_TOTAL_ROWS  = MEM_SIZE_BYTES / MEM_ROW_BYTES;
  static constexpr int64_t MEM_ADDR_BASE = RAM_ADDR;
  ImRiscGui(int Width = 320, int Height = 200);
  bool Init();
  bool Run();
  bool Exit();
  void copyFrameBuffer(uint32_t *src_fb);
  void loadProgramData(uint8_t *data, size_t size);
  GLFWwindow *getWindow() { return m_window; }
  void (*key_callback)(GLFWwindow *window, int key, int scancode, int action,
                       int mods);
  ~ImRiscGui();
};

#endif // GUI_H