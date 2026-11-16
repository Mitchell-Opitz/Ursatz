#include "time/note_value.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

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
    note_value_destroy(value);
    assert(note_value_create_dotted(2, NOTE_VALUE_MAX_DOTS, &value) == NOTE_VALUE_OK);
    note_value_destroy(value);
}

int main(void) {
    test_create_rejects_out_of_range();
    test_accessors();
    test_range_bounds();
    printf("test_note_value: ok\n");
    return 0;
}
