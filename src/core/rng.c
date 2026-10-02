#include "rng.h"

void rng_seed(Rng *r, uint32_t seed)
{
    /* xorshift must never hold 0 */
    r->state = seed ? seed : 0x2545F491UL;
}

uint32_t rng_next(Rng *r)
{
    uint32_t x = r->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->state = x;
    return x;
}

uint16_t rng_range(Rng *r, uint16_t n)
{
    return (uint16_t)(rng_next(r) % n);
}
