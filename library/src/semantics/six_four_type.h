#ifndef MUSIC_SEMANTICS_SIX_FOUR_TYPE_H
#define MUSIC_SEMANTICS_SIX_FOUR_TYPE_H

#include <stdbool.h>

/*
 * SixFourType (Value): the kind of a 6/4 chord, a triad in second inversion, which has
 * a dissonance with the bass (the fourth), so composers treat it in special ways. A plain
 * enum: it names a kind and carries no EntityID, no chord, and no framework claim. In
 * analysis, whenever a 6/4 chord turns up, the Roman numeral alone is not a label: the
 * kind determines it.
 *
 *   CADENTIAL      an embellishment of V(7) from two embellishing tones, a sixth and a
 *                  fourth above sol in the bass, on a stronger beat than its resolution
 *   PASSING        a 6/4 chord built on a passing tone in the bass, between two chords
 *                  of the same function (it prolongs tonic or predominant)
 *   NEIGHBOR       a 6/4 chord over a static bass in which two upper voices make upper
 *                  neighbor motion, between the same root position chord (it prolongs
 *                  I or V); also called the pedal 6/4
 *   ARPEGGIATING   a 6/4 chord made when the bass leaps to the fifth of a chord while
 *                  the upper voices sustain it; it may prolong any harmony and is
 *                  typically not labeled in analysis
 *
 * What makes each kind, and how it is written, is a convention of a Framework
 * (frameworks/standard-harmony), not of this type.
 */

typedef enum {
    SIX_FOUR_TYPE_CADENTIAL = 0,
    SIX_FOUR_TYPE_PASSING,
    SIX_FOUR_TYPE_NEIGHBOR,
    SIX_FOUR_TYPE_ARPEGGIATING,
    SIX_FOUR_TYPE_COUNT
} SixFourType;

/* True for the four types above; false for any other value. */
bool six_four_type_valid(SixFourType type);

/* "cadential" "passing" "neighbor" "arpeggiating"; NULL for an invalid type. */
const char *six_four_type_name(SixFourType type);

/* True for the types that are labeled in analysis: cadential, passing, and neighbor.
   False for the arpeggiating 6/4 and for an invalid type. */
bool six_four_type_is_labeled(SixFourType type);

#endif
