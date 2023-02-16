#include <assert.h>
#include <stdio.h>

#include "length_rules.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_form_length_rule, standard_form_length_constraint,
                                    STANDARD_FORM_LENGTH_RULE_COUNT);
}

static void test_classify_length_change(void) {
    PhraseLengthChange change = PHRASE_LENGTH_CHANGE_CONTRACTION;

    /* a four-measure consequent expanded to five (Example 1) */
    assert(standard_form_classify_length_change(4, 5, &change));
    assert(change == PHRASE_LENGTH_CHANGE_EXPANSION);

    /* a four-measure c.b.i. followed by a three-measure continuation (Example 12) */
    assert(standard_form_classify_length_change(4, 3, &change));
    assert(change == PHRASE_LENGTH_CHANGE_CONTRACTION);

    /* the expected length is neither expansion nor contraction */
    change = PHRASE_LENGTH_CHANGE_EXPANSION;
    assert(!standard_form_classify_length_change(4, 4, &change));
    assert(!standard_form_classify_length_change(0, 4, &change));
    assert(!standard_form_classify_length_change(4, 0, &change));
    assert(change == PHRASE_LENGTH_CHANGE_EXPANSION);
}

/* Contraction always happens inside a phrase; expansion may be internal or external. */
static void test_placement(void) {
    assert(standard_form_placement_allowed(PHRASE_LENGTH_CHANGE_EXPANSION, EXPANSION_PLACEMENT_INTERNAL));
    assert(standard_form_placement_allowed(PHRASE_LENGTH_CHANGE_EXPANSION, EXPANSION_PLACEMENT_EXTERNAL));
    assert(standard_form_placement_allowed(PHRASE_LENGTH_CHANGE_CONTRACTION, EXPANSION_PLACEMENT_INTERNAL));
    assert(!standard_form_placement_allowed(PHRASE_LENGTH_CHANGE_CONTRACTION, EXPANSION_PLACEMENT_EXTERNAL));
    assert(!standard_form_placement_allowed(PHRASE_LENGTH_CHANGE_COUNT, EXPANSION_PLACEMENT_INTERNAL));
    assert(!standard_form_placement_allowed(PHRASE_LENGTH_CHANGE_EXPANSION, EXPANSION_PLACEMENT_COUNT));
}

static void test_expected_length_available(void) {
    assert(standard_form_expected_length_available(true, false));
    assert(standard_form_expected_length_available(false, true));
    assert(standard_form_expected_length_available(true, true));
    assert(!standard_form_expected_length_available(false, false));
}

/* An ending is as long as its beginning. */
static void test_balance(void) {
    assert(standard_form_expected_ending_length(4) == 4);
    assert(standard_form_expected_ending_length(0) == 0);

    PhraseLengthChange change = PHRASE_LENGTH_CHANGE_CONTRACTION;
    assert(standard_form_ending_length_change(4, 5, &change));
    assert(change == PHRASE_LENGTH_CHANGE_EXPANSION);
    assert(standard_form_ending_length_change(4, 3, &change));
    assert(change == PHRASE_LENGTH_CHANGE_CONTRACTION);
    assert(!standard_form_ending_length_change(4, 4, &change));
    assert(!standard_form_ending_length_change(0, 4, &change));
}

int main(void) {
    test_rules_reference_framework();
    test_classify_length_change();
    test_placement();
    test_expected_length_available();
    test_balance();
    printf("test_length_rules (standard-form): ok\n");
    return 0;
}
