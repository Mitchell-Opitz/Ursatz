#ifndef MUSIC_SEMANTICS_MOTIVE_OCCURRENCE_KIND_H
#define MUSIC_SEMANTICS_MOTIVE_OCCURRENCE_KIND_H

#include <stdbool.h>

/*
 * MotiveOccurrenceKind (Value): how a recurrence of a motive reads against its prototype. A plain enum: it names a
 * kind and carries no EntityID, no notes, and no framework claim.
 *
 * It is the vocabulary of what was found, not of the operations that make a motive. The operations are
 * MotiveRelationKind (analysis/motive_relation.h, L8); the kernel matcher (motive_variant_recognize) finds them and the
 * caller maps each to an occurrence kind. VARIED has no relation: it is standard-form's own reading (a motive of at
 * least 5 notes with a few intervals changed).
 *
 *   EXACT       the same pitches
 *   REAL        the same semitone intervals from another pitch
 *   TONAL       the same scale-step intervals, other semitones
 *   INVERTED    every interval negated
 *   RETROGRADE  the notes in reverse order
 *   AUGMENTED   the same intervals, every onset gap doubled
 *   DIMINISHED  the same intervals, every onset gap halved
 *   FRAGMENTED  a prefix or suffix of the prototype
 *   VARIED      a few intervals differ, or the rhythm does
 */

typedef enum {
    MOTIVE_OCCURRENCE_EXACT = 0,
    MOTIVE_OCCURRENCE_REAL,
    MOTIVE_OCCURRENCE_TONAL,
    MOTIVE_OCCURRENCE_INVERTED,
    MOTIVE_OCCURRENCE_RETROGRADE,
    MOTIVE_OCCURRENCE_AUGMENTED,
    MOTIVE_OCCURRENCE_DIMINISHED,
    MOTIVE_OCCURRENCE_FRAGMENTED,
    MOTIVE_OCCURRENCE_VARIED,
    MOTIVE_OCCURRENCE_KIND_COUNT
} MotiveOccurrenceKind;

bool motive_occurrence_kind_valid(MotiveOccurrenceKind kind);
/* "exact" "real-transposed" "tonal-transposed" "inverted" "retrograde" "augmented" "diminished" "fragmented" "varied"; NULL otherwise. */
const char *motive_occurrence_kind_name(MotiveOccurrenceKind kind);
/* The precedence rank of a kind when several apply (lower wins): exact 0, real and tonal 1, inverted and retrograde 2, augmented and
   diminished 3, fragmented 4, varied 5. -1 for an invalid kind. */
int motive_occurrence_kind_rank(MotiveOccurrenceKind kind);

#endif
