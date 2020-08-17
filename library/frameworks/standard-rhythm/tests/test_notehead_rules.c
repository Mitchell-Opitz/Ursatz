#include <assert.h>
#include <stdio.h>

#include "notehead_rules.h"
#include "rule_check.h"

static NoteValue *make(int exponent) {
    NoteValue *value = NULL;
    assert(note_value_create(exponent, &value) == NOTE_VALUE_OK);
    return value;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_rhythm_notehead_rule, standard_rhythm_notehead_constraint,
                                    STANDARD_RHYTHM_NOTEHEAD_RULE_COUNT);
}

/* Quarter and shorter are filled; half and longer are not. */
static void test_filled(void) {
    for (int exponent = NOTE_VALUE_MIN_EXPONENT; exponent <= NOTE_VALUE_MAX_EXPONENT; exponent++) {
        NoteValue *value = make(exponent);
        assert(standard_rhythm_notehead_filled(value) == (exponent >= 2));
        note_value_destroy(value);
    }
    assert(!standard_rhythm_notehead_filled(NULL));
}

/* Filled noteheads always have stems; unfilled may or may not. */
static void test_stems(void) {
    for (int exponent = NOTE_VALUE_MIN_EXPONENT; exponent <= NOTE_VALUE_MAX_EXPONENT; exponent++) {
        NoteValue *value = make(exponent);
        if (exponent >= 2) {
            assert(standard_rhythm_stem_requirement(value) == STEM_REQUIREMENT_REQUIRED);
            assert(standard_rhythm_stem_allowed(value, true));
            assert(!standard_rhythm_stem_allowed(value, false));
        } else {
            assert(standard_rhythm_stem_requirement(value) == STEM_REQUIREMENT_OPTIONAL);
            assert(standard_rhythm_stem_allowed(value, true));
            assert(standard_rhythm_stem_allowed(value, false));
        }
        note_value_destroy(value);
    }
    assert(!standard_rhythm_stem_allowed(NULL, true));
}

/* One flag for the eighth, one more for each further halving. */
static void test_flags(void) {
    for (int exponent = NOTE_VALUE_MIN_EXPONENT; exponent <= NOTE_VALUE_MAX_EXPONENT; exponent++) {
        NoteValue *value = make(exponent);
        int expected = exponent > 2 ? exponent - 2 : 0;
        assert(standard_rhythm_flag_count(value) == expected);

        /* flags only on a stem, and exactly the value's own count */
        assert(standard_rhythm_flags_allowed(value, true, expected));
        assert(!standard_rhythm_flags_allowed(value, true, expected + 1));
        assert(standard_rhythm_flags_allowed(value, false, 0));
        if (expected > 0) {
            assert(!standard_rhythm_flags_allowed(value, false, expected));
        }
        note_value_destroy(value);
    }
    assert(standard_rhythm_flag_count(NULL) == 0);
    assert(!standard_rhythm_flags_allowed(NULL, true, 0));
}

/* Three ways to halve: add a stem, fill the notehead, add a flag. */
static void test_halving_steps(void) {
    NoteValue *breve = make(-1);
    NoteValue *whole = make(0);
    NoteValue *half = make(1);
    NoteValue *quarter = make(2);
    NoteValue *eighth = make(3);
    NoteValue *sixteenth = make(4);

    assert(standard_rhythm_halving_step(breve) == HALVING_STEP_NONE);
    assert(standard_rhythm_halving_step(whole) == HALVING_STEP_ADD_STEM);
    assert(standard_rhythm_halving_step(half) == HALVING_STEP_FILL_NOTEHEAD);
    assert(standard_rhythm_halving_step(quarter) == HALVING_STEP_ADD_FLAG);
    assert(standard_rhythm_halving_step(eighth) == HALVING_STEP_ADD_FLAG);
    assert(standard_rhythm_halving_step(sixteenth) == HALVING_STEP_ADD_FLAG);
    assert(standard_rhythm_halving_step(NULL) == HALVING_STEP_NONE);

    note_value_destroy(breve);
    note_value_destroy(whole);
    note_value_destroy(half);
    note_value_destroy(quarter);
    note_value_destroy(eighth);
    note_value_destroy(sixteenth);
}

/* A dotted value is drawn like its base value. */
static void test_dot_does_not_change_shape(void) {
    NoteValue *dotted = NULL;
    assert(note_value_create_dotted(3, 2, &dotted) == NOTE_VALUE_OK);
    assert(standard_rhythm_notehead_filled(dotted));
    assert(standard_rhythm_flag_count(dotted) == 1);
    note_value_destroy(dotted);
}

int main(void) {
    test_rules_reference_framework();
    test_filled();
    test_stems();
    test_flags();
    test_halving_steps();
    test_dot_does_not_change_shape();
    printf("test_notehead_rules (standard-rhythm): ok\n");
    return 0;
}
