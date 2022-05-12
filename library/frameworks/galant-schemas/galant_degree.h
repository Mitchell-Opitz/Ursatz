#ifndef URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_DEGREE_H
#define URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_DEGREE_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"

/*
 * Scale degrees for galant-schemas-v1. A degree is counted from a tonic in
 * letter names, from 1 (do) to 7 (ti), and its alteration is how many semitones
 * the pitch sits above (+) or below (-) the same degree of the major scale on
 * that tonic. So in C, E-flat is degree 3 altered by -1 (me), and A natural is
 * degree 6 altered by 0 (la); in a minor key the third and sixth degrees (mi
 * and la) are usually lowered (me and le), which is why the schemas' degrees
 * accept either.
 *
 *   GalantPitchDegree   a degree (1 to 7) and an alteration in semitones, of a
 *                       pitch against a tonic
 *   GalantDegree        a degree a prototype asks for: the degree (1 to 7) and
 *                       which alterations are accepted
 *
 * Pitch arithmetic is standard-counterpoint's (12-tone equal temperament); the
 * octave of a pitch does not matter.
 */

typedef struct {
    int degree;
    int alteration;
} GalantPitchDegree;

typedef enum {
    GALANT_ALTERATION_NATURAL = 0,
    GALANT_ALTERATION_LOWERED,
    GALANT_ALTERATION_EITHER,
    GALANT_ALTERATION_RAISED
} GalantAlteration;

typedef struct {
    int degree;
    GalantAlteration alteration;
} GalantDegree;

/* The degree and alteration of `pitch` against `tonic`. False (*out
   untouched) for NULL. */
bool galant_degree_of(const PitchSpelling *pitch, const PitchSpelling *tonic, GalantPitchDegree *out);

/* True when a pitch's degree satisfies what the prototype asks for: the same
   degree, and an alteration of 0 (natural), -1 (lowered), either of those
   (either), or +1 (raised). */
bool galant_degree_allows(const GalantDegree *wanted, const GalantPitchDegree *actual);

/* "natural" "lowered" "either" "raised"; NULL for an invalid alteration. */
const char *galant_alteration_name(GalantAlteration alteration);

#endif
