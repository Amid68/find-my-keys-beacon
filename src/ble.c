/*
 * ble.c - see ble.h.
 *
 * On-air PDU layout (what radio_send transmits after the preamble + access
 * address it adds itself):
 *
 *   +--------+--------+------------------+---------------------------+
 *   | header | length |   AdvA (6 B LE)  |   AdvData (AD structures) |
 *   +--------+--------+------------------+---------------------------+
 *      S0        LEN
 *
 * header byte bit layout (LSB first on air):
 *   [3:0] PDU type   (0x2 = ADV_NONCONN_IND)
 *   [4]   RFU
 *   [5]   ChSel      (0)
 *   [6]   TxAdd      (1 = random address, matches our random-static AdvA)
 *   [7]   RxAdd      (0)
 * => 0x2 | (1<<6) = 0x42.
 *
 * AdvData is a sequence of AD structures, each: [len][type][len-1 bytes value].
 */
#include "ble.h"
#include "auth.h"
#include "config.h"

/* GAP AD types we use (Bluetooth "Assigned Numbers"). */
#define AD_TYPE_FLAGS          0x01
#define AD_TYPE_COMPLETE_NAME  0x09
#define AD_TYPE_MANUFACTURER   0xFF

/* Flags value: LE General Discoverable (0x02) | BR/EDR Not Supported (0x04). */
#define BLE_FLAGS  0x06

#define ADV_NONCONN_IND_HEADER 0x42

/* Compile-time length of BEACON_NAME without the trailing NUL. */
static uint32_t const_strlen(const char *s)
{
    uint32_t n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

uint32_t ble_build_adv(uint8_t *out, const uint8_t mac[6], uint32_t counter)
{
    uint32_t i = 0;

    /* --- Header + length placeholder ------------------------------------- */
    out[i++] = ADV_NONCONN_IND_HEADER; /* S0 (PDU header first byte)         */
    uint32_t len_index = i++;          /* fill LENGTH in after we know it     */

    /* --- AdvA: 6-byte advertiser address, little-endian ------------------ */
    for (uint32_t m = 0; m < 6; m++) {
        out[i++] = mac[m];
    }

    /* --- AD 1: Flags ----------------------------------------------------- */
    out[i++] = 0x02;           /* length of this AD structure */
    out[i++] = AD_TYPE_FLAGS;
    out[i++] = BLE_FLAGS;

    /* --- AD 2: Complete Local Name --------------------------------------- */
    const char *name = BEACON_NAME;
    uint32_t name_len = const_strlen(name);
    out[i++] = (uint8_t)(1 + name_len); /* type byte + name bytes */
    out[i++] = AD_TYPE_COMPLETE_NAME;
    for (uint32_t n = 0; n < name_len; n++) {
        out[i++] = (uint8_t)name[n];
    }

    /* --- AD 3: Manufacturer Specific Data -------------------------------- *
     * value = company_id(2 LE) | version(1) | counter(4 LE) | tag(4)        */
    uint8_t tag[AUTH_TAG_LEN];
    auth_compute_tag(mac, BEACON_VERSION, counter, tag);

    out[i++] = (uint8_t)(1 + 2 + 1 + 4 + AUTH_TAG_LEN); /* AD length */
    out[i++] = AD_TYPE_MANUFACTURER;
    out[i++] = (uint8_t)(BEACON_COMPANY_ID & 0xff);
    out[i++] = (uint8_t)((BEACON_COMPANY_ID >> 8) & 0xff);
    out[i++] = BEACON_VERSION;
    out[i++] = (uint8_t)(counter & 0xff);
    out[i++] = (uint8_t)((counter >> 8) & 0xff);
    out[i++] = (uint8_t)((counter >> 16) & 0xff);
    out[i++] = (uint8_t)((counter >> 24) & 0xff);
    for (uint32_t t = 0; t < AUTH_TAG_LEN; t++) {
        out[i++] = tag[t];
    }

    /* --- Back-fill the PDU LENGTH field ---------------------------------- *
     * LENGTH counts everything after it: AdvA(6) + all AD bytes. That equals
     * the total bytes written minus the 2 header bytes.                     */
    out[len_index] = (uint8_t)(i - 2);

    return i;
}
