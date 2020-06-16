#ifndef URSATZ_FRAMEWORKS_STANDARD_NOTATION_ENHARMONIC_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_NOTATION_ENHARMONIC_RULES_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Enharmonic equivalence rule for standard-notation-v1:
 *
 *   ENHARMONIC_EQUIVALENCE  notes are enharmonically equivalent when they
 *                           are spelled differently but sound the same
 *                           (C-sharp and D-flat; D, C double sharp, and
 *                           E double flat)
 *
 * "Sound the same" is 12-TET semitone number equality (semitone.h).
 * pitch_spelling_equals stays strict -- enharmonic spellings are NOT
 * value-equal there (pitch/pitch_spelling.h); this rule is the
 * theory-relative layer on top of it, kept in the framework so a
 * different tuning or spelling system can define its own.
 */

typedef enum {
    STANDARD_NOTATION_ENHARMONIC_RULE_ENHARMONIC_EQUIVALENCE = 0,
    STANDARD_NOTATION_ENHARMONIC_RULE_COUNT
} StandardNotationEnharmonicRule;

const Rule *standard_notation_enharmonic_rule(StandardNotationEnharmonicRule which);
const Constraint *standard_notation_enharmonic_constraint(StandardNotationEnharmonicRule which);

/* Same semitone number, regardless of spelling. A spelling is sound-equal
   to itself. False for NULL. */
bool standard_notation_sounds_the_same(const PitchSpelling *a, const PitchSpelling *b);

/* ENHARMONIC_EQUIVALENCE: sounds the same AND spelled differently. */
bool standard_notation_enharmonic_equivalent(const PitchSpelling *a, const PitchSpelling *b);

#endif
