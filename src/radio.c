/*
 * radio.c - see radio.h.
 *
 * BLE physical-layer facts baked in below (from the Bluetooth Core spec and the
 * nRF52840 Product Specification):
 *
 *  - Advertising access address is the fixed value 0x8E89BED6. The RADIO builds
 *    the access address from BASE0 (lower bytes) + PREFIX0 (top byte), sent
 *    least-significant-byte first. BASE0=0x89BED600, PREFIX0=0x8E gives the
 *    on-air sequence D6 BE 89 8E == 0x8E89BED6.
 *  - CRC is 24-bit, polynomial 0x65B, initial value 0x555555, computed over the
 *    PDU only (not the access address).
 *  - Data whitening uses a 7-bit LFSR seeded with the channel index (bit 6 of
 *    the seed is hard-wired to 1 by the hardware).
 *  - Channel 37 -> 2402 MHz, 38 -> 2426 MHz, 39 -> 2480 MHz.
 */
#include "radio.h"
#include "config.h"
#include "nrf52840.h"

/* Map a BLE advertising channel index to the RADIO FREQUENCY value, which is
 * megahertz above 2400. */
static uint32_t channel_to_frequency(uint8_t channel)
{
    switch (channel) {
    case BLE_ADV_CHANNEL_37: return 2;  /* 2402 MHz */
    case BLE_ADV_CHANNEL_38: return 26; /* 2426 MHz */
    case BLE_ADV_CHANNEL_39: return 80; /* 2480 MHz */
    default:                 return 2;
    }
}

void radio_init(void)
{
    /* Power-cycle the peripheral to a known state. */
    RADIO_POWER = 0;
    RADIO_POWER = 1;

    RADIO_MODE = RADIO_MODE_BLE_1MBIT;
    RADIO_TXPOWER = BEACON_TXPOWER;

    /* Fast ramp-up shortens the gap between TXEN and the packet going out. */
    RADIO_MODECNF0 = RADIO_MODECNF0_RU_FAST;

    /* Packet field layout (PCNF0):
     *   S0  = 1 byte  -> the PDU header's first byte (type/TxAdd/RxAdd)
     *   LEN = 8 bits  -> the PDU length field
     *   S1  = 0 bits
     *   PREAMBLE = 8 bit (value 0 selects the 1 Mbit preamble)             */
    RADIO_PCNF0 = (1UL << RADIO_PCNF0_S0LEN_Pos) |
                  (8UL << RADIO_PCNF0_LFLEN_Pos) |
                  (0UL << RADIO_PCNF0_S1LEN_Pos) |
                  (0UL << RADIO_PCNF0_PLEN_Pos);

    /* Packet limits & on-air options (PCNF1):
     *   MAXLEN  = 37   (AdvA 6 + AdvData up to 31)
     *   STATLEN = 0
     *   BALEN   = 3    (3 base + 1 prefix = 4-byte access address)
     *   ENDIAN  = little
     *   WHITEEN = enabled                                                   */
    RADIO_PCNF1 = (37UL << RADIO_PCNF1_MAXLEN_Pos) |
                  (0UL << RADIO_PCNF1_STATLEN_Pos) |
                  (3UL << RADIO_PCNF1_BALEN_Pos) |
                  (0UL << RADIO_PCNF1_ENDIAN_Pos) |
                  (1UL << RADIO_PCNF1_WHITEEN_Pos);

    /* Advertising access address 0x8E89BED6 (see file header). */
    RADIO_BASE0 = 0x89BED600UL;
    RADIO_PREFIX0 = 0x0000008EUL;
    RADIO_TXADDRESS = 0;   /* transmit using logical address 0 */
    RADIO_RXADDRESSES = 1;

    /* BLE CRC: 3 bytes, skip the access address, poly 0x65B, init 0x555555. */
    RADIO_CRCCNF = (3UL << RADIO_CRCCNF_LEN_Pos) |
                   (1UL << RADIO_CRCCNF_SKIPADDR_Pos);
    RADIO_CRCPOLY = 0x0000065BUL;
    RADIO_CRCINIT = 0x00555555UL;

    /* Chain the state machine so a single TXEN produces one packet and returns
     * to DISABLED with no CPU intervention:
     *   READY -> START (begin sending once ramped up)
     *   END   -> DISABLE (power down the TX after the packet)               */
    RADIO_SHORTS = RADIO_SHORTS_READY_START | RADIO_SHORTS_END_DISABLE;
}

void radio_send(const uint8_t *pdu, uint8_t channel)
{
    /* Retune and reseed whitening for this channel. Both must be set while the
     * radio is disabled, which it always is on entry here. */
    RADIO_FREQUENCY = channel_to_frequency(channel);
    RADIO_DATAWHITEIV = channel; /* bit 6 forced to 1 by hardware */

    /* Point EasyDMA at the packet. Casting away const is fine: the RADIO only
     * reads this buffer during TX. */
    RADIO_PACKETPTR = (uint32_t)(uintptr_t)pdu;

    /* Clear the completion event, then start the ramp-up. The SHORTS take it
     * from here: READY->START sends, END->DISABLE powers down. */
    RADIO_EVENTS_DISABLED = 0;
    RADIO_EVENTS_END = 0;
    nrf_dsb();
    RADIO_TASKS_TXEN = 1;

    /* Wait for the radio to finish and return to the disabled state. */
    while (RADIO_EVENTS_DISABLED == 0) {
        /* one advertising packet is ~380 us on air at 1 Mbit */
    }
}
