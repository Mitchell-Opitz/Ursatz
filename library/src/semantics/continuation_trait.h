#ifndef MUSIC_SEMANTICS_CONTINUATION_TRAIT_H
#define MUSIC_SEMANTICS_CONTINUATION_TRAIT_H

#include <stdbool.h>

/*
 * ContinuationTrait (Value): a trait that marks a continuation. A plain enum: it names a value and carries no EntityID, no
 * container, and no framework claim.
 *
 *   FRAGMENTATION         unit sizes shorter than the units before
 *   INCREASED_RHYTHMIC_ACTIVITYfaster durations than in previous units
 *   SEQUENCE              units repeated and transposed
 *   INCREASED_HARMONIC_RHYTHMchord changes more often than before
 *
 * How many traits a continuation shows, and which, is a convention of a Framework
 * (frameworks/standard-form), not of this type.
 */

typedef enum {
    CONTINUATION_TRAIT_FRAGMENTATION = 0,
    CONTINUATION_TRAIT_INCREASED_RHYTHMIC_ACTIVITY,
    CONTINUATION_TRAIT_SEQUENCE,
    CONTINUATION_TRAIT_INCREASED_HARMONIC_RHYTHM,
    CONTINUATION_TRAIT_COUNT
} ContinuationTrait;

/* True for the 4 values above; false for any other value. */
bool continuation_trait_valid(ContinuationTrait value);

/* "fragmentation" "increased-rhythmic-activity" "sequence" "increased-harmonic-rhythm"; NULL for an invalid
   value. */
const char *continuation_trait_name(ContinuationTrait value);

#endif
