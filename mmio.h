#pragma once

#include "config.h"
#include "ram.h"
#include "rom.h"
#include "uart.h"
#include "vga.h"
#include "spi.h"
#include <stdbool.h>
#include <stdio.h>

typedef enum mmio_size_t{_BYTE=1u, _SHORT=2u, _WORD=4u} mmio_size_t;

typedef struct mmio_result_t
{
    uint8_t data[_WORD];
    bool valid;
} mmio_result_t;

static uint32_t last_addr = 0x0;
static uint32_t UART_KEY = 0xFFFFFFFF;

static inline bool check_range(const uint32_t addr, const uint32_t begin, const uint32_t size){
    return (addr >= begin) && (addr < (begin + size));
}
/*
mmio_result_t mmio_read(const uint32_t addr, const mmio_size_t readsize){
    last_addr = addr;
    mmio_result_t res = {.data = 0x00, .valid = false};
    if(check_range(addr, ROM_ADDR, ROM_SIZE - readsize )){
        for (size_t i = 0; i < readsize; i++) res.data[i] = ROM[(addr + i) - ROM_ADDR];
        res.valid = true;
    }
    else if(check_range(addr, WAD_ROM_ADDR, WAD_ROM_SIZE - readsize )){
        for (size_t i = 0; i < readsize; i++) res.data[i] = WAD_ROM[(addr + i) - WAD_ROM_ADDR];
        res.valid = true;
    }
    else if(check_range(addr, RAM_ADDR, RAM_SIZE - readsize )){
        for (size_t i = 0; i < readsize; i++) res.data[i] = RAM[(addr + i) - RAM_ADDR];
        res.valid = true;
    }
    else if(check_range(addr, BRAM_ADDR, BRAM_SIZE - readsize )){
        for (size_t i = 0; i < readsize; i++) res.data[i] = BRAM[(addr + i) - BRAM_ADDR];
        res.valid = true;
    }
    else if(check_range(addr, UART_ADDR, UART_SIZE)){
        for (size_t i = 0; i < readsize; i++) res.data[i] = (UART_KEY >> (8*i)) & 0xFF;
        res.valid = true;
        uart_rx = true;
    }
    else if(check_range(addr, SPI_ADDR, SPI_SIZE - readsize )){
        for (size_t i = 0; i < readsize; i++) res.data[i] = SPI[(addr + i) - SPI_ADDR];
        spi_printf(addr, 0);
        res.valid = true;
    }
    else if(check_range(addr, VGA_ADDR, VGA_SIZE - readsize )){
        if(addr == VGA_ADDR){
            uint32_t v = (vga_frameCount & 0xFFFF) | (vga_vblank << 16);
            //printf("STATUS: %08x - Frame Count: %d - VBLANK %d\n", v, vga_frameCount, vga_vblank);
            for (size_t i = 0; i < readsize; i++) res.data[i] = (v >> (8*i)) & 0xFF;
        }else{
            for (size_t i = 0; i < readsize; i++) res.data[i] = VGA[(addr + i) - VGA_ADDR];
        }
        res.valid = true;
    }
    return res;
}
*/

inline mmio_result_t mmio_read(const uint32_t addr, const mmio_size_t readsize){
    last_addr = addr;
    mmio_result_t res = {.data = 0x00, .valid = false};

    int device = -1;
    device = check_range(addr, ROM_ADDR, ROM_SIZE - readsize )         ? 0 : device;
    device = check_range(addr, WAD_ROM_ADDR, WAD_ROM_SIZE - readsize ) ? 1 : device;
    device = check_range(addr, RAM_ADDR, RAM_SIZE - readsize )         ? 2 : device;
    device = check_range(addr, BRAM_ADDR, BRAM_SIZE - readsize )       ? 3 : device;
    device = check_range(addr, UART_ADDR, UART_SIZE)                   ? 4 : device;
    device = check_range(addr, SPI_ADDR, SPI_SIZE - readsize )         ? 5 : device;
    device = check_range(addr, VGA_ADDR, VGA_SIZE - readsize )         ? 6 : device;

    switch (device)
    {
    case 0:
        for (size_t i = 0; i < readsize; i++) res.data[i] = ROM[(addr + i) - ROM_ADDR];
        res.valid = true;
        break;
    case 1:
        for (size_t i = 0; i < readsize; i++) res.data[i] = WAD_ROM[(addr + i) - WAD_ROM_ADDR];
        res.valid = true;
        break;
    case 2:
        for (size_t i = 0; i < readsize; i++) res.data[i] = RAM[(addr + i) - RAM_ADDR];
        res.valid = true;
        break;
    case 3:
        for (size_t i = 0; i < readsize; i++) res.data[i] = BRAM[(addr + i) - BRAM_ADDR];
        res.valid = true;
        break;
    case 4:
        for (size_t i = 0; i < readsize; i++) res.data[i] = (UART_KEY >> (8*i)) & 0xFF;
        res.valid = true;
        uart_rx = true;
        break;
    case 5:
        for (size_t i = 0; i < readsize; i++) res.data[i] = SPI[(addr + i) - SPI_ADDR];
        spi_printf(addr, 0);
        res.valid = true;
        break;
    case 6:
        if(addr == VGA_ADDR){
            uint32_t v = (vga_frameCount & 0xFFFF) | (vga_vblank << 16);
            //printf("STATUS: %08x - Frame Count: %d - VBLANK %d\n", v, vga_frameCount, vga_vblank);
            for (size_t i = 0; i < readsize; i++) res.data[i] = (v >> (8*i)) & 0xFF;
        }else{
            for (size_t i = 0; i < readsize; i++) res.data[i] = VGA[(addr + i) - VGA_ADDR];
        }
        res.valid = true;
        break;
    default:
        break;
    }
    return res;
}
/*
inline mmio_result_t mmio_write_n(const uint32_t addr, const uint32_t val, const mmio_size_t writesize){
    last_addr = addr;
    mmio_result_t res = {.data = 0x00, .valid = false};
    if(check_range(addr, ROM_ADDR, ROM_SIZE - writesize )){
        res.valid = false;
    }
    else if(check_range(addr, WAD_ROM_ADDR, WAD_ROM_SIZE - writesize )){
        res.valid = false;
    }
    else if(check_range(addr, RAM_ADDR, RAM_SIZE - writesize )){
        for (size_t i = 0; i < writesize; i++) RAM[(addr + i) - RAM_ADDR] = res.data[i] = (val >> (i*8)) & 0xFF;
        res.valid = true;
    }
    else if(check_range(addr, BRAM_ADDR, BRAM_SIZE - writesize )){
        for (size_t i = 0; i < writesize; i++) BRAM[(addr + i) - BRAM_ADDR] = res.data[i] = (val >> (i*8)) & 0xFF;
        res.valid = true;
    }
    else if(check_range(addr, UART_ADDR, UART_SIZE)){
        for (size_t i = 0; i < writesize; i++) UART[(addr + i) - UART_ADDR] = res.data[i] = (val >> (i*8)) & 0xFF;
        res.valid = true;
        uart_newbyte = true;
    }
    else if(check_range(addr, SPI_ADDR, SPI_SIZE - writesize )){
        for (size_t i = 0; i < writesize; i++) SPI[(addr + i) - SPI_ADDR] = res.data[i] = (val >> (i*8)) & 0xFF;
        spi_printf(addr, 0);
        res.valid = true;
    }
    else if(check_range(addr, VGA_ADDR, VGA_SIZE - writesize )){
        //switch(writesize){
        //    case _BYTE:
        //        VGA[addr - VGA_ADDR] = res.data[0] = (val >> 0) & 0xff;
        //        break;
        //    case _SHORT:
        //        VGA[addr - VGA_ADDR + 0] = res.data[1] = (val >> 0) & 0xff;
        //        VGA[addr - VGA_ADDR + 1] = res.data[0] = (val >> 8) & 0xff;
        //        break;
        //    case _WORD:
        //        VGA[addr - VGA_ADDR + 3] = res.data[3] = (val >> 0) & 0xff;
        //        VGA[addr - VGA_ADDR + 2] = res.data[2] = (val >> 8) & 0xff;
        //        VGA[addr - VGA_ADDR + 1] = res.data[1] = (val  >> 16) & 0xff;
        //        VGA[addr - VGA_ADDR + 0] = res.data[0] = (val  >> 24) & 0xff;
        //        break;
        //}
        if((addr - VGA_ADDR) < ScreenAddressOffset){
            uint32_t palptr = addr - VGA_ADDR;
            VGA[palptr + 0] = (val >> 16) & 0xff;
            VGA[palptr + 1] = (val >> 8) & 0xff;
            VGA[palptr + 2] = (val  >> 0) & 0xff;
            VGA[palptr + 3] = 0xff;
        }
        else{
            uint32_t scraddr = addr - VGA_ADDR;
            for (size_t i = 0; i < writesize; i++) VGA[scraddr + i] = res.data[i] = (val >> (i*8)) & 0xFF;
        }
        res.valid = true;
    }
    return res;
}
*/

inline mmio_result_t mmio_write_n(const uint32_t addr, const uint32_t val, const mmio_size_t writesize){
    last_addr = addr;
    mmio_result_t res = {.data = 0x00, .valid = false};

    int device = -1;
    device = check_range(addr, ROM_ADDR, ROM_SIZE - writesize )         ? 0 : device;
    device = check_range(addr, WAD_ROM_ADDR, WAD_ROM_SIZE - writesize ) ? 1 : device;
    device = check_range(addr, RAM_ADDR, RAM_SIZE - writesize )         ? 2 : device;
    device = check_range(addr, BRAM_ADDR, BRAM_SIZE - writesize )       ? 3 : device;
    device = check_range(addr, UART_ADDR, UART_SIZE)                    ? 4 : device;
    device = check_range(addr, SPI_ADDR, SPI_SIZE - writesize )         ? 5 : device;
    device = check_range(addr, VGA_ADDR, VGA_SIZE - writesize )         ? 6 : device;

    switch(device){
        case 0:
            res.valid = false;
            break;
        case 1:
            res.valid = false;
            break;
        case 2:
            for (size_t i = 0; i < writesize; i++) RAM[(addr + i) - RAM_ADDR] = res.data[i] = (val >> (i*8)) & 0xFF;
            res.valid = true;
            break;
        case 3:
            for (size_t i = 0; i < writesize; i++) BRAM[(addr + i) - BRAM_ADDR] = res.data[i] = (val >> (i*8)) & 0xFF;
            res.valid = true;
            break;
        case 4:
            for (size_t i = 0; i < writesize; i++) UART[(addr + i) - UART_ADDR] = res.data[i] = (val >> (i*8)) & 0xFF;
            res.valid = true;
            uart_newbyte = true;
            break;
        case 5:
            for (size_t i = 0; i < writesize; i++) SPI[(addr + i) - SPI_ADDR] = res.data[i] = (val >> (i*8)) & 0xFF;
            spi_printf(addr, 0);
            res.valid = true;
            break;
        case 6:
            //switch(writesize){
            //    case _BYTE:
            //        VGA[addr - VGA_ADDR] = res.data[0] = (val >> 0) & 0xff;
            //        break;
            //    case _SHORT:
            //        VGA[addr - VGA_ADDR + 0] = res.data[1] = (val >> 0) & 0xff;
            //        VGA[addr - VGA_ADDR + 1] = res.data[0] = (val >> 8) & 0xff;
            //        break;
            //    case _WORD:
            //        VGA[addr - VGA_ADDR + 3] = res.data[3] = (val >> 0) & 0xff;
            //        VGA[addr - VGA_ADDR + 2] = res.data[2] = (val >> 8) & 0xff;
            //        VGA[addr - VGA_ADDR + 1] = res.data[1] = (val  >> 16) & 0xff;
            //        VGA[addr - VGA_ADDR + 0] = res.data[0] = (val  >> 24) & 0xff;
            //        break;
            //}
            if((addr - VGA_ADDR) < ScreenAddressOffset){
                uint32_t palptr = addr - VGA_ADDR;
                VGA[palptr + 0] = (val >> 16) & 0xff;
                VGA[palptr + 1] = (val >> 8) & 0xff;
                VGA[palptr + 2] = (val  >> 0) & 0xff;
                VGA[palptr + 3] = 0xff;
            }
            else{
                uint32_t scraddr = addr - VGA_ADDR;
                for (size_t i = 0; i < writesize; i++) VGA[scraddr + i] = res.data[i] = (val >> (i*8)) & 0xFF;
            }
            res.valid = true;
            break;
        default:
            break;
    }
    return res;
}

inline mmio_result_t mmio_write_8(const uint32_t addr, const uint8_t val){
   return mmio_write_n(addr, val, _BYTE);
}

inline mmio_result_t mmio_write_16(const uint32_t addr, const uint16_t val){
    return mmio_write_n(addr, val, _SHORT);
}

inline mmio_result_t mmio_write_32(const uint32_t addr, const uint32_t val){
    return mmio_write_n(addr, val, _WORD);
}

inline uint16_t mmio_to_short(const mmio_result_t res){
    return res.data[1] << 8 | res.data[0];
}

inline uint32_t mmio_to_word(const mmio_result_t res){
    return res.data[3] << 24 | res.data[2] << 16 | res.data[1] << 8 | res.data[0];
}