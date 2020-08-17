#include <assert.h>
#include <stdio.h>

#include "hierarchy_rules.h"
#include "rule_check.h"

static NoteValue *make(int exponent) {
    NoteValue *value = NULL;
    assert(note_value_create(exponent, &value) == NOTE_VALUE_OK);
    return value;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_rhythm_hierarchy_rule, standard_rhythm_hierarchy_constraint,
                                    STANDARD_RHYTHM_HIERARCHY_RULE_COUNT);
}

/* Each value divides into two of the next smaller one. */
static void test_halving(void) {
    for (int exponent = NOTE_VALUE_MIN_EXPONENT; exponent < NOTE_VALUE_MAX_EXPONENT; exponent++) {
        NoteValue *value = make(exponent);
        NoteValue *half = NULL;
        assert(standard_rhythm_halved(value, &half));
        assert(note_value_exponent(half) == exponent + 1);

        /* two of the smaller equal the larger, exactly */
        Duration *larger = NULL;
        Duration *smaller = NULL;
        Duration *sum = NULL;
        assert(note_value_duration(value, &larger) == DURATION_OK);
        assert(note_value_duration(half, &smaller) == DURATION_OK);
        assert(duration_add(smaller, smaller, &sum) == DURATION_OK);
        assert(duration_equals(sum, larger));

        duration_destroy(larger);
        duration_destroy(smaller);
        duration_destroy(sum);
        note_value_destroy(half);
        note_value_destroy(value);
    }
}

static void test_halving_declines(void) {
    NoteValue *half = NULL;
    NoteValue *shortest = make(NOTE_VALUE_MAX_EXPONENT);
    assert(!standard_rhythm_halved(shortest, &half));
    assert(half == NULL);
    note_value_destroy(shortest);

    NoteValue *dotted = NULL;
    assert(note_value_create_dotted(2, 1, &dotted) == NOTE_VALUE_OK);
    assert(!standard_rhythm_halved(dotted, &half));
    assert(half == NULL);
    note_value_destroy(dotted);

    assert(!standard_rhythm_halved(NULL, &half));
}

/* A whole note is two half notes, four quarter notes, eight eighth notes. */
static void test_count_within(void) {
    NoteValue *whole = make(0);
    unsigned long expected[] = {1, 2, 4, 8, 16};
    for (int exponent = 0; exponent <= 4; exponent++) {
        NoteValue *smaller = make(exponent);
        unsigned long count = 0;
        assert(standard_rhythm_count_within(whole, smaller, &count));
        assert(count == expected[exponent]);
        note_value_destroy(smaller);
    }

    NoteValue *breve = make(-1);
    unsigned long count = 0;
    assert(standard_rhythm_count_within(breve, whole, &count) && count == 2);
    note_value_destroy(breve);

    note_value_destroy(whole);
}

static void test_count_within_declines(void) {
    NoteValue *half = make(1);
    NoteValue *whole = make(0);
    NoteValue *dotted = NULL;
    assert(note_value_create_dotted(2, 1, &dotted) == NOTE_VALUE_OK);
    unsigned long count = 77;

    assert(!standard_rhythm_count_within(half, whole, &count)); /* smaller is larger */
    assert(!standard_rhythm_count_within(half, dotted, &count));
    assert(!standard_rhythm_count_within(dotted, half, &count));
    assert(!standard_rhythm_count_within(NULL, half, &count));
    assert(count == 77);

    note_value_destroy(half);
    note_value_destroy(whole);
    note_value_destroy(dotted);
}

int main(void) {
    test_rules_reference_framework();
    test_halving();
    test_halving_declines();
    test_count_within();
    test_count_within_declines();
    printf("test_hierarchy_rules (standard-rhythm): ok\n");
    return 0;
}
