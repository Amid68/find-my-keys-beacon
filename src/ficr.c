/*
 * ficr.c - see ficr.h.
 *
 * The nRF52840 stores a per-chip random address across two read-only registers:
 *   DEVICEADDR[0] = address bytes 0..3 (little-endian)
 *   DEVICEADDR[1] = address bytes 4..5 in its low 16 bits
 * (Verified against the nRF52840 Product Specification, FICR section.)
 */
#include "ficr.h"
#include "nrf52840.h"

void ficr_get_ble_mac(uint8_t mac[6])
{
    uint32_t addr_lo = FICR_DEVICEADDR0;   /* bytes 0..3 */
    uint32_t addr_hi = FICR_DEVICEADDR1;   /* bytes 4..5 in bits [15:0] */

    mac[0] = (uint8_t)(addr_lo & 0xff);
    mac[1] = (uint8_t)((addr_lo >> 8) & 0xff);
    mac[2] = (uint8_t)((addr_lo >> 16) & 0xff);
    mac[3] = (uint8_t)((addr_lo >> 24) & 0xff);
    mac[4] = (uint8_t)(addr_hi & 0xff);
    mac[5] = (uint8_t)((addr_hi >> 8) & 0xff);

    /* Force the top two bits of the most-significant byte to 0b11 so that this
     * qualifies as a BLE *random static* address. Without this, scanners may
     * reject the address type. */
    mac[5] |= 0xC0;
}
