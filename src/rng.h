/*
 * rng.h - Hardware random number generator, used for advertising jitter.
 */
#ifndef RNG_H
#define RNG_H

#include <stdint.h>

/* Enable bias correction on the RNG. Call once at startup. */
void rng_init(void);

/* Return one hardware-random byte (blocking, ~tens of microseconds). */
uint8_t rng_get_byte(void);

#endif /* RNG_H */
