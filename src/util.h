/*
 * util.h - Freestanding helpers. We compile with -nostdlib, so the C library
 * is not linked. Anything we would normally get from <string.h> we provide
 * ourselves here. This keeps the firmware fully self-contained and shows what
 * those "free" library functions actually do.
 */
#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdint.h>

/* The compiler may emit calls to memcpy/memset for struct copies and array
 * initialisation even if you never call them by name, so they must exist. */
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);

/* Busy-wait roughly `ms` milliseconds. Calibrated for the 64 MHz core with a
 * simple loop; only used for coarse LED blinks, never for protocol timing
 * (that is the RTC's job). */
void delay_ms(uint32_t ms);

#endif /* UTIL_H */
