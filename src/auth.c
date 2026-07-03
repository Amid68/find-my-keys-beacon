/*
 * auth.c - see auth.h.
 */
#include "auth.h"
#include "config.h"
#include "siphash.h"

/* Monotonic frame counter. Not persisted across reset (see docs/04-security.md
 * for how you would anchor it in flash/UICR to survive power cycles). */
static uint32_t s_counter;

void auth_init(void)
{
    s_counter = 0;
}

uint32_t auth_next_counter(void)
{
    return ++s_counter;
}

void auth_compute_tag(const uint8_t mac[6], uint8_t version, uint32_t counter,
                      uint8_t out[AUTH_TAG_LEN])
{
    /* Build the authenticated message: MAC(6) || version(1) || counter(4 LE).
     * Byte order here must match exactly what the verifier reconstructs. */
    uint8_t msg[6 + 1 + 4];
    msg[0] = mac[0];
    msg[1] = mac[1];
    msg[2] = mac[2];
    msg[3] = mac[3];
    msg[4] = mac[4];
    msg[5] = mac[5];
    msg[6] = version;
    msg[7] = (uint8_t)(counter & 0xff);
    msg[8] = (uint8_t)((counter >> 8) & 0xff);
    msg[9] = (uint8_t)((counter >> 16) & 0xff);
    msg[10] = (uint8_t)((counter >> 24) & 0xff);

    uint64_t tag = siphash24(msg, sizeof(msg), BEACON_SIPHASH_K0, BEACON_SIPHASH_K1);

    /* Truncate to the low 4 bytes, little-endian. */
    out[0] = (uint8_t)(tag & 0xff);
    out[1] = (uint8_t)((tag >> 8) & 0xff);
    out[2] = (uint8_t)((tag >> 16) & 0xff);
    out[3] = (uint8_t)((tag >> 24) & 0xff);
}
