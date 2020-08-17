#include <assert.h>
#include <stdio.h>

#include "dot_rules.h"
#include "rule_check.h"

static NoteValue *make(int exponent) {
    NoteValue *value = NULL;
    assert(note_value_create(exponent, &value) == NOTE_VALUE_OK);
    return value;
}

static void assert_duration(const Duration *duration, long numerator, unsigned long denominator) {
    assert(duration_numerator(duration) == numerator);
    assert(duration_denominator(duration) == denominator);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_rhythm_dot_rule, standard_rhythm_dot_constraint,
                                    STANDARD_RHYTHM_DOT_RULE_COUNT);
}

/* The first dot adds half the base: a dotted quarter is three eighths. */
static void test_first_dot_adds_half(void) {
    NoteValue *quarter = make(2);
    Duration *added = NULL;
    assert(standard_rhythm_dot_added_duration(quarter, 1, &added));
    assert_duration(added, 1, 8); /* an eighth */

    Duration *base = NULL;
    Duration *sum = NULL;
    assert(note_value_duration(quarter, &base) == DURATION_OK);
    assert(duration_add(base, added, &sum) == DURATION_OK);
    assert_duration(sum, 3, 8); /* three eighths */

    duration_destroy(added);
    duration_destroy(base);
    duration_destroy(sum);
    note_value_destroy(quarter);
}

/* Each further dot adds half the previous: quarter + eighth + sixteenth. */
static void test_subsequent_dots_halve(void) {
    NoteValue *quarter = make(2);
    long expected_denominators[] = {0, 8, 16, 32, 64};
    for (unsigned n = 1; n <= NOTE_VALUE_MAX_DOTS; n++) {
        Duration *added = NULL;
        assert(standard_rhythm_dot_added_duration(quarter, n, &added));
        assert_duration(added, 1, (unsigned long)expected_denominators[n]);
        duration_destroy(added);
    }

    /* a double-dotted quarter is quarter + eighth + sixteenth */
    NoteValue *double_dotted = NULL;
    assert(note_value_create_dotted(2, 2, &double_dotted) == NOTE_VALUE_OK);
    Duration *total = NULL;
    assert(note_value_duration(double_dotted, &total) == DURATION_OK);
    assert_duration(total, 7, 16);

    duration_destroy(total);
    note_value_destroy(double_dotted);
    note_value_destroy(quarter);
}

/* Dotted whole is three halves; a breve's first dot adds a whole. */
static void test_other_bases(void) {
    NoteValue *whole = make(0);
    NoteValue *breve = make(-1);
    Duration *added = NULL;

    assert(standard_rhythm_dot_added_duration(whole, 1, &added));
    assert_duration(added, 1, 2);
    duration_destroy(added);

    assert(standard_rhythm_dot_added_duration(breve, 1, &added));
    assert_duration(added, 1, 1);
    duration_destroy(added);

    note_value_destroy(whole);
    note_value_destroy(breve);
}

static void test_added_duration_declines(void) {
    NoteValue *quarter = make(2);
    Duration *added = NULL;
    assert(!standard_rhythm_dot_added_duration(quarter, 0, &added));
    assert(!standard_rhythm_dot_added_duration(quarter, NOTE_VALUE_MAX_DOTS + 1, &added));
    assert(!standard_rhythm_dot_added_duration(NULL, 1, &added));
    assert(added == NULL);
    note_value_destroy(quarter);
}

/* With n dots a value is (2^(n+1) - 1) / 2^n of its base. */
static void test_total_factor(void) {
    unsigned long numerator = 0;
    unsigned long denominator = 0;
    assert(standard_rhythm_dotted_factor(0, &numerator, &denominator) && numerator == 1 && denominator == 1);
    assert(standard_rhythm_dotted_factor(1, &numerator, &denominator) && numerator == 3 && denominator == 2);
    assert(standard_rhythm_dotted_factor(2, &numerator, &denominator) && numerator == 7 && denominator == 4);
    assert(standard_rhythm_dotted_factor(3, &numerator, &denominator) && numerator == 15 && denominator == 8);

    numerator = 9;
    assert(!standard_rhythm_dotted_factor(NOTE_VALUE_MAX_DOTS + 1, &numerator, &denominator));
    assert(numerator == 9);
}

int main(void) {
    test_rules_reference_framework();
    test_first_dot_adds_half();
    test_subsequent_dots_halve();
    test_other_bases();
    test_added_duration_declines();
    test_total_factor();
    printf("test_dot_rules (standard-rhythm): ok\n");
    return 0;
}
