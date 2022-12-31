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

int main(void) {
    test_rules_reference_framework();
    test_phrase_complete();
    test_cadence_signals_closure();
    printf("test_phrase_rules (standard-form): ok\n");
    return 0;
}
