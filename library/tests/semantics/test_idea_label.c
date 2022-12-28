#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "semantics/idea_label.h"

static void test_validity(void) {
    for (int i = 0; i < IDEA_LABEL_COUNT; i++) {
        assert(idea_label_valid((IdeaLabel)i));
    }
    assert(!idea_label_valid(IDEA_LABEL_COUNT));
    assert(!idea_label_valid((IdeaLabel)-1));
}

static void test_names(void) {
    assert(strcmp(idea_label_name(IDEA_LABEL_BASIC_IDEA), "basic-idea") == 0);
    assert(strcmp(idea_label_name(IDEA_LABEL_CONTRASTING_IDEA), "contrasting-idea") == 0);
    assert(strcmp(idea_label_name(IDEA_LABEL_FRAGMENT), "fragment") == 0);
    assert(strcmp(idea_label_name(IDEA_LABEL_UNIT), "unit") == 0);
    assert(idea_label_name(IDEA_LABEL_COUNT) == NULL);
}

static void test_abbreviations(void) {
    assert(strcmp(idea_label_abbreviation(IDEA_LABEL_BASIC_IDEA), "b.i.") == 0);
    assert(strcmp(idea_label_abbreviation(IDEA_LABEL_CONTRASTING_IDEA), "c.i.") == 0);
    assert(strcmp(idea_label_abbreviation(IDEA_LABEL_FRAGMENT), "f.") == 0);
    assert(strcmp(idea_label_abbreviation(IDEA_LABEL_UNIT), "u.") == 0);
    assert(idea_label_abbreviation(IDEA_LABEL_COUNT) == NULL);
}

int main(void) {
    test_validity();
    test_names();
    test_abbreviations();
    printf("test_idea_label: ok\n");
    return 0;
}
