/*
 * ble.h - Construct a raw BLE advertising PDU by hand.
 *
 * No stack builds this for us. We lay out every byte of an ADV_NONCONN_IND
 * (non-connectable, non-scannable undirected advertising) packet exactly as the
 * Bluetooth Core spec describes it, so you can see how a name and some custom
 * data become the bytes a phone displays.
 */
#ifndef BLE_H
#define BLE_H

#include <stdint.h>

/* Largest PDU we build: 2 header bytes + 6 AdvA + 31 AdvData. */
#define BLE_PDU_MAX 39

/*
 * Build a complete advertising PDU into `out` (must be >= BLE_PDU_MAX bytes and
 * live in RAM for EasyDMA). The frame carries:
 *   - Flags AD (LE General Discoverable, BR/EDR not supported)
 *   - Complete Local Name AD (BEACON_NAME)
 *   - Manufacturer Specific Data AD: company id, version, rolling counter and a
 *     4-byte SipHash authentication tag.
 *
 * Returns the number of bytes written (the length the radio will transmit).
 */
uint32_t ble_build_adv(uint8_t *out, const uint8_t mac[6], uint32_t counter);

#endif /* BLE_H */
