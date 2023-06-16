#ifndef URSATZ_COMPOSER_FILL_RNG_H
#define URSATZ_COMPOSER_FILL_RNG_H

#include <stdint.h>

/*
 * The fill loop's random numbers. A run has one generator, seeded once, and
 * every random choice in it comes from that one stream: the loop's tie-breaks
 * and any randomness an operator uses while proposing candidates. Operators must
 * use the FillRng they are handed and no other source (no rand(), no clock), or
 * the run stops being reproducible from its seed.
 *
 * SplitMix64: one 64-bit state, so the whole state fits in a record and a run
 * can be resumed or replayed from any step.
 */

typedef struct {
    uint64_t state;
} FillRng;

void fill_rng_seed(FillRng *rng, unsigned long seed);
uint64_t fill_rng_next(FillRng *rng);
/* A uniform number from 0 to bound - 1, with no modulo bias. bound must be at least 1. */
uint64_t fill_rng_below(FillRng *rng, uint64_t bound);

#endif
