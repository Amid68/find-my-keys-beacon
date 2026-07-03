/*
 * config.h - Compile-time configuration for the Find My Keys beacon.
 *
 * Everything a user is likely to tweak lives here so the peripheral drivers
 * stay generic. Change a value, `make`, `make flash` - done.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* --- Identity ------------------------------------------------------------ */

/* The name advertised as the BLE "Complete Local Name" AD structure. This is
 * what you will actually see in nRF Connect / LightBlue on the iPhone, because
 * iOS deliberately hides the MAC address from apps (see docs/04-security.md). */
#define BEACON_NAME        "FindMyKeys"

/* Manufacturer-data "company identifier". 0xFFFF is reserved by the Bluetooth
 * SIG for internal/testing use, which is exactly what we are doing. */
#define BEACON_COMPANY_ID  0xFFFF

/* Version byte embedded in the manufacturer payload, so a receiver can tell
 * which frame format it is parsing. Bump when you change the layout. */
#define BEACON_VERSION     0x01

/* --- Timing -------------------------------------------------------------- */

/* Advertising interval in milliseconds (time between advertising *events*; one
 * event = one packet on each of the three channels). BLE allows 20 ms .. 10.24 s
 * for non-connectable undirected advertising. 100 ms is snappy; raise it to
 * save power. In "lost mode" (button 1) we advertise faster - see LOST_* below. */
#define ADV_INTERVAL_MS        100u
#define ADV_INTERVAL_LOST_MS   30u

/* Max extra random delay added per event (BLE requires 0..10 ms of jitter so a
 * crowd of beacons does not collide on the same slot forever). */
#define ADV_JITTER_MAX_MS      10u

/* Transmit power. See RADIO_TXPOWER_* in nrf52840.h. */
#define BEACON_TXPOWER         RADIO_TXPOWER_0DBM

/* --- Authentication key (SipHash-2-4) ------------------------------------ *
 *
 *  ####################################################################
 *  #  THIS IS A DEMO KEY. IT IS PUBLIC. DO NOT SHIP IT.               #
 *  #                                                                  #
 *  #  Every frame carries a 4-byte SipHash tag over (MAC || counter)  #
 *  #  so a receiver holding this same key can tell a genuine beacon   #
 *  #  from a spoofed one. That guarantee is only as good as the key's #
 *  #  secrecy. In a real product you would provision a unique random  #
 *  #  key per device at manufacture and store it where firmware       #
 *  #  read-out protection (APPROTECT) actually covers it.             #
 *  ####################################################################
 *
 * 128-bit key expressed as two little-endian 64-bit words, matching the
 * reference SipHash key layout (k0 = bytes 0..7, k1 = bytes 8..15).
 */
#define BEACON_SIPHASH_K0  0x0706050403020100ULL
#define BEACON_SIPHASH_K1  0x0f0e0d0c0b0a0908ULL

/* --- Board pins (nRF52840-DK, all on GPIO port 0, active LOW) ------------- */
#define LED1_PIN   13u  /* blinks once per advertising burst  */
#define LED2_PIN   14u  /* on = lost mode active              */
#define LED3_PIN   15u
#define LED4_PIN   16u
#define BTN1_PIN   11u  /* toggles lost mode                  */

#endif /* CONFIG_H */
