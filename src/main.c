/*
 * main.c - Find My Keys BLE beacon: top-level control flow.
 *
 * The whole program in one breath:
 *   bring up clocks -> read our address -> configure the radio and timer ->
 *   forever { build a fresh authenticated advertisement, transmit it on the
 *   three advertising channels, blink an LED, sleep until the next interval }.
 *
 * Everything hardware-specific is delegated to the small drivers in this folder
 * so this file reads like the datasheet's "typical usage" pseudocode.
 */
#include <stdbool.h>
#include <stdint.h>

#include "auth.h"
#include "ble.h"
#include "board.h"
#include "clock.h"
#include "config.h"
#include "ficr.h"
#include "radio.h"
#include "rng.h"
#include "rtc.h"
#include "rtt.h"

/* The advertising PDU must live in RAM (EasyDMA) - a plain global does. */
static uint8_t s_pdu[BLE_PDU_MAX];

static const uint8_t s_channels[3] = {
    BLE_ADV_CHANNEL_37,
    BLE_ADV_CHANNEL_38,
    BLE_ADV_CHANNEL_39,
};

/* Log the MAC in the human-friendly big-endian order scanners display. */
static void log_mac(const uint8_t mac[6])
{
    rtt_printf("MAC (random static): %02x:%02x:%02x:%02x:%02x:%02x\n",
               mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
}

int main(void)
{
    board_init();
    rtt_init();

    rtt_printf("\n=== Find My Keys beacon ===\n");
    rtt_printf("name=\"%s\" interval=%ums\n", BEACON_NAME, ADV_INTERVAL_MS);

    /* The radio needs the HF crystal; the RTC needs the LF crystal. */
    clock_hfxo_start();
    clock_lfxo_start();

    uint8_t mac[6];
    ficr_get_ble_mac(mac);
    log_mac(mac);

    auth_init();
    rng_init();
    radio_init();
    rtc_init();

    bool lost_mode = false;
    bool btn_prev = false;

    rtt_printf("advertising...\n");

    for (;;) {
        /* Button 1 edge toggles "lost mode": advertise faster and light LED2. */
        bool btn = button1_pressed();
        if (btn && !btn_prev) {
            lost_mode = !lost_mode;
            if (lost_mode) {
                led_on(LED2_PIN);
                rtt_printf("lost mode ON\n");
            } else {
                led_off(LED2_PIN);
                rtt_printf("lost mode OFF\n");
            }
        }
        btn_prev = btn;

        /* Fresh counter + authentication tag for this event. */
        uint32_t counter = auth_next_counter();
        ble_build_adv(s_pdu, mac, counter);

        /* One packet on each advertising channel = one advertising event. */
        for (uint32_t i = 0; i < 3; i++) {
            radio_send(s_pdu, s_channels[i]);
        }

        led_toggle(LED1_PIN); /* visible "heartbeat" of the beacon */

        if ((counter & 0x3f) == 0) {
            rtt_printf("adv #%u sent\n", counter);
        }

        /* Sleep until the next interval, plus BLE-mandated random jitter. */
        uint32_t base_ms = lost_mode ? ADV_INTERVAL_LOST_MS : ADV_INTERVAL_MS;
        uint32_t jitter_ms = rng_get_byte() % (ADV_JITTER_MAX_MS + 1u);
        rtc_sleep_ticks(RTC_MS_TO_TICKS(base_ms + jitter_ms));
    }
}
