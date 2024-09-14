#pragma once

#include <stdint.h>
#include <stdbool.h>

#define SPI_ADDR 0x80000000
#define SPI_SIZE 0x000000FF

static uint8_t SPI[SPI_SIZE] = {0};

void spi_printf(uint32_t addr, bool rw);