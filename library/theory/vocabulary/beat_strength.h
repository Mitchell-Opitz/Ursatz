#ifndef MUSIC_SEMANTICS_BEAT_STRENGTH_H
#define MUSIC_SEMANTICS_BEAT_STRENGTH_H

#include <stdbool.h>

/*
 * BeatStrength (Value): the metric weight of a position in a bar: strong,
 * medium, or weak. A plain enum: it names a role in a metric pattern and
 * carries no EntityID, no time signature, and no framework claim.
 *
 *   STRONG   a position that carries the most metric emphasis: a downbeat
 *   MEDIUM   a position with some emphasis, between the strong and the weak:
 *            the third quarter note of a bar of four
 *   WEAK     a position between the others: an upbeat
 *
 * The values are ordered from the strongest to the weakest, so a smaller
 * value is a stronger position. Which positions of a bar have which
 * strength is a convention of a Framework (in two notes against one, the
 * first note of each bar is strong and the second weak; in four notes
 * against one, the four notes are strong, weak, medium, weak;
 * frameworks/standard-counterpoint), not of this type. It is also not the
 * beat grid of a time signature (time/time_signature.h).
 */

typedef enum {
    BEAT_STRENGTH_STRONG = 0,
    BEAT_STRENGTH_MEDIUM,
    BEAT_STRENGTH_WEAK,
    BEAT_STRENGTH_COUNT
} BeatStrength;

/* True for the three strengths above; false for any other value. */
bool beat_strength_valid(BeatStrength strength);

/* "strong" "medium" "weak"; NULL for an invalid strength. */
const char *beat_strength_name(BeatStrength strength);

/* True when `a` is a stronger position than `b`: strong over medium or
   weak, medium over weak. False for two equal strengths and for invalid
   values. */
bool beat_strength_is_stronger(BeatStrength a, BeatStrength b);

#endif
