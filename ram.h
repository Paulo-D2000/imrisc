#pragma once

#include <stdint.h>

#define RAM_ADDR 0x41000000
#define RAM_SIZE 0x10000000
static uint8_t RAM[RAM_SIZE] = {0};

#define BRAM_ADDR 0x00000000
#define BRAM_SIZE 0x00001000
static uint8_t BRAM[BRAM_SIZE] = {0};