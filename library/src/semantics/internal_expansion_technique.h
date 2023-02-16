#ifndef MUSIC_SEMANTICS_INTERNAL_EXPANSION_TECHNIQUE_H
#define MUSIC_SEMANTICS_INTERNAL_EXPANSION_TECHNIQUE_H

#include <stdbool.h>

/*
 * InternalExpansionTechnique (Value): a technique that expands a phrase from within. A plain enum: it names a value and carries no EntityID, no
 * container, and no framework claim.
 *
 *   REPETITION            material repeated unexpectedly, adding length
 *   STRETCHING            a harmony or melody made to last longer than expected
 *   ONE_MORE_TIME         a cadence tried, evaded, and tried again
 *   ALTERNATIVE_PATH      new material that takes the phrase off its expected course
 *
 * Techniques are often combined. What makes each one is a convention of a Framework
 * (frameworks/standard-form), not of this type.
 */

typedef enum {
    INTERNAL_EXPANSION_TECHNIQUE_REPETITION = 0,
    INTERNAL_EXPANSION_TECHNIQUE_STRETCHING,
    INTERNAL_EXPANSION_TECHNIQUE_ONE_MORE_TIME,
    INTERNAL_EXPANSION_TECHNIQUE_ALTERNATIVE_PATH,
    INTERNAL_EXPANSION_TECHNIQUE_COUNT
} InternalExpansionTechnique;

/* True for the 4 values above; false for any other value. */
bool internal_expansion_technique_valid(InternalExpansionTechnique value);

/* "repetition" "stretching" "one-more-time" "alternative-path"; NULL for an invalid
   value. */
const char *internal_expansion_technique_name(InternalExpansionTechnique value);

#endif
