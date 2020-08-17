#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rest_rules.h"
#include "rule_check.h"

static NoteValue *make(int exponent) {
    NoteValue *value = NULL;
    assert(note_value_create(exponent, &value) == NOTE_VALUE_OK);
    return value;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_rhythm_rest_rule, standard_rhythm_rest_constraint,
                                    STANDARD_RHYTHM_REST_RULE_COUNT);
}

/* Every note value that has a name has a rest name. */
static void test_names(void) {
    const char *american[] = {"breve rest",        "whole rest",         "half rest",
                              "quarter rest",      "eighth rest",        "sixteenth rest",
                              "thirty-second rest", "sixty-fourth rest"};
    const char *british[] = {"breve rest",   "semibreve rest",      "minim rest",
                             "crotchet rest", "quaver rest",        "semiquaver rest",
                             "demisemiquaver rest", "hemidemisemiquaver rest"};
    for (int exponent = -1; exponent <= STANDARD_RHYTHM_MAX_NAMED_EXPONENT; exponent++) {
        NoteValue *value = make(exponent);
        assert(standard_rhythm_note_name(value, STANDARD_RHYTHM_NAMING_AMERICAN) != NULL);
        assert(strcmp(standard_rhythm_rest_name(value, STANDARD_RHYTHM_NAMING_AMERICAN), american[exponent + 1]) == 0);
        assert(strcmp(standard_rhythm_rest_name(value, STANDARD_RHYTHM_NAMING_BRITISH), british[exponent + 1]) == 0);
        note_value_destroy(value);
    }

    NoteValue *unnamed = make(STANDARD_RHYTHM_MAX_NAMED_EXPONENT + 1);
    assert(standard_rhythm_rest_name(unnamed, STANDARD_RHYTHM_NAMING_AMERICAN) == NULL);
    note_value_destroy(unnamed);
    assert(standard_rhythm_rest_name(NULL, STANDARD_RHYTHM_NAMING_AMERICAN) == NULL);
}

/* Flags halve a rest: none through the quarter rest, then one per step. */
static void test_flags(void) {
    for (int exponent = NOTE_VALUE_MIN_EXPONENT; exponent <= NOTE_VALUE_MAX_EXPONENT; exponent++) {
        NoteValue *value = make(exponent);
        assert(standard_rhythm_rest_flag_count(value) == (exponent > 2 ? exponent - 2 : 0));
        note_value_destroy(value);
    }
}

/* A breve rest equals two whole rests; a whole rest, four quarter rests. */
static void test_hierarchy(void) {
    NoteValue *breve = make(-1);
    NoteValue *whole = make(0);
    NoteValue *quarter = make(2);
    unsigned long count = 0;

    assert(standard_rhythm_rest_count_within(breve, whole, &count) && count == 2);
    assert(standard_rhythm_rest_count_within(whole, quarter, &count) && count == 4);

    count = 55;
    assert(!standard_rhythm_rest_count_within(quarter, whole, &count));
    assert(count == 55);

    note_value_destroy(breve);
    note_value_destroy(whole);
    note_value_destroy(quarter);
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_flags();
    test_hierarchy();
    printf("test_rest_rules (standard-rhythm): ok\n");
    return 0;
}
