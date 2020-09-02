#ifndef URSATZ_FRAMEWORKS_STANDARD_NOTATION_FLAG_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_NOTATION_FLAG_RULES_H

#include <stdbool.h>

#include "pitch/staff_position.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "stem_rules.h"

/*
 * Flag direction rules for standard-notation-v1. A flag's facing is
 * determined by its stem's direction (the stem rules in stem_rules.h
 * decide that from the note's staff position):
 *
 *   DOWN_STEM_INWARD   a note above the middle line has a down-stem, on
 *                      the left of the notehead, and an inward-facing
 *                      flag (facing right)
 *   UP_STEM_OUTWARD    a note below the middle line has an up-stem, on
 *                      the right of the notehead, and an outward-facing
 *                      flag (facing left)
 *   MIDDLE_EITHER      a note on the middle line may be flagged either
 *                      way, usually following the contour of the line:
 *                      the flag goes with whichever stem direction is
 *                      chosen
 *
 * Inward and outward are Open Music Theory's terms, measured against the
 * notehead the stem is attached to. Beamed notes have no flags (beams
 * replace them), so these rules apply to unbeamed notes.
 */

typedef enum {
    STANDARD_NOTATION_FLAG_RULE_DOWN_STEM_INWARD = 0,
    STANDARD_NOTATION_FLAG_RULE_UP_STEM_OUTWARD,
    STANDARD_NOTATION_FLAG_RULE_MIDDLE_EITHER,
    STANDARD_NOTATION_FLAG_RULE_COUNT
} StandardNotationFlagRule;

typedef enum {
    FLAG_FACING_INWARD,
    FLAG_FACING_OUTWARD
} FlagFacing;

const Rule *standard_notation_flag_rule(StandardNotationFlagRule which);
const Constraint *standard_notation_flag_constraint(StandardNotationFlagRule which);

/* DOWN_STEM_INWARD / UP_STEM_OUTWARD: the facing that goes with a stem
   direction. */
FlagFacing standard_notation_flag_facing(StemDirection stem_direction);

/* MIDDLE_EITHER: number of flag facings allowed at position: 2 on the
   middle line, else 1. 0 for a NULL position. */
int standard_notation_flag_facing_option_count(const StaffPosition *position);

/* The single required facing at position; on the middle line (either is
   allowed) the facing of the default stem direction. */
FlagFacing standard_notation_flag_facing_default(const StaffPosition *position);

/* True when facing is allowed at position: the stem direction it goes
   with is allowed there. False for a NULL position. */
bool standard_notation_flag_facing_valid(const StaffPosition *position, FlagFacing facing);

#endif
