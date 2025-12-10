#include "gui.h"

//#define DEBUG_REGS
//#define DEBUG_ASM
#define DEBUG_HIST_SIZE 1
//#define BREAKPOINTS_ENABLED
#include "debugging.h"
#include "rom.h"
#include "cpu.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h> 
#include <math.h>
#include <atomic>
#include <queue>
#include <thread>
#include <mutex>

//#define DOOM
#define GPU_LOG 1

static uint32_t rgb_buffer[FRAMESIZE];

void vga_get_buffer(){
    for (size_t i = 0; i < FRAMESIZE; i++)
    {
#ifdef DOOM
            // pallete mode
            uint32_t pal_addr = 4 * VGA[i + ScreenAddressOffset] + PaletteAddressOffset;
            uint8_t r = VGA[pal_addr + 0];
            uint8_t g = VGA[pal_addr + 1];
            uint8_t b = VGA[pal_addr + 2];
            uint8_t a = VGA[pal_addr + 3];
            rgb_buffer[i] = (a << 24) | (b << 16) | (g << 8) | r;
#else
            // direct mode
            //uint8_t r = VGA[ScreenAddressOffset + 4*i + 0];
            //uint8_t g = VGA[ScreenAddressOffset + 4*i + 1];
            //uint8_t b = VGA[ScreenAddressOffset + 4*i + 2];
            //uint8_t a = VGA[ScreenAddressOffset + 4*i + 3];
            //rgb_buffer[i] = (a << 24) | (b << 16) | (g << 8) | r;
#endif
    }
}

void dma_controller(){
    if(!dmaCommit && dmaBusy){
        //printf("[DMA] Transfering %d bytes from [%08x:%08x] -> [%08x:%08x]\n", dmaTransferSz, dmaSrcAddr, dmaSrcAddr+dmaTransferSz, dmaDstAddr, dmaDstAddr+dmaTransferSz);
        for (size_t i = 0; i < dmaTransferSz; i+=4)
        {
            mmio_result_t res = mmio_read(dmaSrcAddr+i, _WORD);
            res = mmio_write_n(dmaDstAddr+i, mmio_to_word(res), _WORD);
        }
        dmaBusy = 0;
    }
    if(dmaCommit && !dmaBusy){
        dmaBusy = 1;
        dmaCommit = 0;
    }
}

#define MIN(A,B) ((A) < (B) ? (A) : (B))
#define MAX(A,B) ((A) > (B) ? (A) : (B))

#define ONE (1<<16)
#define HALF (1<<15)
#define FIX_MAKE(INT,FRAC) (((int32_t)(INT) << 16) | (FRAC))
static inline int32_t fix16_mul(int32_t x, int32_t y)
{
    int64_t p = (int64_t)x * (int64_t)y;

    // add/sub half for rounding
    if (p >= 0) p += (1LL << 15);
    else        p -= (1LL << 15);

    p >>= 16;

    // saturate to int32_t
    if (p > INT32_MAX) return INT32_MAX;
    if (p < INT32_MIN) return INT32_MIN;

    return (int32_t)p;
}

#define MUL(X,Y) fix16_mul((X),(Y))

static inline int32_t fix16_div(int32_t x, int32_t y)
{
    if (y == 0)
        return (x >= 0) ? INT32_MAX : INT32_MIN;

    int64_t num = (int64_t)x << 16;

    int64_t aby = (y >= 0 ? y : -y);
    int64_t half = aby >> 1;

    // apply rounding based on sign of division result
    if ((num >= 0 && y >= 0) || (num < 0 && y < 0))
        num += half;
    else
        num -= half;

    int64_t r = num / y;

    // saturate
    if (r > INT32_MAX) return INT32_MAX;
    if (r < INT32_MIN) return INT32_MIN;

    return (int32_t)r;
}

#define DIV(X,Y) fix16_div((X),(Y))

typedef struct Vec3i{
    int32_t e[3];
}Vec3i;

int32_t edge_cross(Vec3i a, Vec3i b, Vec3i p){
    Vec3i ab = {{b.e[0] - a.e[0], b.e[1] - a.e[1]}};
    Vec3i ap = {{p.e[0] - a.e[0], p.e[1] - a.e[1]}};
    return (ab.e[0] * ap.e[1]) - (ab.e[1] * ap.e[0]);
}

void multPointMatrix(Vec3i in, Vec3i* out, Matrix44i M) 
{ 
    //out = in * M;
    out->e[0]   = MUL(in.e[0], M[0][0]) + MUL(in.e[1], M[1][0]) + MUL(in.e[2], M[2][0]) + M[3][0]; 
    out->e[1]   = MUL(in.e[0], M[0][1]) + MUL(in.e[1], M[1][1]) + MUL(in.e[2], M[2][1]) + M[3][1]; 
    out->e[2]   = MUL(in.e[0], M[0][2]) + MUL(in.e[1], M[1][2]) + MUL(in.e[2], M[2][2]) + M[3][2]; 
    int w    = MUL(in.e[0], M[0][3]) + MUL(in.e[1], M[1][3]) + MUL(in.e[2], M[2][3]) + M[3][3]; 

    // normalize if w is different than 1 (convert from homogeneous to Cartesian coordinates)
    if (w != ONE && w != 0) { 
        out->e[0] = DIV(out->e[0], w); 
        out->e[1] = DIV(out->e[1], w); 
        out->e[2] = DIV(out->e[2], w);
    } 
}

void transform_vertex(Vec3i in, Vec3i* out, 
                      Matrix44i modelMatrix, 
                      Matrix44i viewMatrix, 
                      Matrix44i projMatrix){
    // Reset out...
    out->e[0] = 0;
    out->e[1] = 0;
    out->e[2] = 0;
    
    Vec3i vertModel = {0, 0, 0};
    Vec3i vertView = {0, 0, 0};
    Vec3i projectedVert  = {0, 0, 0};
    multPointMatrix(in, &vertModel, modelMatrix); 
    multPointMatrix(vertModel, &vertView, viewMatrix); 
    multPointMatrix(vertView, &projectedVert, projMatrix); 
    if (projectedVert.e[0] < -ONE || projectedVert.e[0] > ONE|| projectedVert.e[1] < -ONE || projectedVert.e[1] > ONE) return; 
                    
    // convert to raster space and mark the position of the vertex in the image with a simple dot
    out->e[0] = MIN(SCREENWIDTH-1, MAX(0, MUL(SCREENWIDTH,  MUL(HALF, ONE + projectedVert.e[0]))));
    out->e[1] = MIN(SCREENHEIGHT-1, MAX(0, MUL(SCREENHEIGHT, MUL(HALF, ONE + projectedVert.e[1]))));
    out->e[2] = MIN(ONE, MAX(-ONE, projectedVert.e[2]));
}

static int32_t zbuffer[FRAMESIZE];
static uint8_t back_fb[FRAMESIZE*4];
static uint8_t front_fb[FRAMESIZE*4];

static volatile uint8_t* fb0 = back_fb;
static volatile uint8_t* fb1 = front_fb;

void gpu_clk(){
    // Validate ring buffer configuration
    if(gpuRingBufferSize == 0) return;
    
    if(gpuRingBufferReadPtr != gpuRingBufferWritePtr){
        // get a command (MMIO read, can fail, etc)
        uint32_t cmd_addr = gpuRingBufferBase + gpuRingBufferReadPtr;
        mmio_result_t res = mmio_read(cmd_addr, _WORD);
        
        if(!res.valid){
            if(GPU_LOG) printf("[GPU] ERROR: Failed to read command at %08x\n", cmd_addr);
            return; // Don't advance pointer on read failure
        }
        
        uint32_t cmd = mmio_to_word(res);
        uint8_t opcode = (cmd >> 24) & 0xFF;
        uint32_t count = cmd & 0x00FFFFFF;
        
        if(opcode != 0 && GPU_LOG) printf("[GPU] CMD: %02x, COUNT: %d\n", opcode, count);
        
         // Advance read pointer with BITMASK (fast!)
        uint32_t mask = gpuRingBufferSize - 1;
        gpuRingBufferReadPtr = (gpuRingBufferReadPtr + 4) & mask;

        switch(opcode){
            case CMD_NOP:
                // do nothing
                break;
            
            case CMD_SET_VBO:
                {
                    // read 3 params
                    uint32_t params[3];
                    for (size_t i = 0; i < 3; i++)
                    {
                        uint32_t param_addr = gpuRingBufferBase + gpuRingBufferReadPtr;
                        res = mmio_read(param_addr, _WORD);
                        if(!res.valid){
                            if(GPU_LOG) printf("[GPU] ERROR: Failed to read VBO param %ld at %08x\n", i, param_addr);
                            return; // Don't advance on failure
                        }
                        params[i] = mmio_to_word(res);
                        gpuRingBufferReadPtr = (gpuRingBufferReadPtr + 4) % gpuRingBufferSize;
                    }
                    gpuVBOAddr = params[0];
                    gpuVBOStride = params[1];
                    gpuVBOFormat = params[2];
                    if(GPU_LOG) printf("[GPU] SET_VBO Addr: %08x, Stride: %d, Format: %08x\n", 
                                      gpuVBOAddr, gpuVBOStride, gpuVBOFormat);
                }
                break;

            case CMD_DRAW:
                {
                    // read 1 param
                    uint32_t vertexCount = 0;
                    {
                        uint32_t param_addr = gpuRingBufferBase + gpuRingBufferReadPtr;
                        res = mmio_read(param_addr, _WORD);
                        if(!res.valid){
                            if(GPU_LOG) printf("[GPU] ERROR: Failed to read DRAW param at %08x\n", param_addr);
                            return;
                        }
                        vertexCount = mmio_to_word(res);
                        gpuRingBufferReadPtr = (gpuRingBufferReadPtr + 4) % gpuRingBufferSize;
                    }
                    if(GPU_LOG) printf("[GPU] DRAW VertexCount: %d\n", vertexCount);

                    // COPY Matrice44i from uniforms static Matrix44i gpuUniformMatrices[8];
                    Matrix44i modelMatrix, viewMatrix, projMatrix;
                    memcpy(modelMatrix, gpuUniformMatrices[UNIFORM_MODEL_MATRIX], sizeof(Matrix44i));
                    memcpy(viewMatrix, gpuUniformMatrices[UNIFORM_VIEW_MATRIX], sizeof(Matrix44i));
                    memcpy(projMatrix, gpuUniformMatrices[UNIFORM_PROJ_MATRIX], sizeof(Matrix44i));
                    
                    // Read time from uniform
                    int32_t time = gpuUniforms[UNIFORM_TIME][0];

                    // Draw triangles (process every 3 vertices as one triangle)
                    for (size_t tri = 0; tri < vertexCount; tri += 3)
                    {
                        // Load 3 complete vertices
                        Vec3i v0, v1, v2;

                        // Vertex 0
                        uint32_t v0_addr = gpuVBOAddr + (tri * gpuVBOStride);
                        res = mmio_read(v0_addr + 0, _WORD);
                        if(res.valid) v0.e[0] = mmio_to_word(res);
                        else if(GPU_LOG) printf("[GPU] ERROR: Failed to read v0.x\n");
                        
                        res = mmio_read(v0_addr + 4, _WORD);
                        if(res.valid) v0.e[1] = mmio_to_word(res);
                        else if(GPU_LOG) printf("[GPU] ERROR: Failed to read v0.y\n");
                        
                        res = mmio_read(v0_addr + 8, _WORD);
                        if(res.valid) v0.e[2] = mmio_to_word(res);
                        else if(GPU_LOG) printf("[GPU] ERROR: Failed to read v0.z\n");
                        
                        // Vertex 1
                        uint32_t v1_addr = gpuVBOAddr + ((tri + 1) * gpuVBOStride);
                        res = mmio_read(v1_addr + 0, _WORD);
                        if(res.valid) v1.e[0] = mmio_to_word(res);
                        else if(GPU_LOG) printf("[GPU] ERROR: Failed to read v1.x\n");
                        
                        res = mmio_read(v1_addr + 4, _WORD);
                        if(res.valid) v1.e[1] = mmio_to_word(res);
                        else if(GPU_LOG) printf("[GPU] ERROR: Failed to read v1.y\n");
                        
                        res = mmio_read(v1_addr + 8, _WORD);
                        if(res.valid) v1.e[2] = mmio_to_word(res);
                        else if(GPU_LOG) printf("[GPU] ERROR: Failed to read v1.z\n");
                        
                        // Vertex 2
                        uint32_t v2_addr = gpuVBOAddr + ((tri + 2) * gpuVBOStride);
                        res = mmio_read(v2_addr + 0, _WORD);
                        if(res.valid) v2.e[0] = mmio_to_word(res);
                        else if(GPU_LOG) printf("[GPU] ERROR: Failed to read v2.x\n");
                        
                        res = mmio_read(v2_addr + 4, _WORD);
                        if(res.valid) v2.e[1] = mmio_to_word(res);
                        else if(GPU_LOG) printf("[GPU] ERROR: Failed to read v2.y\n");
                        
                        res = mmio_read(v2_addr + 8, _WORD);
                        if(res.valid) v2.e[2] = mmio_to_word(res);
                        else if(GPU_LOG) printf("[GPU] ERROR: Failed to read v2.z\n");

                        // Transform
                        Vec3i tmp_v0, tmp_v1, tmp_v2;
                        transform_vertex(v0, &tmp_v0, modelMatrix, viewMatrix, projMatrix);
                        transform_vertex(v1, &tmp_v1, modelMatrix, viewMatrix, projMatrix);
                        transform_vertex(v2, &tmp_v2, modelMatrix, viewMatrix, projMatrix);
                        v0 = tmp_v0; v1 = tmp_v1; v2 = tmp_v2;

                        // bounding box
                        int32_t x_min = MIN(v0.e[0], MIN(v1.e[0], v2.e[0]));
                        int32_t y_min = MIN(v0.e[1], MIN(v1.e[1], v2.e[1]));
                        int32_t x_max = MAX(v0.e[0], MAX(v1.e[0], v2.e[0]));
                        int32_t y_max = MAX(v0.e[1], MAX(v1.e[1], v2.e[1]));

                        int32_t area = edge_cross(v0, v1, v2);
                        
                        for (int y = y_min; y < y_max; y++){
                            for (int x = x_min; x < x_max; x++){
                                Vec3i p = {{x,y,0}};
                    
                                int32_t w0 = edge_cross(v1, v2, p);
                                int32_t w1 = edge_cross(v2, v0, p);
                                int32_t w2 = edge_cross(v0, v1, p);

                                if(w0 >= 0 && w1 >= 0 && w2 >= 0){
                                    w0 = DIV(w0, area),w1 = DIV(w1, area),w2 = DIV(w2, area);
                                    int32_t z = MUL(v0.e[2], w0) + MUL(v1.e[2], w1) + MUL(v2.e[2], w2);
                                    
                                    uint32_t ptr = (x+y*SCREENWIDTH);
                                    if(z < zbuffer[ptr]){
                                        zbuffer[ptr] = z;
                                        if(1){
                                            fb0[4*ptr + 0] = w0 >> 8;
                                            fb0[4*ptr + 1] = w1 >> 8;
                                            fb0[4*ptr + 2] = w2 >> 8;
                                            fb0[4*ptr + 3] = 0xFF;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    // copy data to vga
                    memcpy((void*)rgb_buffer, (void*)fb0, 4*FRAMESIZE);

                    // swap fb's
                    volatile uint8_t* temp = fb0;
                    fb0 = fb1;
                    fb1 = temp;

                    if(GPU_LOG) printf("[GPU] Done!\n");
                }
                break;

            case CMD_FENCE:
                {
                    // read 2 params
                    uint32_t fenceParams[2];
                    for (size_t i = 0; i < 2; i++)
                    {
                        uint32_t param_addr = gpuRingBufferBase + gpuRingBufferReadPtr;
                        res = mmio_read(param_addr, _WORD);
                        if(!res.valid){
                            if(GPU_LOG) printf("[GPU] ERROR: Failed to read FENCE param %ld at %08x\n", i, param_addr);
                            return;
                        }
                        fenceParams[i] = mmio_to_word(res);
                        gpuRingBufferReadPtr = (gpuRingBufferReadPtr + 4) % gpuRingBufferSize;
                    }
                    if(GPU_LOG) printf("[GPU] FENCE WriteAddr: %08x, WriteValue: %08x\n", fenceParams[0], fenceParams[1]);
                    // write value to addr
                    mmio_write_n(fenceParams[0], fenceParams[1], _WORD);
                }
                break;

            case CMD_SET_VERTEX_SHADER:
            {
                uint32_t params[2];
                for (size_t i = 0; i < 2; i++) {
                    uint32_t param_addr = gpuRingBufferBase + gpuRingBufferReadPtr;
                    res = mmio_read(param_addr, _WORD);
                    if(!res.valid) return;
                    params[i] = mmio_to_word(res);
                    gpuRingBufferReadPtr = (gpuRingBufferReadPtr + 4) & mask;
                }
                gpuVertexShaderAddr = params[0];
                gpuVertexShaderSize = params[1];
                if(GPU_LOG) printf("[GPU] SET_VERTEX_SHADER Addr: %08x, Size: %d\n", 
                                  gpuVertexShaderAddr, gpuVertexShaderSize);
            }
            break;
        
        case CMD_SET_FRAGMENT_SHADER:
            {
                uint32_t params[2];
                for (size_t i = 0; i < 2; i++) {
                    uint32_t param_addr = gpuRingBufferBase + gpuRingBufferReadPtr;
                    res = mmio_read(param_addr, _WORD);
                    if(!res.valid) return;
                    params[i] = mmio_to_word(res);
                    gpuRingBufferReadPtr = (gpuRingBufferReadPtr + 4) & mask;
                }
                gpuFragmentShaderAddr = params[0];
                gpuFragmentShaderSize = params[1];
                if(GPU_LOG) printf("[GPU] SET_FRAGMENT_SHADER Addr: %08x, Size: %d\n", 
                                  gpuFragmentShaderAddr, gpuFragmentShaderSize);
            }
            break;
        
        case CMD_SET_UNIFORM:
            {
                uint32_t params[5];
                for (size_t i = 0; i < 5; i++) {
                    uint32_t param_addr = gpuRingBufferBase + gpuRingBufferReadPtr;
                    res = mmio_read(param_addr, _WORD);
                    if(!res.valid) return;
                    params[i] = mmio_to_word(res);
                    gpuRingBufferReadPtr = (gpuRingBufferReadPtr + 4) & mask;
                }
                uint32_t uniform_id = params[0];
                if(uniform_id < MAX_UNIFORMS) {
                    gpuUniforms[uniform_id][0] = params[1];
                    gpuUniforms[uniform_id][1] = params[2];
                    gpuUniforms[uniform_id][2] = params[3];
                    gpuUniforms[uniform_id][3] = params[4];
                    if(GPU_LOG) printf("[GPU] SET_UNIFORM id=%d, values=(%d,%d,%d,%d)\n", 
                                      uniform_id, params[1], params[2], params[3], params[4]);
                }
            }
            break;
        
        case CMD_SET_UNIFORM_MAT4:
            {
                uint32_t uniform_id;
                {
                    uint32_t param_addr = gpuRingBufferBase + gpuRingBufferReadPtr;
                    res = mmio_read(param_addr, _WORD);
                    if(!res.valid) return;
                    uniform_id = mmio_to_word(res);
                    gpuRingBufferReadPtr = (gpuRingBufferReadPtr + 4) & mask;
                }
                
                if(uniform_id < 8) {  // Max 8 mat4 uniforms
                    for (size_t i = 0; i < 16; i++) {
                        uint32_t param_addr = gpuRingBufferBase + gpuRingBufferReadPtr;
                        res = mmio_read(param_addr, _WORD);
                        if(!res.valid) return;
                        gpuUniformMatrices[uniform_id][i/4][i%4] = mmio_to_word(res);
                        gpuRingBufferReadPtr = (gpuRingBufferReadPtr + 4) & mask;
                    }
                    if(GPU_LOG) printf("[GPU] SET_UNIFORM_MAT4 id=%d\n", uniform_id);

                }
            }
            break;
        
        case CMD_SET_TEXTURE:
            {
                uint32_t params[4];
                for (size_t i = 0; i < 4; i++) {
                    uint32_t param_addr = gpuRingBufferBase + gpuRingBufferReadPtr;
                    res = mmio_read(param_addr, _WORD);
                    if(!res.valid) return;
                    params[i] = mmio_to_word(res);
                    gpuRingBufferReadPtr = (gpuRingBufferReadPtr + 4) & mask;
                }
                uint32_t unit = params[0];
                if(unit < 8) {
                    gpuTextureAddr[unit] = params[1];
                    gpuTextureWidth[unit] = params[2];
                    gpuTextureHeight[unit] = params[3];
                    if(GPU_LOG) printf("[GPU] SET_TEXTURE unit=%d, addr=%08x, size=%dx%d\n", 
                                      unit, params[1], params[2], params[3]);
                }
            }
            break;
        
        case CMD_CLEAR:
            {
                uint32_t params[2];
                for (size_t i = 0; i < 2; i++) {
                    uint32_t param_addr = gpuRingBufferBase + gpuRingBufferReadPtr;
                    res = mmio_read(param_addr, _WORD);
                    if(!res.valid) return;
                    params[i] = mmio_to_word(res);
                    gpuRingBufferReadPtr = (gpuRingBufferReadPtr + 4) & mask;
                }
                
                uint32_t flags = params[0];
                uint32_t color = params[1];
                
                if(GPU_LOG) printf("[GPU] CLEAR flags=%08x, color=%08x\n", flags, color);
                
                if(flags & CLEAR_COLOR_BUFFER) {
                    uint8_t r = (color >> 0) & 0xFF;
                    uint8_t g = (color >> 8) & 0xFF;
                    uint8_t b = (color >> 16) & 0xFF;
                    uint8_t a = (color >> 24) & 0xFF;
                    for (size_t i = 0; i < FRAMESIZE; i++) {
                        fb0[4*i + 0] = r;
                        fb0[4*i + 1] = g;
                        fb0[4*i + 2] = b;
                        fb0[4*i + 3] = a;
                    }
                }
                
                if(flags & CLEAR_DEPTH_BUFFER) {
                    for (size_t i = 0; i < FRAMESIZE; i++) {
                        zbuffer[i] = INT32_MAX;
                    }
                }
            }
            break;
            
            
            default:
                if(GPU_LOG) printf("[GPU] Unknown Opcode: %02x\n", opcode);
                // skip data with proper wraparound
                gpuRingBufferReadPtr = (gpuRingBufferReadPtr + count * 4) % gpuRingBufferSize;
                break;
        }
    }
}

void vga_start(){
    printf("vga_start\n");
    for (size_t i = 0; i < 256; i++)
    {
        VGA[PaletteAddressOffset + 4*i + 0] = i;
        VGA[PaletteAddressOffset + 4*i + 1] = i;
        VGA[PaletteAddressOffset + 4*i + 2] = i;
        VGA[PaletteAddressOffset + 4*i + 3] = 0xFF;
	}
    vga_frameCount = 0;
    vga_vblank = 1;
}


long hex_load(const char* path, uint8_t* ptr, size_t max_size){
    FILE* filePointer;
    int bufferLength = 255;
    char buffer[bufferLength]; /* not ISO 90 compatible */
    long i = 0;
    filePointer = fopen(path, "r");

    if(!filePointer){
        return -1;
    }

    while(fgets(buffer, bufferLength, filePointer)) {
        size_t len = strlen(buffer);
        if(buffer[len-1] == '\n') buffer[len-1] = '\0';

        if((i+1) < max_size){
            uint32_t* prog = (uint32_t*)ptr;
            prog[i] = (uint32_t)strtoul(buffer, NULL, 16);
            printf("ROM [0x%04lx]: 0x%08x Loading program...\n", i, prog[i]);
            i++;
        }else{
            printf("Error! file \"%s\" is to big! %luB > %ldB\n", path, i+1, max_size);
            fclose(filePointer);
            return -1;
        }
    }

    fclose(filePointer);
    return i;
}


long file_load(const char* path, uint8_t* ptr, size_t max_size){
    long i = 0;
    FILE* filePointer = fopen(path, "rb");

    if(!filePointer){
        return -1;
    }

    fseek(filePointer, 0, SEEK_END);
	i = ftell(filePointer);
	fseek(filePointer, 0, SEEK_SET);

    if((i+1) < max_size){
        int ret = fread(ptr, i, 1, filePointer);
    }else{
        printf("Error! file \"%s\" is to big! %ldB > %luB\n", path, i+1, max_size);
        fclose(filePointer);
        return -1;
    }
    fclose(filePointer);
    return i;
}

long file_load_alloc(const char* path, uint8_t** ptr, size_t* alloc_size){
    long i = 0;
    FILE* filePointer = fopen(path, "rb");

    if(!filePointer){
        return -1;
    }

    // find filesize
    fseek(filePointer, 0, SEEK_END);
	*alloc_size = i = ftell(filePointer);
	fseek(filePointer, 0, SEEK_SET);

    //alloc & read
    *ptr = (uint8_t*)malloc(*alloc_size);
    int ret = fread(*ptr, i, 1, filePointer);

    // close
    fclose(filePointer);
    return i;
}

#ifndef BOOTADDR
#define BOOTADDR BRAM_ADDR
#endif

void LoadDoom()
{
    memset(VGA, 0, VGA_SIZE);
    memset(ROM, 0, ROM_SIZE);
    memset(RAM, 0, RAM_SIZE);
    memset(BRAM, 0, BRAM_SIZE);
#ifdef DOOM
    const char* bootloader_path = "../boot.bin";
    const char* doom_wad_path = "../DOOM.WAD";
    const char* doom_path = "../doom-riscv.bin";
    const char* doom_syms_path = "../dump.Sym";
    
    printf("Trying to load doom...\n");
    if(file_load(bootloader_path, &BRAM[0], BRAM_SIZE) == -1){
        exit(-1);
    }
    printf("Loaded %s to %08x\n", bootloader_path, BRAM_ADDR);
   
    if(file_load_alloc(doom_wad_path, &WAD_ROM, &WAD_ROM_SIZE) == -1){
        exit(-1);
    }
    printf("Loaded %s to %08x\n", doom_wad_path, WAD_ROM_ADDR);

    if(WAD_ROM_SIZE < 12408292){
        uint8_t* new_wad = new uint8_t[12408292];
        memcpy(new_wad, WAD_ROM, WAD_ROM_SIZE);
        delete WAD_ROM;
        WAD_ROM = new_wad;
        WAD_ROM_SIZE = 12408292;
    }

    if(file_load(doom_path, &ROM[0], ROM_SIZE) == -1){
        exit(-1);
    }
    printf("Loaded %s to %08x\n", doom_path, ROM_ADDR);

    printf("Trying to load symbols...\n");
    long tab_sz = load_symbols(doom_syms_path);
    if(tab_sz == -1){
        printf("[ERROR] Failed to load symbols!\n");
        exit(-1);
    }
# else
    //BREAKPOINTS[ACTIVE_BREAKPOINTS].address = 0x40100184;
    //BREAKPOINTS[ACTIVE_BREAKPOINTS].enabled = true;
    //ACTIVE_BREAKPOINTS++;
    
    //long result = hex_load("../helloworld.rom", ROM, ROM_SIZE);
    long result = file_load("../program.bin", RAM, RAM_SIZE);
    #define BOOTADDR RAM_ADDR

    //printf("Trying to load symbols...\n");
    //long tab_sz = load_symbols("../helloworld.sym");
    //if(tab_sz == -1){
    //    printf("[ERROR] Failed to load symbols!\n");
    //    exit(-1);
    //}
#endif
    vga_start();

    //SYMBOL_TABLE_SIZE = tab_sz;
}

struct key_event_t
{
    int key;
    int action;
};


std::queue<key_event_t> eventQueue;
std::mutex queueMutex;

std::atomic<bool> update_stats = false;
std::atomic<bool> vga_inc = false;
std::atomic<bool> kill = false;
std::atomic<bool> kready = false;
std::atomic<uint32_t> kcode = 0xff;
std::atomic<cpu_stats_t> cpu_stats;

#define D_KEY_LEFTARROW 0
#define D_KEY_RIGHTARROW 1
#define D_KEY_DOWNARROW 2
#define D_KEY_UPARROW 3
#define D_KEY_RSHIFT 4
#define D_KEY_RCTRL 5
#define D_KEY_RALT 6
#define D_KEY_ESCAPE 7
#define D_KEY_ENTER 8
#define D_KEY_TAB 9
#define D_KEY_BACKSPACE 10
#define D_KEY_PAUSE 11
#define D_KEY_EQUALS 12
#define D_KEY_MINUS 13
#define D_KEY_F1 14
#define D_KEY_F2 15
#define D_KEY_F3 16
#define D_KEY_F4 17
#define D_KEY_F5 18
#define D_KEY_F6 19
#define D_KEY_F7 20
#define D_KEY_F8 21
#define D_KEY_F9 22
#define D_KEY_F10 23
#define D_KEY_F11 24
#define D_KEY_F12 25

void rv32i_threads(){
    CPU rv32i[NCPUS];

    // mhartid 0xF14 Hardware thread ID.
    for (size_t i = 0; i < NCPUS; i++)
    {
        rv32i[i].CSRs[misa] = 0b01000000000000000001000100000000;
        rv32i[i].CSRs[mvendorid] = 0;
        rv32i[i].CSRs[marchid] = 0;
        rv32i[i].CSRs[mimpid] = 0;
        rv32i[i].CSRs[mhartid] = i;
        rv32i[i].CSRs[mstatus] = 0;
        rv32i[i].CSRs[mtvec] = 0x40100000;
        rv32i[i].CSRs[mie] = 0x00000888;
        rv32i[i].CSRs[mip] = 0;
        rv32i[i].CSRs[mcause] = 0;
        rv32i[i].CSRs[mepc] = 0;
        rv32i[i].CSRs[mscratch] = 0;
        rv32i[i].CSRs[mtval] = 0;
        rv32i[i].pc = BOOTADDR;
    }

    size_t cycles = 0;
    Timer tim;
    double average = 0.0;
    timer_start(&tim);
    bool ret = true;
    
    while(ret && !kill){
        static bool reg = true;

        if(cycles % 10000 == 0){
            timer_end(&tim);
            double time_taken = tim.duration; // in seconds 
            average += time_taken;
            timer_start(&tim);
        }

        if(cycles == 10000000){
            cpu_stats_t newstats;
            newstats.cpu_cycle_time_ns = average / cycles;
            newstats.cpu_clk_mhz = 1e3 / newstats.cpu_cycle_time_ns;
            newstats.cpu_pc_addr = rv32i[0].pc;
            cpu_stats.store(newstats);
            update_stats.store(true);

            average = 0.0;
            cycles = 0;
        }

        if(vga_inc.load()){
            vga_vblank = 0;
            if(reg){
                vga_frameCount++;
                reg = false;
            }
        }else{
            vga_vblank = 1;
            reg = true;
        }
        
        if(kready){
            UART_KEY = kcode;
            kready = 0;
        }

        if(!(UART_KEY & 0x80000000)){
            if(uart_rx){
                UART_KEY = -1;
                uart_rx = false;
            }
        }
        else {
            if(uart_rx){
                UART_KEY = -1;
                uart_rx = false;
            }
        }
        
        ret = 1;
        if(!dmaBusy)
            for (size_t i = 0; i < NCPUS; i++)
                ret &= cpu_cycle(&rv32i[i]);
        
#ifndef DOOM
        dma_controller();

        gpu_clk();
#endif

        uart_printf();
        if(!dmaBusy)
            cycles++;
    }
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods){
    std::lock_guard<std::mutex> lock(queueMutex);
    unsigned char K = 0XFF;
    switch (key)
    {
    case GLFW_KEY_RIGHT:
        K = D_KEY_RIGHTARROW  ;
        break;
    case GLFW_KEY_LEFT:
        K = D_KEY_LEFTARROW   ;
        break;
    case GLFW_KEY_UP:
        K = D_KEY_UPARROW     ;
        break;
    case GLFW_KEY_DOWN:
        K = D_KEY_DOWNARROW   ;
        break;
    case GLFW_KEY_ESCAPE:
        K = D_KEY_ESCAPE      ;
        break;
    case GLFW_KEY_ENTER:
        K = D_KEY_ENTER       ;
        break;
    case GLFW_KEY_TAB:
        K = D_KEY_TAB         ;
        break;
    case GLFW_KEY_F1:
        K = D_KEY_F1          ;
        break;
    case GLFW_KEY_F2:
        K = D_KEY_F2          ;
        break;
    case GLFW_KEY_F3:
        K = D_KEY_F3          ;
        break;
    case GLFW_KEY_F4:
        K = D_KEY_F4          ;
        break;
    case GLFW_KEY_F5:
        K = D_KEY_F5          ;
        break;
    case GLFW_KEY_F6:
        K = D_KEY_F6          ;
        break;
    case GLFW_KEY_F7:
        K = D_KEY_F7          ;
        break;
    case GLFW_KEY_F8:
        K = D_KEY_F8          ;
        break;
    case GLFW_KEY_F9:
        K = D_KEY_F9          ;
        break;
    case GLFW_KEY_F10:
        K = D_KEY_F10         ;
        break;
    case GLFW_KEY_F11:
        K = D_KEY_F11         ;
        break;
    case GLFW_KEY_F12:
        K = D_KEY_F12         ;
        break;
    case GLFW_KEY_BACKSPACE:
        K = D_KEY_BACKSPACE   ;
        break;
    case GLFW_KEY_PAUSE:
        K = D_KEY_PAUSE       ;
        break;
    case GLFW_KEY_EQUAL:
        K = D_KEY_EQUALS      ;
        break;
    case GLFW_KEY_MINUS:
        K = D_KEY_MINUS       ;
        break;
    case GLFW_KEY_RIGHT_SHIFT:
        K = D_KEY_RSHIFT      ;
        break;
    case GLFW_KEY_LEFT_SHIFT:
        K = D_KEY_RSHIFT      ;
        break;
    case GLFW_KEY_RIGHT_CONTROL:
        K = D_KEY_RCTRL       ;
        break;
    case GLFW_KEY_RIGHT_ALT:
        K = D_KEY_RALT        ;
        break;
    case GLFW_KEY_LEFT_ALT:
        K = D_KEY_RALT        ;
        break;
    default:
        K = key;
        break;
    }
    if (action == GLFW_PRESS){
        kcode = (0x000000FF) & (0x80 | K);
        printf("%d\n",K);
    }
    else if(action == GLFW_RELEASE)
    {
        kcode = (0x000000FF) & (K);
    }
    kready = 1;
}

// Main code
int main(int, char**)
{
    ImRiscGui gui(SCREENWIDTH, SCREENHEIGHT);
    gui.key_callback = &key_callback;

    LoadDoom();

    vga_inc = true;

    if(!gui.Init()){
        return -1;
    }
    jump_cycles = true;
    std::thread cores(rv32i_threads);

    while(gui.Run()){
        if(update_stats.load()){
            gui.m_stats = cpu_stats.load();
            update_stats.store(false);
        }
        vga_inc.store(true);
        vga_get_buffer();
        gui.copyFrameBuffer(rgb_buffer);
        vga_inc.store(false);
    }
    kill.store(true);
    bool ret = gui.Exit();
    cores.join();
    
    return ret;
}
