#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "segmentation_rules.h"

static void test_idea_length(void) {
    assert(standard_form_idea_length_valid(1));
    assert(standard_form_idea_length_valid(2));
    assert(standard_form_idea_length_valid(4));
    assert(!standard_form_idea_length_valid(0));
}

static void test_ideas_fill_phrase(void) {
    size_t even[4] = {2, 2, 2, 2};
    size_t uneven[3] = {2, 1, 5};
    size_t short_fill[2] = {2, 2};
    size_t with_empty[3] = {4, 0, 4};

    assert(standard_form_ideas_fill_phrase(even, 4, 8));
    assert(standard_form_ideas_fill_phrase(uneven, 3, 8));
    assert(!standard_form_ideas_fill_phrase(short_fill, 2, 8));
    assert(!standard_form_ideas_fill_phrase(even, 4, 7));
    assert(!standard_form_ideas_fill_phrase(with_empty, 3, 8));
    assert(!standard_form_ideas_fill_phrase(even, 0, 0));
    assert(!standard_form_ideas_fill_phrase(NULL, 4, 8));
}

int main(void) {
    test_idea_length();
    test_ideas_fill_phrase();
    printf("test_segmentation_rules (standard-form): ok\n");
    return 0;
}
