#ifndef MUSIC_SEMANTICS_HARMONIC_FUNCTION_H
#define MUSIC_SEMANTICS_HARMONIC_FUNCTION_H

#include <stdbool.h>

/*
 * HarmonicFunction (Value): the role a chord plays in the flow of a phrase,
 * in the order the functions usually occur. A plain enum: it names a role and
 * carries no EntityID, no chord, and no framework claim.
 *
 *   TONIC         a chord that sounds stable, a sense of home or center
 *   PREDOMINANT   a chord that transitions away from tonic toward dominant
 *   DOMINANT      a chord that gives a sense of urgency to resolve to tonic
 *
 * Which chords have which function, and whether a predominant is strong or
 * weak, is a convention of a Framework (frameworks/standard-harmony), not of
 * this type.
 */

typedef enum {
    HARMONIC_FUNCTION_TONIC = 0,
    HARMONIC_FUNCTION_PREDOMINANT,
    HARMONIC_FUNCTION_DOMINANT,
    HARMONIC_FUNCTION_COUNT
} HarmonicFunction;

/* True for the three functions above; false for any other value. */
bool harmonic_function_valid(HarmonicFunction function);

/* "tonic" "predominant" "dominant"; NULL for an invalid function. */
const char *harmonic_function_name(HarmonicFunction function);

#endif
