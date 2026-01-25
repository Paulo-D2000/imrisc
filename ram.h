#ifndef RAM_H
#define RAM_H

#include <stdint.h>

#define RAM_ADDR 0x41000000
#define RAM_SIZE 0x10000000
extern uint8_t RAM[RAM_SIZE];

#define BRAM_ADDR 0x00000000
#define BRAM_SIZE 0x00001000
extern uint8_t BRAM[BRAM_SIZE];

#endif // RAM_H