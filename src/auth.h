/*
 * auth.h - Rolling authentication tag for each advertising frame.
 *
 * Threat we address: anyone can hear our advertisement and replay it, or make
 * a clone that shouts the same name. We cannot stop them transmitting, but we
 * can make a genuine frame *verifiable*:
 *
 *   tag = SipHash-2-4( key, MAC[6] || version[1] || counter[4 LE] )  truncated to 4 bytes
 *
 * The counter is monotonic (increments every event) and is sent in the clear
 * alongside the tag. A receiver that knows the key recomputes the tag and, by
 * remembering the highest counter it has seen, rejects both forgeries (wrong
 * tag) and replays (counter not greater than last seen).
 *
 * Truncating a 64-bit PRF to 32 bits is a deliberate size/security trade for a
 * beacon: a blind forger has a 1-in-2^32 chance per guess, which is plenty when
 * combined with counter monotonicity. docs/04-security.md discusses the limits.
 */
#ifndef AUTH_H
#define AUTH_H

#include <stdint.h>

#define AUTH_TAG_LEN 4

/* Reset the rolling counter (called once at boot). */
void auth_init(void);

/* Advance and return the next counter value. */
uint32_t auth_next_counter(void);

/* Compute the 4-byte tag for (mac || version || counter) into `out`. */
void auth_compute_tag(const uint8_t mac[6], uint8_t version, uint32_t counter,
                      uint8_t out[AUTH_TAG_LEN]);

#endif /* AUTH_H */
