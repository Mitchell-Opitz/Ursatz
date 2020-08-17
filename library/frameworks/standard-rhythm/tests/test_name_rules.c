#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "name_rules.h"
#include "rule_check.h"

static NoteValue *make(int exponent) {
    NoteValue *value = NULL;
    assert(note_value_create(exponent, &value) == NOTE_VALUE_OK);
    return value;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_rhythm_name_rule, standard_rhythm_name_constraint,
                                    STANDARD_RHYTHM_NAME_RULE_COUNT);
}

static void test_names(void) {
    const char *american[] = {"double whole note", "whole note",         "half note",
                              "quarter note",      "eighth note",        "sixteenth note",
                              "thirty-second note", "sixty-fourth note"};
    const char *british[] = {"breve", "semibreve",   "minim",          "crotchet",
                             "quaver", "semiquaver", "demisemiquaver", "hemidemisemiquaver"};
    for (int exponent = -1; exponent <= STANDARD_RHYTHM_MAX_NAMED_EXPONENT; exponent++) {
        NoteValue *value = make(exponent);
        assert(strcmp(standard_rhythm_note_name(value, STANDARD_RHYTHM_NAMING_AMERICAN), american[exponent + 1]) == 0);
        assert(strcmp(standard_rhythm_note_name(value, STANDARD_RHYTHM_NAMING_BRITISH), british[exponent + 1]) == 0);
        note_value_destroy(value);
    }
}

static void test_unnamed_past_sixty_fourth(void) {
    NoteValue *value = make(STANDARD_RHYTHM_MAX_NAMED_EXPONENT + 1);
    assert(standard_rhythm_note_name(value, STANDARD_RHYTHM_NAMING_AMERICAN) == NULL);
    assert(standard_rhythm_note_name(value, STANDARD_RHYTHM_NAMING_BRITISH) == NULL);
    note_value_destroy(value);
    assert(standard_rhythm_note_name(NULL, STANDARD_RHYTHM_NAMING_AMERICAN) == NULL);
}

/* A dot is not part of the name. */
static void test_dotted_value_has_base_name(void) {
    NoteValue *dotted = NULL;
    assert(note_value_create_dotted(2, 2, &dotted) == NOTE_VALUE_OK);
    assert(strcmp(standard_rhythm_note_name(dotted, STANDARD_RHYTHM_NAMING_AMERICAN), "quarter note") == 0);
    note_value_destroy(dotted);
}

static void test_name_lookup(void) {
    int exponent = 99;
    assert(standard_rhythm_note_exponent_from_name("whole note", &exponent) && exponent == 0);
    assert(standard_rhythm_note_exponent_from_name("Semibreve", &exponent) && exponent == 0);
    assert(standard_rhythm_note_exponent_from_name("CROTCHET", &exponent) && exponent == 2);
    assert(standard_rhythm_note_exponent_from_name("quarter note", &exponent) && exponent == 2);
    assert(standard_rhythm_note_exponent_from_name("breve", &exponent) && exponent == -1);
    assert(standard_rhythm_note_exponent_from_name("double whole note", &exponent) && exponent == -1);
    assert(standard_rhythm_note_exponent_from_name("hemidemisemiquaver", &exponent) && exponent == 6);

    exponent = 99;
    assert(!standard_rhythm_note_exponent_from_name("quarter", &exponent));
    assert(!standard_rhythm_note_exponent_from_name("", &exponent));
    assert(!standard_rhythm_note_exponent_from_name(NULL, &exponent));
    assert(exponent == 99);
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_unnamed_past_sixty_fourth();
    test_dotted_value_has_base_name();
    test_name_lookup();
    printf("test_name_rules (standard-rhythm): ok\n");
    return 0;
}
