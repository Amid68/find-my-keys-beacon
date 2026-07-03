/*
 * ficr.h - Read the chip's factory device address and turn it into a valid
 * BLE "random static" address.
 */
#ifndef FICR_H
#define FICR_H

#include <stdint.h>

/*
 * Fill `mac[0..5]` with a BLE random static address derived from the FICR
 * DEVICEADDR registers. mac[0] is the least significant byte (the order the
 * radio transmits AdvA on air).
 *
 * A BLE "random static" address must have its two most-significant bits set to
 * 0b11, so we force them in mac[5]. Everything else is the factory-random value
 * unique to your board.
 */
void ficr_get_ble_mac(uint8_t mac[6]);

#endif /* FICR_H */
