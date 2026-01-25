#ifndef DMA_H
#define DMA_H

#include <stdint.h>
#include <stdbool.h>

/**
 * DMA INTERFACE
 * 
 */

#define DMA_ADDR 0x65000000
#define DstAddrOffset      0x0000
#define SrcAddrOffset      0x0004
#define TransferSizeOffset 0x0008
#define BusyOffset         0x000c
#define CommitOffset       0x0010
#define DMA_SIZE 0x14

static uint32_t dmaBusy = 0;
static uint32_t dmaDstAddr = 0;
static uint32_t dmaSrcAddr = 0;
static uint32_t dmaTransferSz = 0;
static uint32_t dmaCommit = 0;
void dma_controller();

#endif // DMA_H