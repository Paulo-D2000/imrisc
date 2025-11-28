#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * GPU INTERFACE
 * 
 */

// vertex: 3x int32
// triangle: 3x vertex
// 512 points (170 vert, 56 tri) queue @ GPU_ADDR + 0x0000

#define GPU_ADDR 0x64000000
#define VertexQueueOffset  0x0000
#define VertexSizeOffset   0x1000
#define GpuBusyOffset      0x1004
#define GpuCommitOffset    0x1008
#define GPU_SIZE 0x100C

static uint32_t gpuBusy = 0;
static int32_t  gpuVertexQ[512] = {0};
static uint32_t gpuVertexSz = 0;
static uint32_t gpuCommit = 0;
void gpu_clk();