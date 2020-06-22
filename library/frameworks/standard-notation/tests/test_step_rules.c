#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "spelling_test_util.h"
#include "semitone.h"
#include "step_rules.h"

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_NOTATION_STEP_RULE_COUNT; i++) {
        const Rule *rule = standard_notation_step_rule((StandardNotationStepRule)i);
        const Constraint *constraint = standard_notation_step_constraint((StandardNotationStepRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_notation_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_notation_step_rule(STANDARD_NOTATION_STEP_RULE_COUNT) == NULL);
}

static void test_half_steps(void) {
    PitchSpelling *e4 = spell(PITCH_LETTER_E, 0, 4);
    PitchSpelling *f4 = spell(PITCH_LETTER_F, 0, 4);
    PitchSpelling *b3 = spell(PITCH_LETTER_B, 0, 3);
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *g4 = spell(PITCH_LETTER_G, 0, 4);
    PitchSpelling *g_sharp4 = spell(PITCH_LETTER_G, 1, 4);
    PitchSpelling *a4 = spell(PITCH_LETTER_A, 0, 4);

    assert(standard_notation_is_half_step(e4, f4));
    assert(standard_notation_is_half_step(f4, e4)); /* either direction */
    assert(standard_notation_is_half_step(b3, c4));
    /* G-sharp sits a half step above G and a half step below A. */
    assert(standard_notation_is_half_step(g4, g_sharp4));
    assert(standard_notation_is_half_step(g_sharp4, a4));
    assert(!standard_notation_is_half_step(g4, a4));
    assert(!standard_notation_is_half_step(e4, e4));
    assert(!standard_notation_is_half_step(e4, NULL));

    pitch_spelling_destroy(e4);
    pitch_spelling_destroy(f4);
    pitch_spelling_destroy(b3);
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(g4);
    pitch_spelling_destroy(g_sharp4);
    pitch_spelling_destroy(a4);
}

static void test_whole_steps(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *d4 = spell(PITCH_LETTER_D, 0, 4);
    PitchSpelling *e4 = spell(PITCH_LETTER_E, 0, 4);
    PitchSpelling *f_sharp4 = spell(PITCH_LETTER_F, 1, 4);
    PitchSpelling *b3 = spell(PITCH_LETTER_B, 0, 3);
    PitchSpelling *c_sharp4 = spell(PITCH_LETTER_C, 1, 4);

    assert(standard_notation_half_steps_per_whole_step() == 2);
    assert(standard_notation_is_whole_step(c4, d4));
    assert(standard_notation_is_whole_step(d4, c4));
    assert(!standard_notation_is_whole_step(c4, e4));
    /* A whole step above E is F-sharp; above B is C-sharp. */
    assert(standard_notation_is_whole_step(e4, f_sharp4));
    assert(standard_notation_is_whole_step(b3, c_sharp4));
    assert(!standard_notation_is_whole_step(c4, NULL));

    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(d4);
    pitch_spelling_destroy(e4);
    pitch_spelling_destroy(f_sharp4);
    pitch_spelling_destroy(b3);
    pitch_spelling_destroy(c_sharp4);
}

static void test_semitone_above(void) {
    PitchSpelling *e4 = spell(PITCH_LETTER_E, 0, 4);
    PitchSpelling *f_sharp4 = spell(PITCH_LETTER_F, 1, 4);
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);

    /* Whole step above E is two keys right: F-sharp. */
    assert(standard_notation_semitone_above(e4, standard_notation_half_steps_per_whole_step()) ==
           standard_notation_semitone_number(f_sharp4));
    /* Whole step below C is two keys left: B-flat in octave 3. */
    PitchSpelling *b_flat3 = spell(PITCH_LETTER_B, -1, 3);
    assert(standard_notation_semitone_above(c4, -standard_notation_half_steps_per_whole_step()) ==
           standard_notation_semitone_number(b_flat3));

    pitch_spelling_destroy(e4);
    pitch_spelling_destroy(f_sharp4);
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(b_flat3);
}

/* Only E-F and B-C are half steps between adjacent white keys; the seven
   gaps around the octave add up to twelve. */
static void test_natural_half_steps(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                              PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int total = 0;
    int half_step_count = 0;
    for (int i = 0; i < 7; i++) {
        int size = standard_notation_natural_step_size(letters[i]);
        total += size;
        assert(size == 1 || size == 2);
        assert(standard_notation_natural_half_step_above(letters[i]) == (size == 1));
        if (size == 1) {
            half_step_count++;
        }
        assert(standard_notation_next_letter(letters[i]) == letters[(i + 1) % 7]);
    }
    assert(total == 12);
    assert(half_step_count == 2);
    assert(standard_notation_natural_half_step_above(PITCH_LETTER_E));
    assert(standard_notation_natural_half_step_above(PITCH_LETTER_B));
    assert(!standard_notation_natural_half_step_above(PITCH_LETTER_C));
}

int main(void) {
    test_rules_reference_framework();
    test_half_steps();
    test_whole_steps();
    test_semitone_above();
    test_natural_half_steps();
    printf("test_step_rules (standard-notation): ok\n");
    return 0;
}
