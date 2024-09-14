#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct breakpoint_t
{
    uint32_t address;
    bool enabled;
}breakpoint_t;

#define BREAKPOINTS_MAX 0xFF
static size_t ACTIVE_BREAKPOINTS = 0;
static breakpoint_t BREAKPOINTS[BREAKPOINTS_MAX] = {0};