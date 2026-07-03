/*
 * radio.h - Drive the nRF52840 RADIO to emit BLE advertising packets.
 *
 * We speak "BLE 1 Mbit" at the physical layer and hand-assemble the on-air
 * packet format (access address, whitening, CRC) so the RADIO turns our byte
 * array into a spec-legal advertisement that any phone can decode.
 */
#ifndef RADIO_H
#define RADIO_H

#include <stdint.h>

/* The three BLE primary advertising channel indices. */
#define BLE_ADV_CHANNEL_37 37
#define BLE_ADV_CHANNEL_38 38
#define BLE_ADV_CHANNEL_39 39

/*
 * One-time RADIO configuration: 1 Mbit BLE mode, advertising access address,
 * BLE CRC, whitening, packet field layout and TX power. Call once at startup
 * (after the HFXO is running).
 */
void radio_init(void);

/*
 * Transmit a single, fully-formed advertising PDU on `channel` (37, 38 or 39).
 * `pdu` must live in RAM (the RADIO's EasyDMA cannot read flash) and be laid
 * out as: [S0/header][length][payload...]. Blocks until the packet is on air.
 */
void radio_send(const uint8_t *pdu, uint8_t channel);

#endif /* RADIO_H */
