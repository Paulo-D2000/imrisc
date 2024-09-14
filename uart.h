#pragma once

#include <stdint.h>
#include <stdbool.h>

#define UART_ADDR 0x82000000
#define UART_SIZE 0x00000004

static uint8_t UART[UART_SIZE] = {0};

void uart_printf();
static bool uart_newbyte = false;
static bool uart_rx = false;