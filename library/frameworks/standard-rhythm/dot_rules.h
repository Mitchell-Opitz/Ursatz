#ifndef URSATZ_FRAMEWORKS_STANDARD_RHYTHM_DOT_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_RHYTHM_DOT_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/duration.h"
#include "time/note_value.h"

/*
 * Dot rules for standard-rhythm-v1. A dot is written immediately after
 * a note or rest and lengthens it:
 *
 *   DOT_ADDS_HALF  a dot increases the duration by half: a dotted
 *                  quarter note is three eighth notes, a dotted whole
 *                  note three half notes
 *   DOT_SUBSEQUENT each further dot adds half the previous dot's amount,
 *                  so a double-dotted quarter note is a quarter plus an
 *                  eighth plus a sixteenth
 *   DOT_TOTAL      with n dots a value is (2^(n+1) - 1) / 2^n times its
 *                  base: 3/2 for one dot, 7/4 for two, 15/8 for three
 *
 * Dots apply to rests exactly as to notes. NoteValue holds the dot
 * count and caps it at NOTE_VALUE_MAX_DOTS.
 */

typedef enum {
    STANDARD_RHYTHM_DOT_RULE_ADDS_HALF = 0,
    STANDARD_RHYTHM_DOT_RULE_SUBSEQUENT,
    STANDARD_RHYTHM_DOT_RULE_TOTAL,
    STANDARD_RHYTHM_DOT_RULE_COUNT
} StandardRhythmDotRule;

const Rule *standard_rhythm_dot_rule(StandardRhythmDotRule which);
const Constraint *standard_rhythm_dot_constraint(StandardRhythmDotRule which);

/*
 * DOT_ADDS_HALF / DOT_SUBSEQUENT: the length the nth dot adds (n counts
 * from 1) to the note_value's undotted value -- half the base for the
 * first dot, a quarter for the second, and so on. The note_value's own
 * dot count is ignored. False, leaving *out untouched, when n is 0 or
 * past NOTE_VALUE_MAX_DOTS, or on allocation failure.
 */
bool standard_rhythm_dot_added_duration(const NoteValue *note_value, unsigned n, Duration **out);

/*
 * DOT_TOTAL: the factor a value is lengthened by when it carries `dots`
 * dots, as numerator / denominator (not reduced beyond what the formula
 * gives). False, leaving the outputs untouched, when dots is past
 * NOTE_VALUE_MAX_DOTS.
 */
bool standard_rhythm_dotted_factor(unsigned dots, unsigned long *out_numerator, unsigned long *out_denominator);

#endif
