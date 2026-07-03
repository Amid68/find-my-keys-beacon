/*
 * clock.h - Start the clock sources the rest of the firmware depends on.
 */
#ifndef CLOCK_H
#define CLOCK_H

/*
 * Start the 64 MHz high-frequency crystal oscillator (HFXO) and block until it
 * is running. The RADIO *requires* the crystal (not the internal RC) for an
 * accurate carrier, so this must be called before any transmission.
 */
void clock_hfxo_start(void);

/*
 * Start the 32.768 kHz low-frequency crystal oscillator (LFXO) and block until
 * it is running. This clocks RTC0, our wakeup timer.
 */
void clock_lfxo_start(void);

#endif /* CLOCK_H */
