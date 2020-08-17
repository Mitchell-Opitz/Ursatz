#ifndef URSATZ_FRAMEWORKS_STANDARD_RHYTHM_NOTEHEAD_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_RHYTHM_NOTEHEAD_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/note_value.h"

/*
 * Notehead, stem, and flag rules for standard-rhythm-v1. How a note's
 * value shows in its shape:
 *
 *   FILLED   noteheads are filled in (black) for the quarter note and
 *            shorter, unfilled (white) for the half note and longer
 *   STEMS    a filled notehead always has a stem; an unfilled one may or
 *            may not
 *   FLAGS    flags go only on the stem of a filled notehead: one for the
 *            eighth, two for the sixteenth, and one more for each
 *            further halving
 *   HALVING  there are three ways to halve a value in writing: add a stem
 *            (whole to half), fill the notehead (half to quarter), add a
 *            flag (quarter to eighth, and each step after). The breve to
 *            whole step is none of these.
 *
 * A dot does not change any of this: a dotted value is drawn like its
 * base value. Beams are not covered (they replace flags across a group
 * and depend on meter).
 */

typedef enum {
    STANDARD_RHYTHM_NOTEHEAD_RULE_FILLED = 0,
    STANDARD_RHYTHM_NOTEHEAD_RULE_STEMS,
    STANDARD_RHYTHM_NOTEHEAD_RULE_FLAGS,
    STANDARD_RHYTHM_NOTEHEAD_RULE_HALVING,
    STANDARD_RHYTHM_NOTEHEAD_RULE_COUNT
} StandardRhythmNoteheadRule;

typedef enum {
    STEM_REQUIREMENT_OPTIONAL, /* unfilled noteheads */
    STEM_REQUIREMENT_REQUIRED  /* filled noteheads */
} StemRequirement;

typedef enum {
    HALVING_STEP_NONE,
    HALVING_STEP_ADD_STEM,
    HALVING_STEP_FILL_NOTEHEAD,
    HALVING_STEP_ADD_FLAG
} HalvingStep;

const Rule *standard_rhythm_notehead_rule(StandardRhythmNoteheadRule which);
const Constraint *standard_rhythm_notehead_constraint(StandardRhythmNoteheadRule which);

/* FILLED: true for the quarter note and shorter. */
bool standard_rhythm_notehead_filled(const NoteValue *note_value);

/* STEMS: whether a stem is required or optional for this value. */
StemRequirement standard_rhythm_stem_requirement(const NoteValue *note_value);

/* STEMS: true when drawing the value with or without a stem is allowed. */
bool standard_rhythm_stem_allowed(const NoteValue *note_value, bool has_stem);

/* FLAGS: how many flags the value carries (0 up to the quarter note, 1
   for the eighth, 2 for the sixteenth, ...). */
int standard_rhythm_flag_count(const NoteValue *note_value);

/* FLAGS: true when a note of this value drawn with or without a stem may
   carry `flags` flags -- the value's own flag count, and only on a
   stem. */
bool standard_rhythm_flags_allowed(const NoteValue *note_value, bool has_stem, int flags);

/* HALVING: how the value is written when halved to the next smaller one
   (HALVING_STEP_NONE for the breve). */
HalvingStep standard_rhythm_halving_step(const NoteValue *note_value);

#endif
