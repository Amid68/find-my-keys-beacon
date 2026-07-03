/*
 * board.h - nRF52840-DK board specifics: the four user LEDs and button 1.
 *
 * All LEDs are wired active-LOW (drive the pin low to light the LED). Buttons
 * read low when pressed and need an internal pull-up.
 */
#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stdint.h>

/* Configure LED pins as outputs (off) and button 1 as a pulled-up input. */
void board_init(void);

void led_on(uint32_t pin);
void led_off(uint32_t pin);
void led_toggle(uint32_t pin);

/* Returns true while button 1 is held down. */
bool button1_pressed(void);

#endif /* BOARD_H */
