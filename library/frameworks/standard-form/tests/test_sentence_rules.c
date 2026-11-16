#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "sentence_rules.h"

static void test_structure(void) {
    assert(standard_form_subphrase_roles_in_order(SUBPHRASE_ROLE_PRESENTATION, SUBPHRASE_ROLE_CONTINUATION));
    assert(!standard_form_subphrase_roles_in_order(SUBPHRASE_ROLE_CONTINUATION, SUBPHRASE_ROLE_PRESENTATION));
    assert(!standard_form_subphrase_roles_in_order(SUBPHRASE_ROLE_PRESENTATION, SUBPHRASE_ROLE_PRESENTATION));
}

static void test_lengths(void) {
    assert(standard_form_presentation_length_usual(4));
    assert(!standard_form_presentation_length_usual(2));

    assert(standard_form_basic_idea_length_valid(1));
    assert(standard_form_basic_idea_length_valid(2));
    assert(standard_form_basic_idea_length_valid(4));
    assert(!standard_form_basic_idea_length_valid(3));
    assert(!standard_form_basic_idea_length_valid(0));

    /* continuation equal or longer is usual; shorter is uncommon */
    assert(standard_form_continuation_length_usual(4, 4));
    assert(standard_form_continuation_length_usual(4, 6));
    assert(!standard_form_continuation_length_usual(4, 2));
    assert(!standard_form_continuation_length_usual(0, 4));

    assert(standard_form_continuation_balanced(4, 4));
    assert(!standard_form_continuation_balanced(4, 6));
    assert(!standard_form_continuation_balanced(0, 0));
}

/* Two-measure basic ideas fragmented into one-measure units. */
static void test_fragmentation(void) {
    assert(standard_form_is_fragmentation(2, 1));
    assert(!standard_form_is_fragmentation(2, 2));
    assert(!standard_form_is_fragmentation(2, 3));
    assert(!standard_form_is_fragmentation(2, 0));
}

static void test_repetition_contour(void) {
    assert(standard_form_idea_reads_as_repetition(true));
    assert(!standard_form_idea_reads_as_repetition(false));
}

static void test_sentential_and_archetypal(void) {
    assert(standard_form_phrase_is_sentential(true, true));
    assert(!standard_form_phrase_is_sentential(true, false));
    assert(!standard_form_phrase_is_sentential(false, true));

    assert(standard_form_sentence_is_archetypal(8, 4, 4, true));
    assert(standard_form_sentence_is_archetypal(4, 2, 2, true));
    assert(!standard_form_sentence_is_archetypal(8, 4, 4, false));  /* no fragmentation */
    assert(!standard_form_sentence_is_archetypal(8, 4, 2, true));   /* parts do not fill the phrase */
    assert(!standard_form_sentence_is_archetypal(10, 4, 6, true));  /* unbalanced, not 4 or 8 */
    assert(!standard_form_sentence_is_archetypal(12, 6, 6, true));  /* not 4 or 8 */
    assert(!standard_form_sentence_is_archetypal(8, 2, 6, true));   /* unbalanced */
}

int main(void) {
    test_structure();
    test_lengths();
    test_fragmentation();
    test_repetition_contour();
    test_sentential_and_archetypal();
    printf("test_sentence_rules (standard-form): ok\n");
    return 0;
}
