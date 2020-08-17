#ifndef URSATZ_FRAMEWORKS_STANDARD_RHYTHM_REST_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_RHYTHM_REST_RULES_H

#include <stdbool.h>

#include "name_rules.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "time/note_value.h"

/*
 * Rest value rules for standard-rhythm-v1. A rest is a silence; each
 * note value has a corresponding rest of the same length, so a rest is
 * written with the same NoteValue as its note:
 *
 *   REST_NAMES      a rest is named for its value: whole rest, half rest,
 *                   quarter rest, eighth rest, sixteenth rest (British:
 *                   semibreve rest, minim rest, crotchet rest, ...);
 *                   the rare double-length rest is the breve rest
 *   REST_FLAGS      from the eighth rest on, each flag halves the rest:
 *                   one for the eighth, two for the sixteenth, ...
 *   REST_HIERARCHY  like notes, each rest divides into two of the next
 *                   smaller: a breve rest equals two whole rests
 *
 * Where a rest sits on the staff (a whole rest hangs from a line, a
 * half rest sits on one, a breve rest fills the second space from the
 * top) is engraving craft and is not covered.
 */

typedef enum {
    STANDARD_RHYTHM_REST_RULE_NAMES = 0,
    STANDARD_RHYTHM_REST_RULE_FLAGS,
    STANDARD_RHYTHM_REST_RULE_HIERARCHY,
    STANDARD_RHYTHM_REST_RULE_COUNT
} StandardRhythmRestRule;

const Rule *standard_rhythm_rest_rule(StandardRhythmRestRule which);
const Constraint *standard_rhythm_rest_constraint(StandardRhythmRestRule which);

/* REST_NAMES: the rest's name in the given system, or NULL when the
   value is past the sixty-fourth. Same naming systems as note names. */
const char *standard_rhythm_rest_name(const NoteValue *note_value, StandardRhythmNaming naming);

/* REST_FLAGS: flags on the rest (0 up to the quarter rest). */
int standard_rhythm_rest_flag_count(const NoteValue *note_value);

/* REST_HIERARCHY: how many `smaller` rests make up `larger`. False, leaving
   *out_count untouched, when either is dotted or `smaller` is larger. */
bool standard_rhythm_rest_count_within(const NoteValue *larger, const NoteValue *smaller, unsigned long *out_count);

#endif
