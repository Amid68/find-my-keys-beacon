/*
 * test_siphash.c - Host-side self-test for the firmware's SipHash-2-4.
 *
 * This compiles the *exact same* src/siphash.c that runs on the nRF52840, but
 * with your native compiler, and checks it against published SipHash-2-4 test
 * vectors (from the SipHash paper, Aumasson & Bernstein 2012, and the official
 * reference implementation at https://github.com/veorq/SipHash).
 *
 * Why this matters (the DevOps angle): the crypto is the one part of this
 * firmware you cannot eyeball-verify with a phone. A scanner shows you the
 * name and the counter, but a wrong tag looks just as random as a right one.
 * So we prove the primitive correct on the host, in CI, on every push - and
 * because it is the same C file, byte for byte, that proof carries over to
 * the target.
 *
 * It also prints one sample beacon tag so you can cross-check the firmware,
 * the RTT logs, and tools/verify_beacon.py against each other by hand.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/siphash.h"

/* Reference key from the SipHash paper: bytes 00 01 02 ... 0f. As two
 * little-endian u64 words that is: */
#define TEST_K0 0x0706050403020100ULL
#define TEST_K1 0x0f0e0d0c0b0a0908ULL

struct vector {
    size_t len;        /* message is bytes 00, 01, ..., len-1 */
    uint64_t expected; /* published SipHash-2-4 output        */
};

/* Published vectors for key 00..0f, message 00..len-1. The 15-byte entry is
 * the worked example in Appendix A of the SipHash paper itself. */
static const struct vector vectors[] = {
    {0, 0x726fdb47dd0e0e31ULL},
    {1, 0x74f839c593dc67fdULL},
    {8, 0x93f5f5799a932462ULL},
    {15, 0xa129ca6149be45e5ULL},
};

/* Mirror of the firmware's auth_compute_tag() message layout:
 * MAC(6) || version(1) || counter(4 LE), tag = low 4 bytes of SipHash. */
static void sample_beacon_tag(void)
{
    const uint8_t mac[6] = {0xA3, 0xF5, 0x50, 0x8F, 0x2A, 0xE2};
    const uint8_t version = 0x01;
    const uint32_t counter = 1;

    uint8_t msg[11];
    memcpy(msg, mac, 6);
    msg[6] = version;
    msg[7] = (uint8_t)(counter & 0xff);
    msg[8] = (uint8_t)((counter >> 8) & 0xff);
    msg[9] = (uint8_t)((counter >> 16) & 0xff);
    msg[10] = (uint8_t)((counter >> 24) & 0xff);

    uint64_t t = siphash24(msg, sizeof(msg), TEST_K0, TEST_K1);
    printf("sample beacon tag (demo key, MAC e2:2a:8f:50:f5:a3, ctr=1): "
           "%02x %02x %02x %02x\n",
           (unsigned)(t & 0xff), (unsigned)((t >> 8) & 0xff),
           (unsigned)((t >> 16) & 0xff), (unsigned)((t >> 24) & 0xff));
}

int main(void)
{
    uint8_t msg[16];
    for (size_t i = 0; i < sizeof(msg); i++) {
        msg[i] = (uint8_t)i;
    }

    int failures = 0;
    for (size_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++) {
        uint64_t got = siphash24(msg, vectors[i].len, TEST_K0, TEST_K1);
        if (got == vectors[i].expected) {
            printf("PASS len=%2zu  0x%016llx\n", vectors[i].len,
                   (unsigned long long)got);
        } else {
            printf("FAIL len=%2zu  got 0x%016llx  want 0x%016llx\n",
                   vectors[i].len, (unsigned long long)got,
                   (unsigned long long)vectors[i].expected);
            failures++;
        }
    }

    sample_beacon_tag();

    if (failures) {
        printf("%d vector(s) FAILED\n", failures);
        return 1;
    }
    printf("all SipHash-2-4 vectors passed\n");
    return 0;
}
