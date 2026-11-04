#include "accidental_placement_rules.h"

#include <stddef.h>

#include "accidental_rules.h"
#include "rule_pair.h"

static const char *const NAMES[STANDARD_NOTATION_ACCIDENTAL_PLACEMENT_RULE_COUNT] = {
    "accidental-left-of-note", "accidental-same-position", "accidental-per-note"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_notation_framework, "standard-notation",
    NAMES, STANDARD_NOTATION_ACCIDENTAL_PLACEMENT_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_notation_accidental_placement, StandardNotationAccidentalPlacementRule, STANDARD_NOTATION_ACCIDENTAL_PLACEMENT_RULE_COUNT, g_table)

bool standard_notation_accidental_side_valid(AccidentalSide side) {
    return side == ACCIDENTAL_SIDE_LEFT;
}

bool standard_notation_accidental_position_valid(const StaffPosition *note_position,
                                                 const StaffPosition *accidental_position) {
    if (note_position == NULL || accidental_position == NULL) {
        return false;
    }
    return staff_position_equals(note_position, accidental_position);
}

bool standard_notation_accidentals_apply_per_note(const StaffPosition *first_position,
                                                  const Accidental *first_accidental, const PitchSpelling *first,
                                                  const StaffPosition *second_position,
                                                  const Accidental *second_accidental,
                                                  const PitchSpelling *second) {
    if (first_position == NULL || first_accidental == NULL || first == NULL || second_position == NULL ||
        second_accidental == NULL || second == NULL) {
        return false;
    }
    if (!staff_position_equals(first_position, second_position)) {
        return true;
    }
    return standard_notation_spelling_matches_accidental(first, first_accidental) &&
           standard_notation_spelling_matches_accidental(second, second_accidental);
}
