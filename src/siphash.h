/*
 * siphash.h - SipHash-2-4 keyed pseudorandom function / MAC.
 *
 * SipHash (Aumasson & Bernstein, 2012) is a fast, tiny keyed hash designed for
 * short inputs. It is a *PRF*: without the 128-bit key you cannot predict or
 * forge its output. That is exactly what an authenticated beacon needs - we
 * tag every frame so a receiver holding the shared key can distinguish our
 * real beacon from a spoofed clone.
 *
 * Why SipHash and not, say, AES-CMAC?
 *   - Pure ARX (add / rotate / xor): no S-boxes, no tables, ~80 lines of C,
 *     no dependence on the chip's AES peripheral. Perfect for "from scratch".
 *   - Constant-time by construction (no data-dependent branches or lookups),
 *     which matters on a device an attacker can hold in their hand.
 *   - Built for short messages, which is all we have (6-byte MAC + counter).
 *
 * Reference implementation: https://github.com/veorq/SipHash (CC0).
 * This is an independent, spec-faithful reimplementation.
 */
#ifndef SIPHASH_H
#define SIPHASH_H

#include <stddef.h>
#include <stdint.h>

/*
 * Compute SipHash-2-4 over `in` (`inlen` bytes) using the 128-bit key given as
 * two 64-bit words (k0, k1). Returns the full 64-bit tag.
 *
 * "2-4" = 2 SipRounds per absorbed message block, 4 finalization rounds. This
 * is the standard, recommended parameter set.
 */
uint64_t siphash24(const void *in, size_t inlen, uint64_t k0, uint64_t k1);

#endif /* SIPHASH_H */
