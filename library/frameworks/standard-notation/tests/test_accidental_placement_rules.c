#include <assert.h>
#include <stdio.h>

#include "accidental_placement_rules.h"
#include "framework.h"
#include "spelling_test_util.h"

static Accidental *accidental(AccidentalKind kind) {
    Accidental *a = NULL;
    assert(accidental_create(kind, &a) == ACCIDENTAL_OK);
    return a;
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_NOTATION_ACCIDENTAL_PLACEMENT_RULE_COUNT; i++) {
        const Rule *rule = standard_notation_accidental_placement_rule((StandardNotationAccidentalPlacementRule)i);
        const Constraint *constraint =
            standard_notation_accidental_placement_constraint((StandardNotationAccidentalPlacementRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_notation_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_notation_accidental_placement_rule(STANDARD_NOTATION_ACCIDENTAL_PLACEMENT_RULE_COUNT) == NULL);
}

static void test_left_of_note(void) {
    assert(standard_notation_accidental_side_valid(ACCIDENTAL_SIDE_LEFT));
    assert(!standard_notation_accidental_side_valid(ACCIDENTAL_SIDE_RIGHT));
}

/* Directly across the line or space of the note, for lines and spaces alike. */
static void test_same_position(void) {
    for (int steps = -3; steps <= 11; steps++) {
        StaffPosition *note = staff_at(steps);
        StaffPosition *same = staff_at(steps);
        StaffPosition *above = staff_at(steps + 1);
        assert(standard_notation_accidental_position_valid(note, same));
        assert(!standard_notation_accidental_position_valid(note, above));
        staff_position_destroy(note);
        staff_position_destroy(same);
        staff_position_destroy(above);
    }
    StaffPosition *p = staff_at(2);
    assert(!standard_notation_accidental_position_valid(p, NULL));
    assert(!standard_notation_accidental_position_valid(NULL, p));
    staff_position_destroy(p);
}

/* B-flat then B-natural on the same line: each accidental applies to its
   own note. */
static void test_per_note(void) {
    StaffPosition *b_line = staff_at(4);
    StaffPosition *b_line_again = staff_at(4);
    Accidental *flat = accidental(ACCIDENTAL_FLAT), *natural = accidental(ACCIDENTAL_NATURAL);
    PitchSpelling *b_flat = spell(PITCH_LETTER_B, -1, 4);
    PitchSpelling *b_natural = spell(PITCH_LETTER_B, 0, 4);

    assert(standard_notation_accidentals_apply_per_note(b_line, flat, b_flat, b_line_again, natural, b_natural));

    /* The flat bleeding onto the second note is a violation. */
    assert(!standard_notation_accidentals_apply_per_note(b_line, flat, b_flat, b_line_again, natural, b_flat));

    /* Different positions are outside the rule. */
    StaffPosition *elsewhere = staff_at(6);
    assert(standard_notation_accidentals_apply_per_note(b_line, flat, b_flat, elsewhere, natural, b_flat));

    assert(!standard_notation_accidentals_apply_per_note(NULL, flat, b_flat, b_line_again, natural, b_natural));

    staff_position_destroy(b_line);
    staff_position_destroy(b_line_again);
    staff_position_destroy(elsewhere);
    accidental_destroy(flat);
    accidental_destroy(natural);
    pitch_spelling_destroy(b_flat);
    pitch_spelling_destroy(b_natural);
}

int main(void) {
    test_rules_reference_framework();
    test_left_of_note();
    test_same_position();
    test_per_note();
    printf("test_accidental_placement_rules (standard-notation): ok\n");
    return 0;
}
