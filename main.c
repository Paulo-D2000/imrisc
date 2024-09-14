//#define DEBUG_REGS
#define DEBUG_ASM
#define DEBUG_HIST_SIZE 1
#include "debugging.h"

#include "rom.h"
#include "cpu.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h> 
#include <math.h>

static uint32_t rgb_buffer[320*200];
static FILE* vga_fptr;
void vga_get_buffer(){
    static size_t i = 0;
    static size_t fpc = 0;
    if(vga_new_pixel){
        if(i < 320*200){
            vga_new_pixel = false;
            uint8_t* vga_screen = &VGA[ScreenAddressOffset];
            uint8_t* vga_palette = &VGA[PaletteAddressOffset];
            uint8_t pal_idx = vga_screen[i];
            uint8_t r = vga_palette[4 * pal_idx + 0];
            uint8_t g = vga_palette[4 * pal_idx + 1];
            uint8_t b = vga_palette[4 * pal_idx + 2];
            uint8_t a = vga_palette[4 * pal_idx + 3];
            rgb_buffer[i] = (a << 24) | (b << 24) | (g << 24) | r;
            i++;
        }else{
            i = 0;
            fwrite(rgb_buffer, 1, 320*200*sizeof(uint32_t), vga_fptr);
            fseek(vga_fptr, 0, SEEK_SET);
        }
    }
    if(fpc == 2000){
        fpc = 0;
        vga_frameCount += 1;
    }else{
        fpc++;
    }
}


void vga_start(){
    printf("vga_start\n");
    uint32_t* vga_palette = (uint32_t*)&VGA[PaletteAddressOffset];
    for (size_t i = 0; i < 256; i++)
    {
		VGA[PaletteAddressOffset + i*4 + 0] = (uint8_t)(i & 0xFF);
        VGA[PaletteAddressOffset + i*4 + 1] = (uint8_t)(i & 0xFF);
        VGA[PaletteAddressOffset + i*4 + 2] = (uint8_t)(i & 0xFF);
        VGA[PaletteAddressOffset + i*4 + 3] = 0xFF;
	}

    vga_frameCount = 0;
    vga_vblank = 1;

    // Open a file in binary mode
    printf("vga_fopen\n");
    vga_fptr = fopen("test.raw", "wb");
    printf("vga_ret\n");
}

long hex_load(const char* path, uint8_t* ptr, size_t max_size){
    FILE* filePointer;
    int bufferLength = 255;\
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

int main(){
    printf("Starting ImRisc RV32I Emulator...\n");

    CPU rv32i = {BRAM_ADDR, { 0, 0, 0, 0, 0, 0, 0, 0,
                   0, 0, 0, 0, 0, 0, 0, 0,
                   0, 0, 0, 0, 0, 0, 0, 0,
                   0, 0, 0, 0, 0, 0, 0, 0 }};

    

    if(1){
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
        
        vga_start();

        SYMBOL_TABLE_SIZE = tab_sz;

        // breakpoints
        //BREAKPOINTS[ACTIVE_BREAKPOINTS].address = 0x4013cc30;
        //BREAKPOINTS[ACTIVE_BREAKPOINTS].enabled = true;
        //ACTIVE_BREAKPOINTS++;
    }

    //if(file_load("../binfile", (void*)&BRAM[0], BRAM_SIZE) == -1){
    //   exit(-1);
    //}
    //printf("Trying to load symbols...\n");
    //long tab_sz = load_symbols("../binfile_syms.Sym");
    //if(tab_sz == -1){
    //    printf("[ERROR] Failed to load symbols!\n");
    //    exit(-1);
    //}

    /* ------------------------- TESTS -------------------- */

    // ALU TEST [ PASS ]
    // if(hex_load("../test_alu.mem", (void*)&BRAM[0], BRAM_SIZE) == -1){
    //     exit(-1);
    // }

    // JAL_JALR TEST [ PASS ]
    // if(hex_load("../test_jaljalr.mem", (void*)&BRAM[0], BRAM_SIZE) == -1){
    //     exit(-1);
    // }

    // JMPS TEST [ PASS ]
    // if(hex_load("../test_jmps.mem", (void*)&BRAM[0], BRAM_SIZE) == -1){
    //     exit(-1);
    // }

    // LOAD_SOTORE TEST [ PASS ]
    //if(hex_load("../test_loadstore.mem", (void*)&BRAM[0], BRAM_SIZE) == -1){
    //    exit(-1);
    //}

    // LUI_AUIPC TEST [ PASS ]
    //if(hex_load("../test_luiauipc.mem", (void*)&BRAM[0], BRAM_SIZE) == -1){
    //    exit(-1);
    //}

    /* ------------------------- TESTS -------------------- */

    size_t cycles = 0;
    bool ret = true;
    clock_t t;
    double average = 0.0f;
    t = clock();
    jump_cycles = true;
    while(ret){
        if(cycles % 100000 == 0){
            t = clock() - t; 
            double time_taken = ((double)t)/CLOCKS_PER_SEC; // in seconds 
            average += time_taken;
            t = clock();
        }
        if(cycles % 10000000 == 0){
            double mean_cpu_time = 1e9 * average / cycles;
            double mean_cpu_freq = 1e3 / mean_cpu_time;
            printf("[RV32I CPU]\n  PC: %08x\n  Cycles: %ld\n  Mean cycle time: %f ns\n  Mean Clock: %f MHz\n", rv32i.pc, cycles,  mean_cpu_time, mean_cpu_freq); 
            //cycles = 0;
            average = 0.0;
        }
        ret = cpu_cycle(&rv32i);
        uart_printf();
        vga_get_buffer();
        cycles++;
        //if(cycles > 16761798) jump_cycles = false;
    }
    double mean_cpu_time = 1e9 * average / cycles;
    double mean_cpu_freq = 1e3 / mean_cpu_time;
    printf("[RV32I CPU]\n  Cycles: %ld\n  Mean cycle time: %f ns\n  Mean Clock: %f MHz\n", cycles,  mean_cpu_time, mean_cpu_freq); 

    return 0;
}