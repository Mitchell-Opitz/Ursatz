#ifndef URSATZ_FRAMEWORKS_STANDARD_NOTATION_ACCIDENTAL_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_NOTATION_ACCIDENTAL_RULES_H

#include <stdbool.h>

#include "pitch/accidental.h"
#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Accidental effect rules for standard-notation-v1. An accidental
 * changes the pitch of a note:
 *
 *   SHARP_RAISES         a sharp raises a note by a half step
 *   FLAT_LOWERS          a flat lowers a note by a half step
 *   NATURAL_CANCELS      a natural cancels a previous accidental
 *   DOUBLE_WHOLE_STEP    a double sharp raises a note by a whole step (two
 *                        half steps); a double flat lowers it by a whole step
 *
 * An accidental sets a note's alteration absolutely, relative to its
 * letter -- it does not accumulate on top of an earlier one. That is
 * what lets a natural cancel: it sets the alteration back to zero.
 */

typedef enum {
    STANDARD_NOTATION_ACCIDENTAL_RULE_SHARP_RAISES = 0,
    STANDARD_NOTATION_ACCIDENTAL_RULE_FLAT_LOWERS,
    STANDARD_NOTATION_ACCIDENTAL_RULE_NATURAL_CANCELS,
    STANDARD_NOTATION_ACCIDENTAL_RULE_DOUBLE_WHOLE_STEP,
    STANDARD_NOTATION_ACCIDENTAL_RULE_COUNT
} StandardNotationAccidentalRule;

const Rule *standard_notation_accidental_rule(StandardNotationAccidentalRule which);
const Constraint *standard_notation_accidental_constraint(StandardNotationAccidentalRule which);

/* Half steps the accidental moves a note: -2, -1, 0, +1, +2. */
int standard_notation_accidental_half_steps(const Accidental *accidental);

bool standard_notation_accidental_raises(const Accidental *accidental);
bool standard_notation_accidental_lowers(const Accidental *accidental);

/* NATURAL_CANCELS: true only for a natural. */
bool standard_notation_accidental_cancels(const Accidental *accidental);

/*
 * NATURAL_CANCELS: the alteration in force for a note. With no written
 * accidental (NULL) the prior alteration carries; any written accidental,
 * natural included, replaces it.
 */
int standard_notation_effective_offset(const Accidental *written, int prior_offset);

/* True when the spelling's accidental is within the five accidentals
   this framework recognizes (-2..+2). */
bool standard_notation_spelling_accidental_valid(const PitchSpelling *spelling);

/* True when the spelling carries exactly the alteration the accidental names. */
bool standard_notation_spelling_matches_accidental(const PitchSpelling *spelling, const Accidental *accidental);

#endif
