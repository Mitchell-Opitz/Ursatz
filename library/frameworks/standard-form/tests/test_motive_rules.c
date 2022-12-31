#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "motive_rules.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_form_motive_rule, standard_form_motive_constraint,
                                    STANDARD_FORM_MOTIVE_RULE_COUNT);
}

/* A motive is shorter than its idea and not empty. */
static void test_motive_fits_idea(void) {
    assert(standard_form_motive_fits_idea(3, 8));
    assert(standard_form_motive_fits_idea(1, 2));
    assert(!standard_form_motive_fits_idea(8, 8));
    assert(!standard_form_motive_fits_idea(9, 8));
    assert(!standard_form_motive_fits_idea(0, 8));
}

static void test_default_kind(void) {
    assert(standard_form_motive_default_kind() == MOTIVE_KIND_PITCH);
}

static void test_transformations(void) {
    for (int i = 0; i < STANDARD_FORM_MOTIVE_TRANSFORMATION_COUNT; i++) {
        StandardFormMotiveTransformation t = (StandardFormMotiveTransformation)i;
        assert(standard_form_motive_transformation_valid(t));
        assert(standard_form_motive_transformation_name(t) != NULL);
        assert(standard_form_motive_transformation_type(t) != NULL);
    }
    assert(!standard_form_motive_transformation_valid(STANDARD_FORM_MOTIVE_TRANSFORMATION_COUNT));
    assert(!standard_form_motive_transformation_valid((StandardFormMotiveTransformation)-1));
    assert(standard_form_motive_transformation_name(STANDARD_FORM_MOTIVE_TRANSFORMATION_COUNT) == NULL);
    assert(standard_form_motive_transformation_type(STANDARD_FORM_MOTIVE_TRANSFORMATION_COUNT) == NULL);

    assert(strcmp(standard_form_motive_transformation_name(STANDARD_FORM_MOTIVE_TRANSFORMATION_ENLARGEMENT),
                  "enlargement") == 0);
    assert(strcmp(standard_form_motive_transformation_type(STANDARD_FORM_MOTIVE_TRANSFORMATION_ENLARGEMENT),
                  "augmentation") == 0);
    /* contraction shortens durations, so it is diminution, not transform/contraction.h */
    assert(strcmp(standard_form_motive_transformation_type(STANDARD_FORM_MOTIVE_TRANSFORMATION_CONTRACTION),
                  "diminution") == 0);
    assert(strcmp(standard_form_motive_transformation_type(
                      STANDARD_FORM_MOTIVE_TRANSFORMATION_INTERVALLIC_MANIPULATION),
                  "intervallic_manipulation") == 0);
    assert(strcmp(standard_form_motive_transformation_type(STANDARD_FORM_MOTIVE_TRANSFORMATION_EMBELLISHMENT),
                  "ornamentation") == 0);
}

int main(void) {
    test_rules_reference_framework();
    test_motive_fits_idea();
    test_default_kind();
    test_transformations();
    printf("test_motive_rules (standard-form): ok\n");
    return 0;
}
