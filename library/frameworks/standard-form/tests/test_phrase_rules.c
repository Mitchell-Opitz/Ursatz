#include <assert.h>
#include <stdio.h>

#include "phrase_rules.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_form_phrase_rule, standard_form_phrase_constraint,
                                    STANDARD_FORM_PHRASE_RULE_COUNT);
}

static void test_phrase_complete(void) {
    assert(standard_form_phrase_complete(true, true, true));
    assert(!standard_form_phrase_complete(false, true, true));
    assert(!standard_form_phrase_complete(true, false, true));
    assert(!standard_form_phrase_complete(true, true, false));
    assert(!standard_form_phrase_complete(false, false, false));
}

static void test_cadence_signals_closure(void) {
    assert(standard_form_cadence_signals_closure(true));
    assert(!standard_form_cadence_signals_closure(false));
}

static void test_phrase_length(void) {
    assert(!standard_form_phrase_length_valid(0));
    assert(standard_form_phrase_length_valid(1));
    assert(standard_form_phrase_length_valid(13));
    assert(standard_form_phrase_length_common(4));
    assert(standard_form_phrase_length_common(8));
    assert(standard_form_phrase_length_common(16));
    assert(!standard_form_phrase_length_common(13));
    assert(!standard_form_phrase_length_common(6));
}

static void test_phrase_goal(void) {
    assert(standard_form_cadence_is_phrase_goal(CADENCE_TYPE_PERFECT_AUTHENTIC));
    assert(standard_form_cadence_is_phrase_goal(CADENCE_TYPE_IMPERFECT_AUTHENTIC));
    assert(standard_form_cadence_is_phrase_goal(CADENCE_TYPE_HALF));
    assert(!standard_form_cadence_is_phrase_goal(CADENCE_TYPE_SUBVERTED));
    assert(!standard_form_cadence_is_phrase_goal(CADENCE_TYPE_PLAGAL));
    assert(!standard_form_cadence_is_phrase_goal(CADENCE_TYPE_DECEPTIVE));
    assert(!standard_form_cadence_is_phrase_goal(CADENCE_TYPE_AUTHENTIC_UNSPECIFIED));
    assert(!standard_form_cadence_is_phrase_goal(CADENCE_TYPE_UNKNOWN));
    assert(!standard_form_cadence_is_phrase_goal(CADENCE_TYPE_COUNT));
}

/* Brackets for ideas and subphrases; arcs for the phrase level and above; a motive takes neither. */
static void test_diagram_mark(void) {
    StandardFormDiagramMark mark = STANDARD_FORM_DIAGRAM_MARK_COUNT;
    assert(standard_form_level_diagram_mark(FORM_LEVEL_IDEA, &mark));
    assert(mark == STANDARD_FORM_DIAGRAM_MARK_BRACKET);
    assert(standard_form_level_diagram_mark(FORM_LEVEL_SUBPHRASE, &mark));
    assert(mark == STANDARD_FORM_DIAGRAM_MARK_BRACKET);
    assert(standard_form_level_diagram_mark(FORM_LEVEL_PHRASE, &mark));
    assert(mark == STANDARD_FORM_DIAGRAM_MARK_ARC);
    assert(standard_form_level_diagram_mark(FORM_LEVEL_THEME, &mark));
    assert(mark == STANDARD_FORM_DIAGRAM_MARK_ARC);
    assert(standard_form_level_diagram_mark(FORM_LEVEL_PIECE, &mark));
    assert(mark == STANDARD_FORM_DIAGRAM_MARK_ARC);

    mark = STANDARD_FORM_DIAGRAM_MARK_COUNT;
    assert(!standard_form_level_diagram_mark(FORM_LEVEL_MOTIVE, &mark));
    assert(!standard_form_level_diagram_mark(FORM_LEVEL_COUNT, &mark));
    assert(mark == STANDARD_FORM_DIAGRAM_MARK_COUNT);
}

int main(void) {
    test_rules_reference_framework();
    test_phrase_complete();
    test_cadence_signals_closure();
    test_phrase_length();
    test_phrase_goal();
    test_diagram_mark();
    printf("test_phrase_rules (standard-form): ok\n");
    return 0;
}
