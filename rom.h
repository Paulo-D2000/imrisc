#pragma once

#include <stdint.h>
#include <stddef.h>

#define ROM_ADDR 0x40100000
#define ROM_SIZE 0x00100000
static uint8_t ROM[ROM_SIZE] = {0};

#define WAD_ROM_ADDR 0x40200000
static size_t WAD_ROM_SIZE = 0;
static uint8_t* WAD_ROM = NULL;