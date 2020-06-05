#ifndef URSATZ_FRAMEWORKS_STANDARD_NOTATION_CLEF_LETTER_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_NOTATION_CLEF_LETTER_RULES_H

#include <stdbool.h>

#include "pitch/clef.h"
#include "pitch/pitch_spelling.h"
#include "pitch/staff_position.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Clef letter-name rules for standard-notation-v1:
 *
 *   LINE_SPACE_LETTERS   each clef gives the lines and spaces of the staff
 *                        fixed letter names, e.g. treble lines E G B D F
 *                        and spaces F A C E, bass lines G B D F A and
 *                        spaces A C E G
 *   LEDGER_CONTINUATION  ledger lines and the spaces beyond the staff are
 *                        named as if the staff simply continued upward or
 *                        downward, for any clef
 *
 * Neutral clefs name no letters; every check here is false for them.
 */

typedef enum {
    STANDARD_NOTATION_CLEF_LETTER_RULE_LINE_SPACE_LETTERS = 0,
    STANDARD_NOTATION_CLEF_LETTER_RULE_LEDGER_CONTINUATION,
    STANDARD_NOTATION_CLEF_LETTER_RULE_COUNT
} StandardNotationClefLetterRule;

const Rule *standard_notation_clef_letter_rule(StandardNotationClefLetterRule which);
const Constraint *standard_notation_clef_letter_constraint(StandardNotationClefLetterRule which);

#define STANDARD_NOTATION_STAFF_LINES 5
#define STANDARD_NOTATION_STAFF_SPACES 4

/*
 * LINE_SPACE_LETTERS: letter name of the nth line (1 = bottom .. 5 = top)
 * or space (1 = lowest .. 4 = highest) under clef. Returns false, leaving
 * *out untouched, for an unpitched clef, NULL clef, or n out of range.
 */
bool standard_notation_clef_line_letter(const Clef *clef, int line, PitchLetter *out);
bool standard_notation_clef_space_letter(const Clef *clef, int space, PitchLetter *out);

/*
 * LEDGER_CONTINUATION: true when the letter written at position is the
 * letter reached by stepping through the A-G loop from the nearest edge
 * of the staff (top line going up, bottom line going down). Positions on
 * the staff itself are trivially true. False for an unpitched clef.
 */
bool standard_notation_ledger_continues_letters(const Clef *clef, const StaffPosition *position);

#endif
