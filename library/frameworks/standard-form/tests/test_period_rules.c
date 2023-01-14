#include <assert.h>
#include <stdio.h>

#include "period_rules.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_form_period_rule, standard_form_period_constraint,
                                    STANDARD_FORM_PERIOD_RULE_COUNT);
}

static void test_structure(void) {
    assert(standard_form_phrase_roles_in_order(PHRASE_ROLE_ANTECEDENT, PHRASE_ROLE_CONSEQUENT));
    assert(!standard_form_phrase_roles_in_order(PHRASE_ROLE_CONSEQUENT, PHRASE_ROLE_ANTECEDENT));
    assert(!standard_form_phrase_roles_in_order(PHRASE_ROLE_ANTECEDENT, PHRASE_ROLE_ANTECEDENT));

    assert(standard_form_period_phrase_ideas_valid(IDEA_LABEL_BASIC_IDEA, IDEA_LABEL_CONTRASTING_IDEA));
    assert(!standard_form_period_phrase_ideas_valid(IDEA_LABEL_CONTRASTING_IDEA, IDEA_LABEL_BASIC_IDEA));
    assert(!standard_form_period_phrase_ideas_valid(IDEA_LABEL_BASIC_IDEA, IDEA_LABEL_FRAGMENT));
}

/* The consequent must end stronger than the antecedent. */
static void test_cadences(void) {
    assert(standard_form_period_cadences_valid(CADENCE_TYPE_HALF, CADENCE_TYPE_PERFECT_AUTHENTIC));
    assert(standard_form_period_cadences_valid(CADENCE_TYPE_HALF, CADENCE_TYPE_IMPERFECT_AUTHENTIC));
    assert(standard_form_period_cadences_valid(CADENCE_TYPE_IMPERFECT_AUTHENTIC, CADENCE_TYPE_PERFECT_AUTHENTIC));
    assert(!standard_form_period_cadences_valid(CADENCE_TYPE_PERFECT_AUTHENTIC, CADENCE_TYPE_HALF));
    assert(!standard_form_period_cadences_valid(CADENCE_TYPE_HALF, CADENCE_TYPE_HALF));
    assert(!standard_form_period_cadences_valid(CADENCE_TYPE_SUBVERTED, CADENCE_TYPE_PERFECT_AUTHENTIC));
    assert(!standard_form_period_cadences_valid(CADENCE_TYPE_HALF, CADENCE_TYPE_SUBVERTED));

    assert(standard_form_antecedent_cadence_usual(CADENCE_TYPE_HALF));
    assert(!standard_form_antecedent_cadence_usual(CADENCE_TYPE_IMPERFECT_AUTHENTIC));
    assert(standard_form_consequent_cadence_usual(CADENCE_TYPE_PERFECT_AUTHENTIC));
    assert(!standard_form_consequent_cadence_usual(CADENCE_TYPE_IMPERFECT_AUTHENTIC));
}

static void test_lengths(void) {
    assert(standard_form_antecedent_length_usual(4));
    assert(!standard_form_antecedent_length_usual(8));

    assert(standard_form_consequent_length_usual(4, 4));
    assert(standard_form_consequent_length_usual(4, 8));
    assert(!standard_form_consequent_length_usual(4, 2));
    assert(!standard_form_consequent_length_usual(0, 4));
}

static void test_modulation(void) {
    assert(standard_form_phrase_role_usually_modulates(PHRASE_ROLE_CONSEQUENT));
    assert(!standard_form_phrase_role_usually_modulates(PHRASE_ROLE_ANTECEDENT));
    assert(!standard_form_phrase_role_usually_modulates(PHRASE_ROLE_COUNT));
}

static void test_consequent_ideas(void) {
    assert(standard_form_consequent_contrast_usual(true));
    assert(!standard_form_consequent_contrast_usual(false));
    assert(standard_form_basic_ideas_need_numbering(true));
    assert(!standard_form_basic_ideas_need_numbering(false));
}

int main(void) {
    test_rules_reference_framework();
    test_structure();
    test_cadences();
    test_lengths();
    test_modulation();
    test_consequent_ideas();
    printf("test_period_rules (standard-form): ok\n");
    return 0;
}
