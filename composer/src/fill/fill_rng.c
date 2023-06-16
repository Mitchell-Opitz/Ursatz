#include <ursatz_composer/fill_rng.h>

void fill_rng_seed(FillRng *rng, unsigned long seed) {
    rng->state = (uint64_t)seed;
}

uint64_t fill_rng_next(FillRng *rng) {
    rng->state += 0x9E3779B97F4A7C15ULL;
    uint64_t z = rng->state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

uint64_t fill_rng_below(FillRng *rng, uint64_t bound) {
    if (bound <= 1) {
        return 0;
    }
    /* Draws below 2^64 mod bound would favor small results; skip them. */
    uint64_t threshold = (0 - bound) % bound;
    uint64_t draw;
    do {
        draw = fill_rng_next(rng);
    } while (draw < threshold);
    return draw % bound;
}
