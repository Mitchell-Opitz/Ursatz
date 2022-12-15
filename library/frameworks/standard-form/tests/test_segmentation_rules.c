#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rule_check.h"
#include "segmentation_rules.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_form_segmentation_rule, standard_form_segmentation_constraint,
                                    STANDARD_FORM_SEGMENTATION_RULE_COUNT);
}

static void test_steps(void) {
    assert(strcmp(standard_form_segmentation_step_name(STANDARD_FORM_SEGMENTATION_STEP_IDENTIFY_CLOSURE),
                  "identify-closure") == 0);
    assert(strcmp(standard_form_segmentation_step_name(STANDARD_FORM_SEGMENTATION_STEP_DIVIDE_INTO_IDEAS),
                  "divide-into-ideas") == 0);
    assert(strcmp(standard_form_segmentation_step_name(STANDARD_FORM_SEGMENTATION_STEP_BRACKET_IDEAS),
                  "bracket-ideas") == 0);
    assert(strcmp(standard_form_segmentation_step_name(STANDARD_FORM_SEGMENTATION_STEP_VERIFY_CADENCES),
                  "verify-cadences") == 0);
    assert(standard_form_segmentation_step_name(STANDARD_FORM_SEGMENTATION_STEP_COUNT) == NULL);
    assert(!standard_form_segmentation_step_valid(STANDARD_FORM_SEGMENTATION_STEP_COUNT));
    assert(!standard_form_segmentation_step_valid((StandardFormSegmentationStep)-1));
}

static void test_step_order(void) {
    StandardFormSegmentationStep step = STANDARD_FORM_SEGMENTATION_STEP_IDENTIFY_CLOSURE;
    assert(standard_form_segmentation_step_next(step, &step));
    assert(step == STANDARD_FORM_SEGMENTATION_STEP_DIVIDE_INTO_IDEAS);
    assert(standard_form_segmentation_step_next(step, &step));
    assert(step == STANDARD_FORM_SEGMENTATION_STEP_BRACKET_IDEAS);
    assert(standard_form_segmentation_step_next(step, &step));
    assert(step == STANDARD_FORM_SEGMENTATION_STEP_VERIFY_CADENCES);
    assert(!standard_form_segmentation_step_next(step, &step));
    assert(step == STANDARD_FORM_SEGMENTATION_STEP_VERIFY_CADENCES);
    assert(!standard_form_segmentation_step_next(STANDARD_FORM_SEGMENTATION_STEP_COUNT, &step));
}

static void test_closure_points(void) {
    bool cadence_after[6] = {false, false, true, false, false, true};
    size_t points[4];
    assert(standard_form_closure_points(cadence_after, 6, points, 4) == 2);
    assert(points[0] == 2);
    assert(points[1] == 5);

    /* more points than capacity: all are counted, only the capacity is written */
    size_t one[1];
    assert(standard_form_closure_points(cadence_after, 6, one, 1) == 2);
    assert(one[0] == 2);

    bool none[3] = {false, false, false};
    assert(standard_form_closure_points(none, 3, points, 4) == 0);
    assert(standard_form_closure_points(NULL, 3, points, 4) == 0);
    assert(standard_form_closure_points(cadence_after, 6, NULL, 0) == 2);
}

static void test_idea_length(void) {
    assert(standard_form_idea_typical_measures() == 2);
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
    test_rules_reference_framework();
    test_steps();
    test_step_order();
    test_closure_points();
    test_idea_length();
    test_ideas_fill_phrase();
    printf("test_segmentation_rules (standard-form): ok\n");
    return 0;
}
