/*
 * board.c - see board.h.
 */
#include "board.h"
#include "config.h"
#include "nrf52840.h"

void board_init(void)
{
    const uint32_t leds = (1UL << LED1_PIN) | (1UL << LED2_PIN) |
                          (1UL << LED3_PIN) | (1UL << LED4_PIN);

    /* Drive LEDs high first (active-low => off), then make them outputs so we
     * never flash them on during configuration. */
    GPIO_P0_OUTSET = leds;
    GPIO_P0_DIRSET = leds;

    /* Button 1: input with pull-up (idles high, reads low when pressed). */
    GPIO_P0_DIRCLR = (1UL << BTN1_PIN);
    GPIO_P0_PIN_CNF(BTN1_PIN) = GPIO_PIN_CNF_INPUT | GPIO_PIN_CNF_PULLUP;
}

void led_on(uint32_t pin)     { GPIO_P0_OUTCLR = (1UL << pin); } /* active low */
void led_off(uint32_t pin)    { GPIO_P0_OUTSET = (1UL << pin); }

void led_toggle(uint32_t pin)
{
    if (GPIO_P0_OUT & (1UL << pin)) {
        GPIO_P0_OUTCLR = (1UL << pin);
    } else {
        GPIO_P0_OUTSET = (1UL << pin);
    }
}

bool button1_pressed(void)
{
    return (GPIO_P0_IN & (1UL << BTN1_PIN)) == 0;
}
