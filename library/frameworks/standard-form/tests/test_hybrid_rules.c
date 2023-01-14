#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "hybrid_rules.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_form_hybrid_rule, standard_form_hybrid_constraint,
                                    STANDARD_FORM_HYBRID_RULE_COUNT);
}

static void test_commonality_names(void) {
    for (int i = 0; i < STANDARD_FORM_COMMONALITY_COUNT; i++) {
        assert(standard_form_commonality_valid((StandardFormCommonality)i));
    }
    assert(!standard_form_commonality_valid(STANDARD_FORM_COMMONALITY_COUNT));
    assert(strcmp(standard_form_commonality_name(STANDARD_FORM_COMMONALITY_VERY_COMMON), "very-common") == 0);
    assert(strcmp(standard_form_commonality_name(STANDARD_FORM_COMMONALITY_SLIGHTLY_LESS_COMMON),
                  "slightly-less-common") == 0);
    assert(strcmp(standard_form_commonality_name(STANDARD_FORM_COMMONALITY_OCCASIONAL), "occasional") == 0);
    assert(strcmp(standard_form_commonality_name(STANDARD_FORM_COMMONALITY_RARE), "rare") == 0);
    assert(standard_form_commonality_name(STANDARD_FORM_COMMONALITY_COUNT) == NULL);
}

/* Presentation + continuation is a sentence, antecedent + consequent a period, the rest hybrids. */
static void test_pairing_form(void) {
    PhraseLevelForm form = PHRASE_LEVEL_FORM_UNIQUE;

    assert(standard_form_pairing_form(PHRASE_BEGINNING_PRESENTATION, PHRASE_ENDING_CONTINUATION, &form));
    assert(form == PHRASE_LEVEL_FORM_SENTENCE);
    assert(standard_form_pairing_form(PHRASE_BEGINNING_ANTECEDENT, PHRASE_ENDING_CONSEQUENT, &form));
    assert(form == PHRASE_LEVEL_FORM_PERIOD);

    int hybrids = 0;
    for (int b = 0; b < PHRASE_BEGINNING_COUNT; b++) {
        for (int e = 0; e < PHRASE_ENDING_COUNT; e++) {
            if (standard_form_pairing_is_hybrid((PhraseBeginning)b, (PhraseEnding)e)) {
                assert(standard_form_pairing_form((PhraseBeginning)b, (PhraseEnding)e, &form));
                assert(form == PHRASE_LEVEL_FORM_HYBRID);
                hybrids++;
            }
        }
    }
    assert(hybrids == 7);

    form = PHRASE_LEVEL_FORM_UNIQUE;
    assert(!standard_form_pairing_form(PHRASE_BEGINNING_COUNT, PHRASE_ENDING_CONSEQUENT, &form));
    assert(!standard_form_pairing_form(PHRASE_BEGINNING_ANTECEDENT, PHRASE_ENDING_COUNT, &form));
    assert(form == PHRASE_LEVEL_FORM_UNIQUE);
    assert(!standard_form_pairing_is_hybrid(PHRASE_BEGINNING_ANTECEDENT, PHRASE_ENDING_CONSEQUENT));
    assert(!standard_form_pairing_is_hybrid(PHRASE_BEGINNING_COUNT, PHRASE_ENDING_CONSEQUENT));
}

static void assert_commonality(PhraseBeginning b, PhraseEnding e, StandardFormCommonality expected) {
    StandardFormCommonality c = STANDARD_FORM_COMMONALITY_COUNT;
    assert(standard_form_pairing_commonality(b, e, &c));
    assert(c == expected);
}

/* Example 7. */
static void test_commonality(void) {
    assert_commonality(PHRASE_BEGINNING_PRESENTATION, PHRASE_ENDING_CONTINUATION, STANDARD_FORM_COMMONALITY_VERY_COMMON);
    assert_commonality(PHRASE_BEGINNING_ANTECEDENT, PHRASE_ENDING_CONSEQUENT, STANDARD_FORM_COMMONALITY_VERY_COMMON);

    assert_commonality(PHRASE_BEGINNING_COMPOUND_BASIC_IDEA, PHRASE_ENDING_CONSEQUENT,
                       STANDARD_FORM_COMMONALITY_SLIGHTLY_LESS_COMMON);
    assert_commonality(PHRASE_BEGINNING_COMPOUND_BASIC_IDEA, PHRASE_ENDING_CONTINUATION,
                       STANDARD_FORM_COMMONALITY_SLIGHTLY_LESS_COMMON);
    assert_commonality(PHRASE_BEGINNING_ANTECEDENT, PHRASE_ENDING_CONTINUATION,
                       STANDARD_FORM_COMMONALITY_SLIGHTLY_LESS_COMMON);

    assert_commonality(PHRASE_BEGINNING_COMPOUND_BASIC_IDEA, PHRASE_ENDING_CADENTIAL,
                       STANDARD_FORM_COMMONALITY_OCCASIONAL);
    assert_commonality(PHRASE_BEGINNING_ANTECEDENT, PHRASE_ENDING_CADENTIAL, STANDARD_FORM_COMMONALITY_OCCASIONAL);

    /* rare or non-existent, but still a valid pairing */
    assert_commonality(PHRASE_BEGINNING_PRESENTATION, PHRASE_ENDING_CADENTIAL, STANDARD_FORM_COMMONALITY_RARE);
    assert_commonality(PHRASE_BEGINNING_PRESENTATION, PHRASE_ENDING_CONSEQUENT, STANDARD_FORM_COMMONALITY_RARE);
    assert(standard_form_pairing_is_hybrid(PHRASE_BEGINNING_PRESENTATION, PHRASE_ENDING_CONSEQUENT));

    StandardFormCommonality c = STANDARD_FORM_COMMONALITY_COUNT;
    assert(!standard_form_pairing_commonality(PHRASE_BEGINNING_COUNT, PHRASE_ENDING_CONSEQUENT, &c));
    assert(!standard_form_pairing_commonality(PHRASE_BEGINNING_ANTECEDENT, PHRASE_ENDING_COUNT, &c));
    assert(c == STANDARD_FORM_COMMONALITY_COUNT);
}

static void test_subphrase_role(void) {
    PhraseBeginning beginning = PHRASE_BEGINNING_ANTECEDENT;
    PhraseEnding ending = PHRASE_ENDING_CONSEQUENT;

    assert(standard_form_beginning_of_subphrase_role(SUBPHRASE_ROLE_PRESENTATION, &beginning));
    assert(beginning == PHRASE_BEGINNING_PRESENTATION);
    assert(standard_form_ending_of_subphrase_role(SUBPHRASE_ROLE_CONTINUATION, &ending));
    assert(ending == PHRASE_ENDING_CONTINUATION);

    beginning = PHRASE_BEGINNING_ANTECEDENT;
    ending = PHRASE_ENDING_CONSEQUENT;
    assert(!standard_form_beginning_of_subphrase_role(SUBPHRASE_ROLE_CONTINUATION, &beginning));
    assert(!standard_form_ending_of_subphrase_role(SUBPHRASE_ROLE_PRESENTATION, &ending));
    assert(!standard_form_beginning_of_subphrase_role(SUBPHRASE_ROLE_COUNT, &beginning));
    assert(beginning == PHRASE_BEGINNING_ANTECEDENT && ending == PHRASE_ENDING_CONSEQUENT);
}

int main(void) {
    test_rules_reference_framework();
    test_commonality_names();
    test_pairing_form();
    test_commonality();
    test_subphrase_role();
    printf("test_hybrid_rules (standard-form): ok\n");
    return 0;
}
