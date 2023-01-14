#include <assert.h>
#include <stdio.h>

#include "beginning_rules.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_form_beginning_rule, standard_form_beginning_constraint,
                                    STANDARD_FORM_BEGINNING_RULE_COUNT);
}

static void test_cadence(void) {
    assert(standard_form_beginning_ends_with_cadence(PHRASE_BEGINNING_ANTECEDENT));
    assert(!standard_form_beginning_ends_with_cadence(PHRASE_BEGINNING_COMPOUND_BASIC_IDEA));
    assert(!standard_form_beginning_ends_with_cadence(PHRASE_BEGINNING_PRESENTATION));
    assert(!standard_form_beginning_ends_with_cadence(PHRASE_BEGINNING_COUNT));
}

static void test_ideas(void) {
    IdeaLabel first = IDEA_LABEL_UNIT;
    IdeaLabel second = IDEA_LABEL_UNIT;

    assert(standard_form_beginning_ideas(PHRASE_BEGINNING_ANTECEDENT, &first, &second));
    assert(first == IDEA_LABEL_BASIC_IDEA && second == IDEA_LABEL_CONTRASTING_IDEA);
    assert(standard_form_beginning_ideas(PHRASE_BEGINNING_COMPOUND_BASIC_IDEA, &first, &second));
    assert(first == IDEA_LABEL_BASIC_IDEA && second == IDEA_LABEL_CONTRASTING_IDEA);
    assert(standard_form_beginning_ideas(PHRASE_BEGINNING_PRESENTATION, &first, &second));
    assert(first == IDEA_LABEL_BASIC_IDEA && second == IDEA_LABEL_BASIC_IDEA);

    first = second = IDEA_LABEL_UNIT;
    assert(!standard_form_beginning_ideas(PHRASE_BEGINNING_COUNT, &first, &second));
    assert(first == IDEA_LABEL_UNIT && second == IDEA_LABEL_UNIT);
}

static void test_compound_basic_idea(void) {
    assert(standard_form_beginning_is_cadence_free_antecedent(PHRASE_BEGINNING_COMPOUND_BASIC_IDEA));
    assert(!standard_form_beginning_is_cadence_free_antecedent(PHRASE_BEGINNING_ANTECEDENT));
    assert(!standard_form_beginning_is_cadence_free_antecedent(PHRASE_BEGINNING_PRESENTATION));
}

/* Example 3: the three beginnings differ by cadence and second idea. */
static void test_classify(void) {
    PhraseBeginning beginning = PHRASE_BEGINNING_PRESENTATION;

    assert(standard_form_classify_beginning(true, IDEA_LABEL_BASIC_IDEA, IDEA_LABEL_CONTRASTING_IDEA, &beginning));
    assert(beginning == PHRASE_BEGINNING_ANTECEDENT);
    assert(standard_form_classify_beginning(false, IDEA_LABEL_BASIC_IDEA, IDEA_LABEL_CONTRASTING_IDEA, &beginning));
    assert(beginning == PHRASE_BEGINNING_COMPOUND_BASIC_IDEA);
    assert(standard_form_classify_beginning(false, IDEA_LABEL_BASIC_IDEA, IDEA_LABEL_BASIC_IDEA, &beginning));
    assert(beginning == PHRASE_BEGINNING_PRESENTATION);

    /* combinations no beginning has leave the output alone */
    beginning = PHRASE_BEGINNING_ANTECEDENT;
    assert(!standard_form_classify_beginning(true, IDEA_LABEL_BASIC_IDEA, IDEA_LABEL_BASIC_IDEA, &beginning));
    assert(!standard_form_classify_beginning(false, IDEA_LABEL_CONTRASTING_IDEA, IDEA_LABEL_BASIC_IDEA, &beginning));
    assert(!standard_form_classify_beginning(false, IDEA_LABEL_BASIC_IDEA, IDEA_LABEL_FRAGMENT, &beginning));
    assert(beginning == PHRASE_BEGINNING_ANTECEDENT);
}

int main(void) {
    test_rules_reference_framework();
    test_cadence();
    test_ideas();
    test_compound_basic_idea();
    test_classify();
    printf("test_beginning_rules (standard-form): ok\n");
    return 0;
}
