#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "octave_equivalence_rules.h"

static StaffPosition *position(int steps) {
    StaffPosition *p = NULL;
    assert(staff_position_create(steps, &p) == STAFF_POSITION_OK);
    return p;
}

static void test_rule_references_framework(void) {
    const Rule *rule = standard_notation_octave_rule(STANDARD_NOTATION_OCTAVE_RULE_OCTAVE_EQUIVALENCE);
    const Constraint *constraint =
        standard_notation_octave_constraint(STANDARD_NOTATION_OCTAVE_RULE_OCTAVE_EQUIVALENCE);
    assert(rule != NULL && constraint != NULL);
    assert(framework_identity_equals(rule_framework(rule), standard_notation_framework()));
    assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    assert(standard_notation_octave_rule(STANDARD_NOTATION_OCTAVE_RULE_COUNT) == NULL);
}

static void test_letters_loop(void) {
    assert(standard_notation_letter_count() == 7);
    assert(standard_notation_octave_equivalent(PITCH_LETTER_A, PITCH_LETTER_A));
    assert(!standard_notation_octave_equivalent(PITCH_LETTER_A, PITCH_LETTER_B));
}

/* Positions are octave-equivalent exactly when they are a multiple of
   seven steps apart, under any pitched clef. */
static void test_seven_steps_is_an_octave(void) {
    Clef *clefs[7];
    assert(clef_create_treble(&clefs[0]) == CLEF_OK);
    assert(clef_create_bass(&clefs[1]) == CLEF_OK);
    assert(clef_create_alto(&clefs[2]) == CLEF_OK);
    assert(clef_create_tenor(&clefs[3]) == CLEF_OK);
    assert(clef_create_soprano(&clefs[4]) == CLEF_OK);
    assert(clef_create_mezzo_soprano(&clefs[5]) == CLEF_OK);
    assert(clef_create_baritone(&clefs[6]) == CLEF_OK);

    for (int c = 0; c < 7; c++) {
        for (int a_steps = -8; a_steps <= 14; a_steps++) {
            for (int b_steps = -8; b_steps <= 14; b_steps++) {
                StaffPosition *a = position(a_steps), *b = position(b_steps);
                bool equivalent = standard_notation_positions_octave_equivalent(clefs[c], a, b);
                assert(equivalent == (((a_steps - b_steps) % 7) == 0));
                staff_position_destroy(a);
                staff_position_destroy(b);
            }
        }
        clef_destroy(clefs[c]);
    }
}

static void test_unpitched_and_null(void) {
    Clef *neutral = NULL;
    assert(clef_create_neutral(&neutral) == CLEF_OK);
    StaffPosition *a = position(0), *b = position(7);
    assert(!standard_notation_positions_octave_equivalent(neutral, a, b));
    assert(!standard_notation_positions_octave_equivalent(NULL, a, b));
    assert(!standard_notation_positions_octave_equivalent(neutral, NULL, b));
    staff_position_destroy(a);
    staff_position_destroy(b);
    clef_destroy(neutral);
}

int main(void) {
    test_rule_references_framework();
    test_letters_loop();
    test_seven_steps_is_an_octave();
    test_unpitched_and_null();
    printf("test_octave_equivalence_rules (standard-notation): ok\n");
    return 0;
}
