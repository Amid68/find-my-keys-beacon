/*
 * rtt.h - Real-Time Transfer logging, implemented from scratch.
 *
 * RTT is SEGGER's trick for printf-style debugging with no UART: the target
 * keeps a ring buffer in RAM and a small "control block" describing it; the
 * debug probe (here, the DK's on-board J-Link, read by probe-rs) reaches in
 * over SWD and drains the buffer. Zero extra pins, no baud rate, non-blocking.
 *
 * We build the control block by hand so you can see the whole mechanism - it is
 * just a magic string plus buffer pointers and read/write offsets.
 */
#ifndef RTT_H
#define RTT_H

#include <stdint.h>

/* Initialise the control block (must run before the first log call). */
void rtt_init(void);

/* Write a NUL-terminated string to the RTT up-channel. */
void rtt_write_str(const char *s);

/*
 * Minimal formatted logging. Supported conversions:
 *   %s  string        %c  char        %%  literal percent
 *   %u  unsigned dec  %d  signed dec
 *   %x  lowercase hex %02x .. %08x zero-padded hex
 * Anything else is emitted verbatim.
 */
void rtt_printf(const char *fmt, ...);

#endif /* RTT_H */
