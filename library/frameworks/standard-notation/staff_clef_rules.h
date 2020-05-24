#ifndef URSATZ_FRAMEWORKS_STANDARD_NOTATION_STAFF_CLEF_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_NOTATION_STAFF_CLEF_RULES_H

#include <stdbool.h>

#include "pitch/clef.h"
#include "pitch/staff_position.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Staff and clef rules for standard-notation-v1:
 *
 *   FIVE_LINES      a staff consists of five evenly spaced horizontal lines
 *   CLEF_REFERENCE  a clef's sign marks its reference pitch on its anchor
 *                   line (treble G4 line 2, bass F3 line 4, alto/tenor
 *                   C4 lines 3/4)
 *   PITCH_HEIGHT    higher notes are written higher on the staff
 *
 * Rule/Constraint accessors follow chord_vocabulary: lazily built, owned
 * by this module, NULL only on allocation failure or invalid argument.
 */

typedef enum {
    STANDARD_NOTATION_STAFF_RULE_FIVE_LINES = 0,
    STANDARD_NOTATION_STAFF_RULE_CLEF_REFERENCE,
    STANDARD_NOTATION_STAFF_RULE_PITCH_HEIGHT,
    STANDARD_NOTATION_STAFF_RULE_COUNT
} StandardNotationStaffRule;

const Rule *standard_notation_staff_rule(StandardNotationStaffRule which);
const Constraint *standard_notation_staff_constraint(StandardNotationStaffRule which);

/* FIVE_LINES: a staff has exactly this many lines. */
int standard_notation_staff_line_count(void);

/* CLEF_REFERENCE: true when position is where clef places its reference pitch. */
bool standard_notation_clef_places_reference(const Clef *clef, const StaffPosition *position);

/* PITCH_HEIGHT: true when the pitch written at higher is above the pitch
   written at lower under clef, whenever higher is above lower on the staff
   (and equal pitches share a position). */
bool standard_notation_pitch_height_respected(const Clef *clef, const StaffPosition *lower,
                                              const StaffPosition *higher);

#endif
