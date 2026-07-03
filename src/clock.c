/*
 * clock.c - see clock.h.
 *
 * Pattern for every nRF task/event pair:
 *   1. clear the EVENTS_* flag,
 *   2. fire the TASKS_* trigger,
 *   3. spin until EVENTS_* is set by hardware.
 */
#include "clock.h"
#include "nrf52840.h"

void clock_hfxo_start(void)
{
    CLOCK_EVENTS_HFCLKSTARTED = 0;
    CLOCK_TASKS_HFCLKSTART = 1;
    while (CLOCK_EVENTS_HFCLKSTARTED == 0) {
        /* wait for the crystal to stabilise */
    }
}

void clock_lfxo_start(void)
{
    /* Select the external 32.768 kHz crystal as the LFCLK source. */
    CLOCK_LFCLKSRC = CLOCK_LFCLKSRC_XTAL;

    CLOCK_EVENTS_LFCLKSTARTED = 0;
    CLOCK_TASKS_LFCLKSTART = 1;
    while (CLOCK_EVENTS_LFCLKSTARTED == 0) {
        /* wait for the LF crystal to start */
    }
}
