#ifndef MUSIC_SEMANTICS_BEAT_STRENGTH_H
#define MUSIC_SEMANTICS_BEAT_STRENGTH_H

#include <stdbool.h>

/*
 * BeatStrength (Value): the metric weight of a position in a bar, strong or
 * weak. A plain enum: it names a role in a metric pattern and carries no
 * EntityID, no time signature, and no framework claim.
 *
 *   STRONG   a position that carries metric emphasis: a downbeat
 *   WEAK     a position between the strong ones: an upbeat
 *
 * Which positions of a bar are strong is a convention of a Framework (in
 * two notes against one, the first note of each bar is strong and the
 * second weak; frameworks/standard-counterpoint), not of this type. It is
 * also not the beat grid of a time signature (time/time_signature.h).
 */

typedef enum {
    BEAT_STRENGTH_STRONG = 0,
    BEAT_STRENGTH_WEAK,
    BEAT_STRENGTH_COUNT
} BeatStrength;

/* True for the two strengths above; false for any other value. */
bool beat_strength_valid(BeatStrength strength);

/* "strong" "weak"; NULL for an invalid strength. */
const char *beat_strength_name(BeatStrength strength);

/* True when `a` is strong and `b` is weak. False for any other pair,
   including two equal strengths and invalid values. */
bool beat_strength_is_stronger(BeatStrength a, BeatStrength b);

#endif
