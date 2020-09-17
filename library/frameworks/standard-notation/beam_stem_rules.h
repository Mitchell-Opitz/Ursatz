#ifndef URSATZ_FRAMEWORKS_STANDARD_NOTATION_BEAM_STEM_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_NOTATION_BEAM_STEM_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/staff_position.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "stem_rules.h"

/*
 * Beamed group stem direction rule for standard-notation-v1. Notes
 * joined by a beam share one stem direction, so each note's own
 * above/below-the-middle-line direction (stem_rules.h) no longer applies
 * to it:
 *
 *   FARTHEST_NOTE  the note farthest from the middle line decides the
 *                  direction for the whole group: farthest above means
 *                  down-stems, farthest below means up-stems. When the
 *                  farthest notes above and below are equally far, or
 *                  every note is on the middle line, either direction is
 *                  allowed.
 *
 * Distance is in staff steps (lines and spaces), the unit StaffPosition
 * counts in.
 */

typedef enum {
    STANDARD_NOTATION_BEAM_STEM_RULE_FARTHEST_NOTE = 0,
    STANDARD_NOTATION_BEAM_STEM_RULE_COUNT
} StandardNotationBeamStemRule;

const Rule *standard_notation_beam_stem_rule(StandardNotationBeamStemRule which);
const Constraint *standard_notation_beam_stem_constraint(StandardNotationBeamStemRule which);

/*
 * FARTHEST_NOTE: number of directions allowed for the beamed group whose
 * notes are at `positions` (2 when the group may go either way, else 1;
 * 0 for an empty group or a NULL entry).
 */
int standard_notation_beamed_stem_direction_option_count(const StaffPosition *const *positions, size_t count);

/* FARTHEST_NOTE: the required direction, or STEM_DIRECTION_DOWN as the
   default when either is allowed (same convention as
   standard_notation_stem_direction_default). */
StemDirection standard_notation_beamed_stem_direction_default(const StaffPosition *const *positions, size_t count);

/* FARTHEST_NOTE: true when direction is allowed for the whole group.
   False for an empty group or a NULL entry. */
bool standard_notation_beamed_stem_direction_valid(const StaffPosition *const *positions, size_t count,
                                                   StemDirection direction);

#endif
