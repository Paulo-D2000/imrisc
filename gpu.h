#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "cpu.h"

/**
 * GPU INTERFACE
 * 
 */

// vertex: 3x int32
// triangle: 3x vertex
// 256 Command FIFO, 1x STATUS reg, Shared Memory VBO

// GPU Memory Map
#define GPU_ADDR 0x64000000
#define GpuRingBufferBase          0x0000
#define GpuRingBufferSize          0x0004
#define GpuRingBufferReadPtr       0x0008
#define GpuRingBufferWritePtr      0x000C
#define GpuStatusOffset            0x0010
#define GPU_SIZE 0x0014

// Command opcodes (32-bit headers)
// Format: [8-bit Opcode] [24-bit Count of following words]
#define CMD_NOP             0x00  // Body: none
#define CMD_SET_VBO         0x01  // Body: [Addr] [Stride] [Format] (16.16 fixed-point 3D)
#define CMD_DRAW            0x02  // Body: [VertexCount]
#define CMD_FENCE           0x03  // Body: [WriteAddress] [WriteValue]
#define CMD_SET_VERTEX_SHADER    0x04  // Body: [ShaderAddr] [ShaderSize]
#define CMD_SET_FRAGMENT_SHADER  0x05  // Body: [ShaderAddr] [ShaderSize]
#define CMD_SET_UNIFORM          0x06  // Body: [UniformID] [Value0] [Value1] [Value2] [Value3]
#define CMD_SET_UNIFORM_MAT4     0x07  // Body: [UniformID] [16 values]
#define CMD_SET_TEXTURE          0x08  // Body: [TextureUnit] [TextureAddr] [Width] [Height] [Format]
#define CMD_CLEAR                0x09  // Body: [ClearFlags] [Color_RGBA]

// Vertex buffer format
#define VBO_FORMAT_FIXED16_16_3D 0x0

// Uniform IDs
#define UNIFORM_MODEL_MATRIX      0
#define UNIFORM_VIEW_MATRIX       1
#define UNIFORM_PROJ_MATRIX       2
#define UNIFORM_TIME              3
#define UNIFORM_LIGHT_POS         4
#define UNIFORM_CAMERA_POS        5

// Clear flags
#define CLEAR_COLOR_BUFFER  (1 << 0)
#define CLEAR_DEPTH_BUFFER  (1 << 1)
#define CLEAR_STENCIL_BUFFER (1 << 2)

// Gpu MMIO Registers
static uint32_t gpuRingBufferBase = 0;
static uint32_t gpuRingBufferSize = 0;
static uint32_t gpuRingBufferReadPtr = 0;
static uint32_t gpuRingBufferWritePtr = 0;
static uint32_t gpuStatus = 0;

// Gpu Internal Registers
static uint32_t gpuVBOAddr = 0;
static uint32_t gpuVBOStride = 0;
static uint32_t gpuVBOFormat = 0;
static uint32_t gpuVertexShaderAddr = 0;
static uint32_t gpuVertexShaderSize = 0;
static uint32_t gpuFragmentShaderAddr = 0;
static uint32_t gpuFragmentShaderSize = 0;

typedef int32_t Matrix44i[4][4];

// Uniform storage
#define MAX_UNIFORMS 32
static int32_t gpuUniforms[MAX_UNIFORMS][4];  // Vec4s
static Matrix44i gpuUniformMatrices[8];     // Mat4x4s

// Texture state
static uint32_t gpuTextureAddr[8] = {0};
static uint32_t gpuTextureWidth[8] = {0};
static uint32_t gpuTextureHeight[8] = {0};
void gpu_clk();