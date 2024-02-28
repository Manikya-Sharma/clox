#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// for generating logs of the VM
#define DEBUG_TRACE_EXECUTION
#define DEBUG_PRINT_CODE
// stress mode test for GC (run as often as it can)
#define DEBUG_STRESS_GC
// logs for GC
#define DEBUG_LOG_GC

#define UINT8_COUNT (UINT8_MAX + 1)

#endif