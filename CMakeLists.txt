cmake_minimum_required(VERSION 3.1)

IF(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0")
ENDIF(CMAKE_COMPILER_IS_GNUCC)

project(ImRisc VERSION 1.0 LANGUAGES C CXX)

set(EMULATOR_SRCS debugging.h cpu.h ram.h rom.h uart.h spi.h vga.h symbol_parser.h breakpoints.h)

#add_executable(${PROJECT_NAME} main.c ${EMULATOR_SRCS})

# ------- GUI APP ----------- #

# Find OPENGL
find_package(OpenGL REQUIRED)

# Some dirs
set(IMGUI_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/imgui)
set(GLFW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glfw)

# Compile & add GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "Build the GLFW example programs")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "Build the GLFW test programs")
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "Build the GLFW documentation")
set(GLFW_INSTALL OFF CACHE INTERNAL "Generate installation target")
add_subdirectory(${GLFW_DIR})

## Add includes
include_directories(
    thirdparty/imgui
    thirdparty/imgui/backends
)
    
# Add IMGUI & Backends
file(GLOB IMGUI_SRCS ${IMGUI_SRC_DIR}/*.cpp)
list(APPEND IMGUI_SRCS
    ${IMGUI_SRC_DIR}/backends/imgui_impl_opengl3.cpp
    ${IMGUI_SRC_DIR}/backends/imgui_impl_glfw.cpp)


# App srcs
file(GLOB APP_SRCS main.cpp gui.cpp)

# Main app
add_executable(ImRisc-gui main.cpp ${APP_SRCS} ${IMGUI_SRCS} ${EMULATOR_SRCS})

# Main libs link
target_link_libraries(ImRisc-gui glfw ${GLFW_LIBRARIES} ${OPENGL_LIBRARIES} atomic)

set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")