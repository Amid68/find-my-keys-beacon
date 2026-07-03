/*
 * util.c - see util.h.
 */
#include "util.h"

void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    while (n--) {
        *d++ = (uint8_t)c;
    }
    return dst;
}

/*
 * Coarse millisecond delay. `volatile` on the loop counter stops the optimiser
 * from deleting the empty loop. The iteration count is a rough fit for the
 * 64 MHz Cortex-M4; it does not need to be exact because we only use it for
 * human-visible LED blinks.
 */
void delay_ms(uint32_t ms)
{
    /* ~ 64000 core cycles per ms; a few cycles per loop iteration. */
    for (volatile uint32_t i = 0; i < ms * 8000u; i++) {
        __asm volatile("nop");
    }
}
