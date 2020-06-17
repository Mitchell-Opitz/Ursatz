#ifndef URSATZ_FRAMEWORKS_STANDARD_NOTATION_ACCIDENTAL_PLACEMENT_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_NOTATION_ACCIDENTAL_PLACEMENT_RULES_H

#include <stdbool.h>

#include "pitch/accidental.h"
#include "pitch/pitch_spelling.h"
#include "pitch/staff_position.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Accidental placement rules for standard-notation-v1:
 *
 *   LEFT_OF_NOTE   accidentals are written to the left of the note,
 *                  regardless of stem direction
 *   SAME_POSITION  an accidental is written directly across the line or
 *                  space on which its note appears
 *   PER_NOTE       when two notes share a line or space but differ in
 *                  accidental, each accidental applies to its own note
 *                  (a B-flat and a B-natural side by side)
 */

typedef enum {
    STANDARD_NOTATION_ACCIDENTAL_PLACEMENT_RULE_LEFT_OF_NOTE = 0,
    STANDARD_NOTATION_ACCIDENTAL_PLACEMENT_RULE_SAME_POSITION,
    STANDARD_NOTATION_ACCIDENTAL_PLACEMENT_RULE_PER_NOTE,
    STANDARD_NOTATION_ACCIDENTAL_PLACEMENT_RULE_COUNT
} StandardNotationAccidentalPlacementRule;

typedef enum {
    ACCIDENTAL_SIDE_LEFT,
    ACCIDENTAL_SIDE_RIGHT
} AccidentalSide;

const Rule *standard_notation_accidental_placement_rule(StandardNotationAccidentalPlacementRule which);
const Constraint *standard_notation_accidental_placement_constraint(StandardNotationAccidentalPlacementRule which);

/* LEFT_OF_NOTE: true only when the accidental is on the left of its note. */
bool standard_notation_accidental_side_valid(AccidentalSide side);

/* SAME_POSITION: true when the accidental sits on the same staff position
   as its note. False for NULL. */
bool standard_notation_accidental_position_valid(const StaffPosition *note_position,
                                                 const StaffPosition *accidental_position);

/*
 * PER_NOTE: for two notes written at the same staff position, true when
 * each note's own accidental determines its own alteration -- i.e. the
 * two spellings differ in sounding pitch exactly as their accidentals
 * differ, with neither accidental bleeding onto the other note. Notes at
 * different positions are not covered by this rule and return true.
 * False for NULL.
 */
bool standard_notation_accidentals_apply_per_note(const StaffPosition *first_position,
                                                  const Accidental *first_accidental, const PitchSpelling *first,
                                                  const StaffPosition *second_position,
                                                  const Accidental *second_accidental,
                                                  const PitchSpelling *second);

#endif
