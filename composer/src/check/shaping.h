#ifndef URSATZ_COMPOSER_SHAPING_H
#define URSATZ_COMPOSER_SHAPING_H

#include <stdio.h>

/*
 * Schoenberg's shaping preferences (R-164): soft rules the melody and harmony checkers score with their others.
 * A checker that finds one broken sets its bit in PhraseVerdict.shaping_broken.
 */
#define SHAPING_ENDING_REDUCTION (1u << 0) /* ch. II  */
#define SHAPING_WAVES (1u << 1)            /* ch. XI  */
#define SHAPING_CENTRAL_RANGE (1u << 2)    /* ch. XI  */
#define SHAPING_MONOTONY (1u << 3)         /* ch. XII */
#define SHAPING_ROOT_PROGRESSION (1u << 4) /* ch. XII */
#define SHAPING_COUNT 5

/* Where a scored candidate's reason names the shaping rules that did not hold: "<what>; not held: <names>". */
#define SHAPING_NOT_HELD "; not held: "

static inline const char *shaping_name(unsigned index) {
    static const char *const NAMES[SHAPING_COUNT] = {"ending-reduction", "waves", "central-range", "monotony",
                                                      "root-progression"};
    return index < SHAPING_COUNT ? NAMES[index] : "";
}

/* Writes base, and after it the names of the rules in the broken bits when there are any. */
static inline void shaping_reason(char *out, size_t size, const char *base, unsigned broken) {
    int used = snprintf(out, size, "%s", base);
    const char *separator = SHAPING_NOT_HELD;
    for (unsigned i = 0; broken && i < SHAPING_COUNT; i++) {
        if ((broken & (1u << i)) && used > 0 && (size_t)used < size) {
            used += snprintf(out + used, size - (size_t)used, "%s%s", separator, shaping_name(i));
            separator = ", ";
        }
    }
}

#endif
