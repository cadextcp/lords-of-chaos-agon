/* Deterministic PRNG (xorshift32). Same seed -> same sequence on Agon and host. */
#ifndef LOC_RNG_H
#define LOC_RNG_H

#include <stdint.h>

typedef struct {
    uint32_t state;
} Rng;

void rng_seed(Rng *r, uint32_t seed);
uint32_t rng_next(Rng *r);
/* Uniform value in [0, n). n must be > 0. */
uint16_t rng_range(Rng *r, uint16_t n);

#endif
