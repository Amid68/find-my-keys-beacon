/*
 * rng.c - see rng.h.
 *
 * The RNG derives entropy from thermal noise. DERCEN ("digital error
 * correction enable") removes bias at the cost of a little speed - worth it for
 * quality randomness. We only need a byte at a time for jitter, so blocking on
 * VALRDY is fine.
 */
#include "rng.h"
#include "nrf52840.h"

void rng_init(void)
{
    RNG_CONFIG = RNG_CONFIG_DERCEN;
}

uint8_t rng_get_byte(void)
{
    RNG_EVENTS_VALRDY = 0;
    RNG_TASKS_START = 1;
    while (RNG_EVENTS_VALRDY == 0) {
        /* wait for a fresh random byte */
    }
    uint8_t v = (uint8_t)(RNG_VALUE & 0xff);
    RNG_TASKS_STOP = 1;
    return v;
}
