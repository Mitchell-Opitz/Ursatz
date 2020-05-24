#ifndef URSATZ_FRAMEWORKS_STANDARD_NOTATION_SECOND_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_NOTATION_SECOND_RULES_H

#include <stdbool.h>

#include "pitch/staff_position.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Second displacement rule for standard-notation-v1:
 *
 *   LOWER_NOTE_LEFT  a harmonic second sits on an adjacent line and space,
 *                    so one notehead is displaced to the other side of
 *                    the stem; the lower note always goes on the left,
 *                    whichever way the stem points. Seconds are neither
 *                    stacked on one another nor drawn with the lower
 *                    note on the right.
 */

typedef enum {
    STANDARD_NOTATION_SECOND_RULE_LOWER_NOTE_LEFT = 0,
    STANDARD_NOTATION_SECOND_RULE_COUNT
} StandardNotationSecondRule;

typedef enum {
    NOTEHEAD_SIDE_LEFT,
    NOTEHEAD_SIDE_RIGHT,
    NOTEHEAD_SIDE_CENTER /* not displaced: directly on the stem line */
} NoteheadSide;

const Rule *standard_notation_second_rule(StandardNotationSecondRule which);
const Constraint *standard_notation_second_constraint(StandardNotationSecondRule which);

/* True when the two positions are adjacent (a line and the space beside it). */
bool standard_notation_is_second(const StaffPosition *a, const StaffPosition *b);

/*
 * LOWER_NOTE_LEFT: true when the pair is drawn correctly. For a second,
 * the lower note must be LEFT and the upper RIGHT. Positions that do not
 * form a second are not governed by this rule and return true.
 */
bool standard_notation_second_displacement_valid(const StaffPosition *lower, NoteheadSide lower_side,
                                                 const StaffPosition *upper, NoteheadSide upper_side);

#endif
