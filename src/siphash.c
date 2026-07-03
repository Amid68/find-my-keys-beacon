/*
 * siphash.c - SipHash-2-4, reimplemented from the specification.
 *
 * State is four 64-bit words v0..v3. The message is absorbed 8 bytes at a time;
 * each block is xored into v3, then `cROUNDS` SipRounds mix the state, then the
 * block is xored into v0. After the last (length-tagged) block, v2 ^= 0xff and
 * `dROUNDS` finalization rounds run. The tag is v0 ^ v1 ^ v2 ^ v3.
 *
 * See the ASCII diagram in docs/04-security.md for the data flow.
 */
#include "siphash.h"

#define cROUNDS 2
#define dROUNDS 4

/* 64-bit rotate-left. */
static inline uint64_t rotl(uint64_t x, int b)
{
    return (x << b) | (x >> (64 - b));
}

/* Read 8 little-endian bytes into a u64. Endian-safe: we assemble byte by byte
 * rather than casting, so it behaves identically on the LE Cortex-M4 target and
 * on whatever host runs the CI self-test. */
static inline uint64_t u8to64_le(const uint8_t *p)
{
    return ((uint64_t)p[0]) | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) |
           ((uint64_t)p[3] << 24) | ((uint64_t)p[4] << 32) |
           ((uint64_t)p[5] << 40) | ((uint64_t)p[6] << 48) |
           ((uint64_t)p[7] << 56);
}

/* The core mixing function. Four MIX operations arranged as an ARX network. */
#define SIPROUND                     \
    do {                             \
        v0 += v1;                    \
        v1 = rotl(v1, 13);           \
        v1 ^= v0;                    \
        v0 = rotl(v0, 32);           \
        v2 += v3;                    \
        v3 = rotl(v3, 16);           \
        v3 ^= v2;                    \
        v0 += v3;                    \
        v3 = rotl(v3, 21);           \
        v3 ^= v0;                    \
        v2 += v1;                    \
        v1 = rotl(v1, 17);           \
        v1 ^= v2;                    \
        v2 = rotl(v2, 32);           \
    } while (0)

uint64_t siphash24(const void *in, size_t inlen, uint64_t k0, uint64_t k1)
{
    const uint8_t *ni = (const uint8_t *)in;

    /* Initialise state with the four SipHash magic constants xored with the
     * key. The constants spell "somepseudorandomlygeneratedbytes" in ASCII. */
    uint64_t v0 = 0x736f6d6570736575ULL ^ k0;
    uint64_t v1 = 0x646f72616e646f6dULL ^ k1;
    uint64_t v2 = 0x6c7967656e657261ULL ^ k0;
    uint64_t v3 = 0x7465646279746573ULL ^ k1;

    const uint8_t *end = ni + (inlen - (inlen % 8));
    const int left = (int)(inlen & 7);
    int i;

    /* Absorb every full 8-byte block. */
    for (; ni != end; ni += 8) {
        uint64_t m = u8to64_le(ni);
        v3 ^= m;
        for (i = 0; i < cROUNDS; i++) {
            SIPROUND;
        }
        v0 ^= m;
    }

    /* Final block: the remaining 0..7 bytes, with the top byte set to the total
     * message length mod 256. This length-padding is what makes SipHash safe
     * against extension games on same-prefix messages. */
    uint64_t b = ((uint64_t)inlen) << 56;
    switch (left) {
    case 7: b |= ((uint64_t)ni[6]) << 48; /* fallthrough */
    case 6: b |= ((uint64_t)ni[5]) << 40; /* fallthrough */
    case 5: b |= ((uint64_t)ni[4]) << 32; /* fallthrough */
    case 4: b |= ((uint64_t)ni[3]) << 24; /* fallthrough */
    case 3: b |= ((uint64_t)ni[2]) << 16; /* fallthrough */
    case 2: b |= ((uint64_t)ni[1]) << 8;  /* fallthrough */
    case 1: b |= ((uint64_t)ni[0]);       /* fallthrough */
    case 0: break;
    }

    v3 ^= b;
    for (i = 0; i < cROUNDS; i++) {
        SIPROUND;
    }
    v0 ^= b;

    /* Finalization. */
    v2 ^= 0xff;
    for (i = 0; i < dROUNDS; i++) {
        SIPROUND;
    }

    return v0 ^ v1 ^ v2 ^ v3;
}
