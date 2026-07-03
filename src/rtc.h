/*
 * rtc.h - RTC0 as a low-power wakeup timer between advertising bursts.
 *
 * RTC0 counts ticks of the 32.768 kHz LFCLK. With PRESCALER = 0, one tick is
 * 1/32768 s ~= 30.5 us. We arm a COMPARE and sleep the CPU (WFE) until it
 * fires, which is how a real coin-cell beacon spends >99% of its life.
 */
#ifndef RTC_H
#define RTC_H

#include <stdint.h>

/* Convert milliseconds to RTC ticks (32768 ticks per second). */
#define RTC_MS_TO_TICKS(ms) (((uint32_t)(ms) * 32768u) / 1000u)

/* Start RTC0. Call once after the LFCLK is running. */
void rtc_init(void);

/*
 * Sleep (WFE, core powered down) until `ticks` LFCLK ticks have elapsed since
 * the RTC was last cleared, then clear it for the next interval.
 */
void rtc_sleep_ticks(uint32_t ticks);

#endif /* RTC_H */
