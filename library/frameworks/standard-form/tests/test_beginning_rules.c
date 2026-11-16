#include <assert.h>
#include <stdio.h>

#include "beginning_rules.h"

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
    test_classify();
    printf("test_beginning_rules (standard-form): ok\n");
    return 0;
}
