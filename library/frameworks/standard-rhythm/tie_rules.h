#ifndef URSATZ_FRAMEWORKS_STANDARD_RHYTHM_TIE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_RHYTHM_TIE_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/duration.h"
#include "time/note_value.h"

/*
 * Tie rules for standard-rhythm-v1. A tie is a curved line joining two
 * or more notes to combine their durations:
 *
 *   SAME_PITCH         a tie connects notes of the same pitch; notes of
 *                      different pitches joined by a curve are a slur,
 *                      not a tie (slurs are not covered here). Pitch is
 *                      compared as written -- PitchSpelling equality --
 *                      so C-sharp and D-flat are different pitches.
 *   NOT_ON_RESTS       ties are never used with rests
 *   NO_REARTICULATION  a tied-to note is not articulated again: only the
 *                      first note of a tied chain sounds an attack
 *   DURATION_SUM       the tied durations add: a half note tied to a
 *                      quarter note is held for three quarters, the same
 *                      length as a dotted half note
 *
 * The Tie entity (events/tie.h) holds only the two NoteEvent ids, so
 * these rules take the pitches and values as plain inputs; resolving a
 * Tie's ids to NoteEvents is the caller's job. A NULL pitch stands for a
 * rest.
 */

typedef enum {
    STANDARD_RHYTHM_TIE_RULE_SAME_PITCH = 0,
    STANDARD_RHYTHM_TIE_RULE_NOT_ON_RESTS,
    STANDARD_RHYTHM_TIE_RULE_NO_REARTICULATION,
    STANDARD_RHYTHM_TIE_RULE_DURATION_SUM,
    STANDARD_RHYTHM_TIE_RULE_COUNT
} StandardRhythmTieRule;

const Rule *standard_rhythm_tie_rule(StandardRhythmTieRule which);
const Constraint *standard_rhythm_tie_constraint(StandardRhythmTieRule which);

/* SAME_PITCH: true when the two pitches are equal as written. False if
   either is NULL. */
bool standard_rhythm_tie_same_pitch(const PitchSpelling *first, const PitchSpelling *second);

/* NOT_ON_RESTS: true when a tie may connect these (neither is a rest,
   i.e. neither is NULL). */
bool standard_rhythm_tie_allowed_between(const PitchSpelling *first, const PitchSpelling *second);

/* SAME_PITCH + NOT_ON_RESTS: true when a tie validly connects the two. */
bool standard_rhythm_tie_valid(const PitchSpelling *first, const PitchSpelling *second);

/* NO_REARTICULATION: whether the note at position `index` (0 is the first
   note of the tied chain) is articulated -- only index 0 is. */
bool standard_rhythm_tied_note_articulated(size_t index);

/*
 * DURATION_SUM: the combined duration of a tied chain, the exact sum of
 * each value's Duration (dots included). False, leaving *out untouched,
 * for an empty chain, a NULL entry, or allocation failure. On success the
 * caller owns *out.
 */
bool standard_rhythm_tied_duration(const NoteValue *const *values, size_t count, Duration **out);

#endif
