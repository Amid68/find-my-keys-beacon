/*
 * rtc.c - see rtc.h.
 *
 * The compare interrupt handler does the bare minimum: acknowledge the event
 * and set a flag. The heavy lifting stays in thread mode. `s_fired` is written
 * by the ISR and read by the main loop, so it must be volatile.
 */
#include "rtc.h"
#include "nrf52840.h"

static volatile uint32_t s_fired;

/* RTC0 interrupt handler. The name is referenced by the vector table in
 * startup.c, overriding the weak Default_Handler. */
void RTC0_IRQHandler(void)
{
    if (RTC0_EVENTS_COMPARE0) {
        RTC0_EVENTS_COMPARE0 = 0; /* acknowledge */
        s_fired = 1;
    }
}

void rtc_init(void)
{
    RTC0_PRESCALER = 0;                    /* full 32.768 kHz resolution */
    RTC0_INTENSET = RTC0_INTEN_COMPARE0;   /* interrupt on COMPARE0      */
    nvic_enable_irq(IRQ_RTC0);
    RTC0_TASKS_CLEAR = 1;
    RTC0_TASKS_START = 1;
}

void rtc_sleep_ticks(uint32_t ticks)
{
    s_fired = 0;
    RTC0_CC0 = ticks;
    RTC0_EVENTS_COMPARE0 = 0;

    /* Sleep until the ISR sets the flag. Re-checking in a loop guards against
     * spurious wakeups from other events. */
    while (!s_fired) {
        nrf_wfe();
    }

    /* Reset the counter so the next interval measures from zero. */
    RTC0_TASKS_CLEAR = 1;
}
