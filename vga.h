#pragma once

#include <stdint.h>
#include <stdbool.h>

#define VGA_ADDR 0x81000000
#define ControlAddressOffset 0x00000
#define PaletteAddressOffset 0x10000
#define ScreenAddressOffset 0x20000 // Palette Size
#define SCREENWIDTH  320
#define SCREENHEIGHT 200
#define FRAMESIZE (SCREENHEIGHT*SCREENWIDTH)
#define VGA_SIZE (5*FRAMESIZE)

static uint8_t VGA[VGA_SIZE] = {0};
static bool vga_new_pixel = false;
static uint32_t vga_frameCount = 0;
static uint32_t vga_vblank = 0;
