#include <assert.h>
#include <stdio.h>

#include "hierarchy_rules.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_form_hierarchy_rule, standard_form_hierarchy_constraint,
                                    STANDARD_FORM_HIERARCHY_RULE_COUNT);
}

static void test_level_contains(void) {
    assert(standard_form_level_contains(FORM_LEVEL_PIECE, FORM_LEVEL_MOTIVE));
    assert(standard_form_level_contains(FORM_LEVEL_PHRASE, FORM_LEVEL_IDEA));
    assert(!standard_form_level_contains(FORM_LEVEL_IDEA, FORM_LEVEL_PHRASE));
    assert(!standard_form_level_contains(FORM_LEVEL_PHRASE, FORM_LEVEL_PHRASE));
    assert(!standard_form_level_contains(FORM_LEVEL_COUNT, FORM_LEVEL_MOTIVE));
    assert(!standard_form_level_contains(FORM_LEVEL_PIECE, FORM_LEVEL_COUNT));
}

static void test_next_smaller(void) {
    FormLevel next = FORM_LEVEL_PIECE;
    assert(standard_form_level_next_smaller(FORM_LEVEL_PIECE, &next));
    assert(next == FORM_LEVEL_MOVEMENT);
    assert(standard_form_level_next_smaller(FORM_LEVEL_PHRASE, &next));
    assert(next == FORM_LEVEL_SUBPHRASE);
    assert(standard_form_level_next_smaller(FORM_LEVEL_IDEA, &next));
    assert(next == FORM_LEVEL_MOTIVE);

    next = FORM_LEVEL_THEME;
    assert(!standard_form_level_next_smaller(FORM_LEVEL_MOTIVE, &next));
    assert(!standard_form_level_next_smaller(FORM_LEVEL_COUNT, &next));
    assert(next == FORM_LEVEL_THEME);
}

/* A single phrase may be a whole section. */
static void test_levels_may_collapse(void) {
    assert(standard_form_levels_may_collapse(FORM_LEVEL_SECTION, FORM_LEVEL_PHRASE));
    assert(standard_form_levels_may_collapse(FORM_LEVEL_PHRASE, FORM_LEVEL_IDEA));
    assert(!standard_form_levels_may_collapse(FORM_LEVEL_PHRASE, FORM_LEVEL_SECTION));
    assert(!standard_form_levels_may_collapse(FORM_LEVEL_PHRASE, FORM_LEVEL_PHRASE));
}

static void test_phrase_level(void) {
    assert(!standard_form_level_is_phrase_level(FORM_LEVEL_PIECE));
    assert(!standard_form_level_is_phrase_level(FORM_LEVEL_THEME));
    assert(standard_form_level_is_phrase_level(FORM_LEVEL_PHRASE));
    assert(standard_form_level_is_phrase_level(FORM_LEVEL_SUBPHRASE));
    assert(standard_form_level_is_phrase_level(FORM_LEVEL_IDEA));
    assert(standard_form_level_is_phrase_level(FORM_LEVEL_MOTIVE));
    assert(!standard_form_level_is_phrase_level(FORM_LEVEL_COUNT));
}

int main(void) {
    test_rules_reference_framework();
    test_level_contains();
    test_next_smaller();
    test_levels_may_collapse();
    test_phrase_level();
    printf("test_hierarchy_rules (standard-form): ok\n");
    return 0;
}
