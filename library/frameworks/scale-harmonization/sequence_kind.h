#ifndef MUSIC_SEMANTICS_SEQUENCE_KIND_H
#define MUSIC_SEMANTICS_SEQUENCE_KIND_H

#include <stdbool.h>

/*
 * SequenceKind (Value): a kind of sequential harmonization of a bass scale, a
 * pattern of one or more harmonies that repeats sequentially in the direction
 * of the scale. A plain enum: it names a kind and carries no EntityID, no
 * pitches, and no framework claim.
 *
 *   PARALLEL_CHORDS   a chord on each bass note, all of one kind (first
 *                     inversion), with no hierarchy or variety
 *   FIVE_SIX          a fifth and a sixth above the bass, alternating
 *   SEVEN_SIX         a chain of suspensions: a seventh above the bass
 *                     resolving to a sixth
 *   CYCLE_OF_FIFTHS   root motion descending by fifths, usually a fifth down
 *                     and a fourth up, with a stepwise progression between
 *                     alternate bass notes
 *   TWO_THREE         a chain of suspensions: a second above the bass
 *                     resolving to a third (the inversion of the 7-6)
 *
 * Which patterns a framework encodes, and their intervals above the bass, is
 * its own convention (frameworks/scale-harmonization).
 */

typedef enum {
    SEQUENCE_KIND_PARALLEL_CHORDS = 0,
    SEQUENCE_KIND_FIVE_SIX,
    SEQUENCE_KIND_SEVEN_SIX,
    SEQUENCE_KIND_CYCLE_OF_FIFTHS,
    SEQUENCE_KIND_TWO_THREE,
    SEQUENCE_KIND_COUNT
} SequenceKind;

/* True for the five kinds above; false for any other value. */
bool sequence_kind_valid(SequenceKind kind);

/* "parallel-chords" "five-six" "seven-six" "cycle-of-fifths" "two-three"; NULL
   for an invalid kind. */
const char *sequence_kind_name(SequenceKind kind);

/* True for the kinds that are chains of suspensions: the 7-6 and the 2-3. False
   for the others and for an invalid kind. */
bool sequence_kind_is_suspension_chain(SequenceKind kind);

#endif
