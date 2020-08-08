#ifndef URSATZ_FRAMEWORKS_STANDARD_RHYTHM_HIERARCHY_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_RHYTHM_HIERARCHY_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/note_value.h"

/*
 * Note value hierarchy rules for standard-rhythm-v1. Note values are
 * defined relative to one another, like a pizza cut into halves,
 * quarters, eighths:
 *
 *   HALVING  each value divides into two of the next smaller value: a
 *            whole note into two half notes, a half note into two
 *            quarter notes, and so on
 *   COUNT    so a value divides into 2^n of any value n steps smaller:
 *            a whole note is two half notes, four quarter notes, eight
 *            eighth notes
 *
 * The hierarchy is stated for undotted values. A dotted value is not a
 * step in it, so both rules decline (return false) for a dotted one;
 * dots are dot_rules.h.
 */

typedef enum {
    STANDARD_RHYTHM_HIERARCHY_RULE_HALVING = 0,
    STANDARD_RHYTHM_HIERARCHY_RULE_COUNT_WITHIN,
    STANDARD_RHYTHM_HIERARCHY_RULE_COUNT
} StandardRhythmHierarchyRule;

const Rule *standard_rhythm_hierarchy_rule(StandardRhythmHierarchyRule which);
const Constraint *standard_rhythm_hierarchy_constraint(StandardRhythmHierarchyRule which);

/*
 * HALVING: the next smaller value, half the length. False, leaving *out
 * untouched, for a dotted value or when the next smaller value is past
 * NOTE_VALUE_MAX_EXPONENT. On success the caller owns *out.
 */
bool standard_rhythm_halved(const NoteValue *note_value, NoteValue **out);

/*
 * COUNT: how many of `smaller` make up `larger` (2^(difference of
 * exponents)). False, leaving *out_count untouched, when either is
 * dotted or `smaller` is larger than `larger`.
 */
bool standard_rhythm_count_within(const NoteValue *larger, const NoteValue *smaller, unsigned long *out_count);

#endif
