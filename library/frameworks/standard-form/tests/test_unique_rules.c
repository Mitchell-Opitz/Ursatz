#include <assert.h>
#include <stdio.h>

#include "rule_check.h"
#include "unique_rules.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_form_unique_rule, standard_form_unique_constraint,
                                    STANDARD_FORM_UNIQUE_RULE_COUNT);
}

static void test_unique_form(void) {
    assert(standard_form_organized_as_unique(false, false));
    assert(!standard_form_organized_as_unique(true, false));
    assert(!standard_form_organized_as_unique(false, true));
    assert(!standard_form_organized_as_unique(true, true));
}

static void test_archetypal(void) {
    assert(standard_form_form_is_archetypal(PHRASE_LEVEL_FORM_SENTENCE));
    assert(standard_form_form_is_archetypal(PHRASE_LEVEL_FORM_PERIOD));
    assert(standard_form_form_is_archetypal(PHRASE_LEVEL_FORM_COMPOUND_PERIOD));
    assert(!standard_form_form_is_archetypal(PHRASE_LEVEL_FORM_REPEATED_PHRASE));
    assert(!standard_form_form_is_archetypal(PHRASE_LEVEL_FORM_UNIQUE));
    assert(!standard_form_form_is_archetypal(PHRASE_LEVEL_FORM_COUNT));

    assert(standard_form_form_is_unique(PHRASE_LEVEL_FORM_UNIQUE));
    assert(!standard_form_form_is_unique(PHRASE_LEVEL_FORM_SENTENCE));
    assert(!standard_form_form_is_unique(PHRASE_LEVEL_FORM_REPEATED_PHRASE));
    assert(!standard_form_form_is_unique(PHRASE_LEVEL_FORM_COUNT));
}

/* The two categories are ends of a spectrum: a form can show traits of both. */
static void test_spectrum_and_labels(void) {
    assert(standard_form_form_is_mixed(true, true));
    assert(!standard_form_form_is_mixed(true, false));
    assert(!standard_form_form_is_mixed(false, true));
    assert(!standard_form_form_is_mixed(false, false));

    assert(standard_form_label_should_apply(true));
    assert(!standard_form_label_should_apply(false));
}

int main(void) {
    test_rules_reference_framework();
    test_unique_form();
    test_archetypal();
    test_spectrum_and_labels();
    printf("test_unique_rules (standard-form): ok\n");
    return 0;
}
