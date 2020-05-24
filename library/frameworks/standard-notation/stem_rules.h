#ifndef URSATZ_FRAMEWORKS_STANDARD_NOTATION_STEM_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_NOTATION_STEM_RULES_H

#include <stdbool.h>

#include "pitch/staff_position.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Stem direction rules for standard-notation-v1:
 *
 *   ABOVE_MIDDLE_DOWN  notes above the middle line have stems pointing down
 *   BELOW_MIDDLE_UP    notes below the middle line have stems pointing up
 *   ON_MIDDLE_EITHER   notes on the middle line may point either way,
 *                      depending on surrounding notes
 */

typedef enum {
    STANDARD_NOTATION_STEM_RULE_ABOVE_MIDDLE_DOWN = 0,
    STANDARD_NOTATION_STEM_RULE_BELOW_MIDDLE_UP,
    STANDARD_NOTATION_STEM_RULE_ON_MIDDLE_EITHER,
    STANDARD_NOTATION_STEM_RULE_COUNT
} StandardNotationStemRule;

typedef enum {
    STEM_DIRECTION_UP,
    STEM_DIRECTION_DOWN
} StemDirection;

const Rule *standard_notation_stem_rule(StandardNotationStemRule which);
const Constraint *standard_notation_stem_constraint(StandardNotationStemRule which);

/* Number of directions allowed at position: 2 on the middle line, else 1. */
int standard_notation_stem_direction_option_count(const StaffPosition *position);

/* The single required direction at position. On the middle line (either
   direction is allowed) returns STEM_DIRECTION_DOWN as the default;
   use standard_notation_stem_direction_valid to test a specific choice. */
StemDirection standard_notation_stem_direction_default(const StaffPosition *position);

/* True when direction is allowed at position. */
bool standard_notation_stem_direction_valid(const StaffPosition *position, StemDirection direction);

#endif
