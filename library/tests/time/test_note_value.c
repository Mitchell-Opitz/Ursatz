#include "time/note_value.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

static void assert_duration(const NoteValue *value, long numerator, unsigned long denominator) {
    Duration *duration = NULL;
    assert(note_value_duration(value, &duration) == DURATION_OK);
    assert(duration_numerator(duration) == numerator);
    assert(duration_denominator(duration) == denominator);
    duration_destroy(duration);
}

static void test_create_rejects_out_of_range(void) {
    NoteValue *value = NULL;
    assert(note_value_create(NOTE_VALUE_MIN_EXPONENT - 1, &value) == NOTE_VALUE_ERR_EXPONENT_OUT_OF_RANGE);
    assert(note_value_create(NOTE_VALUE_MAX_EXPONENT + 1, &value) == NOTE_VALUE_ERR_EXPONENT_OUT_OF_RANGE);
    assert(note_value_create_dotted(2, NOTE_VALUE_MAX_DOTS + 1, &value) == NOTE_VALUE_ERR_TOO_MANY_DOTS);
    assert(value == NULL);
}

static void test_accessors(void) {
    NoteValue *value = NULL;
    assert(note_value_create(2, &value) == NOTE_VALUE_OK);
    assert(note_value_exponent(value) == 2);
    assert(note_value_dots(value) == 0);
    note_value_destroy(value);

    assert(note_value_create_dotted(3, 2, &value) == NOTE_VALUE_OK);
    assert(note_value_exponent(value) == 3);
    assert(note_value_dots(value) == 2);
    note_value_destroy(value);
}

/* Breve through 1024th: every bound is accepted. */
static void test_range_bounds(void) {
    NoteValue *value = NULL;
    assert(note_value_create(NOTE_VALUE_MIN_EXPONENT, &value) == NOTE_VALUE_OK);
    note_value_destroy(value);
    assert(note_value_create(NOTE_VALUE_MAX_EXPONENT, &value) == NOTE_VALUE_OK);
    assert_duration(value, 1, 1024);
    note_value_destroy(value);
    assert(note_value_create_dotted(2, NOTE_VALUE_MAX_DOTS, &value) == NOTE_VALUE_OK);
    note_value_destroy(value);
}

/* Each step halves: breve 2, whole 1, half 1/2, quarter 1/4, ... */
static void test_undotted_durations(void) {
    long numerators[] = {2, 1, 1, 1, 1, 1};
    unsigned long denominators[] = {1, 1, 2, 4, 8, 16};
    for (int exponent = -1; exponent <= 4; exponent++) {
        NoteValue *value = NULL;
        assert(note_value_create(exponent, &value) == NOTE_VALUE_OK);
        assert_duration(value, numerators[exponent + 1], denominators[exponent + 1]);
        note_value_destroy(value);
    }
}

/* A dot adds half; a quarter is two eighths, a dotted quarter three. */
static void test_dotted_durations(void) {
    NoteValue *value = NULL;

    assert(note_value_create_dotted(2, 1, &value) == NOTE_VALUE_OK); /* dotted quarter */
    assert_duration(value, 3, 8);
    note_value_destroy(value);

    assert(note_value_create_dotted(0, 1, &value) == NOTE_VALUE_OK); /* dotted whole = three halves */
    assert_duration(value, 3, 2);
    note_value_destroy(value);

    assert(note_value_create_dotted(1, 1, &value) == NOTE_VALUE_OK); /* dotted half = three quarters */
    assert_duration(value, 3, 4);
    note_value_destroy(value);

    assert(note_value_create_dotted(2, 2, &value) == NOTE_VALUE_OK); /* quarter + eighth + sixteenth */
    assert_duration(value, 7, 16);
    note_value_destroy(value);

    assert(note_value_create_dotted(2, 3, &value) == NOTE_VALUE_OK);
    assert_duration(value, 15, 32);
    note_value_destroy(value);

    assert(note_value_create_dotted(-1, 1, &value) == NOTE_VALUE_OK); /* dotted breve = 3 wholes */
    assert_duration(value, 3, 1);
    note_value_destroy(value);

    assert(note_value_create_dotted(-1, 2, &value) == NOTE_VALUE_OK);
    assert_duration(value, 7, 2);
    note_value_destroy(value);
}

/* A double-dotted note is 7/4 of its base. */
static void test_double_dot_is_seven_quarters_of_base(void) {
    NoteValue *base = NULL;
    NoteValue *dotted = NULL;
    assert(note_value_create(2, &base) == NOTE_VALUE_OK);
    assert(note_value_create_dotted(2, 2, &dotted) == NOTE_VALUE_OK);

    Duration *base_duration = NULL;
    Duration *dotted_duration = NULL;
    assert(note_value_duration(base, &base_duration) == DURATION_OK);
    assert(note_value_duration(dotted, &dotted_duration) == DURATION_OK);
    /* dotted / base = 7/4  =>  dotted * 4 = base * 7 */
    assert(duration_numerator(dotted_duration) * (long)duration_denominator(base_duration) * 4 ==
           duration_numerator(base_duration) * 7 * (long)duration_denominator(dotted_duration));

    duration_destroy(base_duration);
    duration_destroy(dotted_duration);
    note_value_destroy(base);
    note_value_destroy(dotted);
}

static void test_equality(void) {
    NoteValue *quarter = NULL;
    NoteValue *other_quarter = NULL;
    NoteValue *dotted_quarter = NULL;
    NoteValue *eighth = NULL;
    assert(note_value_create(2, &quarter) == NOTE_VALUE_OK);
    assert(note_value_create(2, &other_quarter) == NOTE_VALUE_OK);
    assert(note_value_create_dotted(2, 1, &dotted_quarter) == NOTE_VALUE_OK);
    assert(note_value_create(3, &eighth) == NOTE_VALUE_OK);

    assert(note_value_equals(quarter, other_quarter));
    assert(!note_value_equals(quarter, dotted_quarter));
    assert(!note_value_equals(quarter, eighth));
    assert(!note_value_equals(quarter, NULL));
    assert(!note_value_equals(NULL, quarter));

    note_value_destroy(quarter);
    note_value_destroy(other_quarter);
    note_value_destroy(dotted_quarter);
    note_value_destroy(eighth);
}

int main(void) {
    test_create_rejects_out_of_range();
    test_accessors();
    test_range_bounds();
    test_undotted_durations();
    test_dotted_durations();
    test_double_dot_is_seven_quarters_of_base();
    test_equality();
    printf("test_note_value: ok\n");
    return 0;
}
