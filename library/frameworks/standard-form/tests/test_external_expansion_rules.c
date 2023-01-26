#include <assert.h>
#include <stdio.h>

#include "external_expansion_rules.h"
#include "rule_check.h"

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_form_external_expansion_rule, standard_form_external_expansion_constraint,
                                    STANDARD_FORM_EXTERNAL_EXPANSION_RULE_COUNT);
}

static void test_is_suffix(void) {
    assert(!standard_form_external_kind_is_suffix(EXTERNAL_EXPANSION_KIND_PREFIX));
    assert(standard_form_external_kind_is_suffix(EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION));
    assert(standard_form_external_kind_is_suffix(EXTERNAL_EXPANSION_KIND_CODETTA));
    assert(standard_form_external_kind_is_suffix(EXTERNAL_EXPANSION_KIND_CODA));
    assert(!standard_form_external_kind_is_suffix(EXTERNAL_EXPANSION_KIND_COUNT));
}

/* A phrase at [4, 12): a prefix lies before it, a suffix at or after its end. */
static void test_outside(void) {
    assert(standard_form_external_expansion_outside(4, 12, 0, EXTERNAL_EXPANSION_KIND_PREFIX));
    assert(standard_form_external_expansion_outside(4, 12, 3, EXTERNAL_EXPANSION_KIND_PREFIX));
    assert(!standard_form_external_expansion_outside(4, 12, 4, EXTERNAL_EXPANSION_KIND_PREFIX));
    assert(!standard_form_external_expansion_outside(4, 12, 12, EXTERNAL_EXPANSION_KIND_PREFIX));

    assert(standard_form_external_expansion_outside(4, 12, 12, EXTERNAL_EXPANSION_KIND_CODETTA));
    assert(standard_form_external_expansion_outside(4, 12, 20, EXTERNAL_EXPANSION_KIND_CODA));
    assert(standard_form_external_expansion_outside(4, 12, 12, EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION));
    assert(!standard_form_external_expansion_outside(4, 12, 11, EXTERNAL_EXPANSION_KIND_CODETTA));
    assert(!standard_form_external_expansion_outside(4, 12, 3, EXTERNAL_EXPANSION_KIND_CODA));
    assert(!standard_form_external_expansion_outside(4, 12, 12, EXTERNAL_EXPANSION_KIND_COUNT));
}

/* Location decides the kind of suffix. */
static void test_classify_suffix(void) {
    ExternalExpansionKind kind = EXTERNAL_EXPANSION_KIND_PREFIX;

    assert(standard_form_classify_suffix(STANDARD_FORM_SUFFIX_LOCATION_WITHIN_SECTION, &kind));
    assert(kind == EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION);
    assert(standard_form_classify_suffix(STANDARD_FORM_SUFFIX_LOCATION_AFTER_SECTION, &kind));
    assert(kind == EXTERNAL_EXPANSION_KIND_CODETTA);
    assert(standard_form_classify_suffix(STANDARD_FORM_SUFFIX_LOCATION_AFTER_PIECE, &kind));
    assert(kind == EXTERNAL_EXPANSION_KIND_CODA);

    kind = EXTERNAL_EXPANSION_KIND_PREFIX;
    assert(!standard_form_classify_suffix(STANDARD_FORM_SUFFIX_LOCATION_COUNT, &kind));
    assert(kind == EXTERNAL_EXPANSION_KIND_PREFIX);
}

/* Lengths are guidelines: 1-2, 4-8, 8 or more. */
static void test_suffix_length(void) {
    assert(standard_form_suffix_length_typical(EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION, 1));
    assert(standard_form_suffix_length_typical(EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION, 2));
    assert(!standard_form_suffix_length_typical(EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION, 0));
    assert(!standard_form_suffix_length_typical(EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION, 3));

    assert(standard_form_suffix_length_typical(EXTERNAL_EXPANSION_KIND_CODETTA, 4));
    assert(standard_form_suffix_length_typical(EXTERNAL_EXPANSION_KIND_CODETTA, 8));
    assert(!standard_form_suffix_length_typical(EXTERNAL_EXPANSION_KIND_CODETTA, 3));
    assert(!standard_form_suffix_length_typical(EXTERNAL_EXPANSION_KIND_CODETTA, 9));

    assert(standard_form_suffix_length_typical(EXTERNAL_EXPANSION_KIND_CODA, 8));
    assert(standard_form_suffix_length_typical(EXTERNAL_EXPANSION_KIND_CODA, 30));
    assert(!standard_form_suffix_length_typical(EXTERNAL_EXPANSION_KIND_CODA, 7));

    /* 3 measures is typical of none; 8 is typical of both a codetta and a coda */
    for (int k = 0; k < EXTERNAL_EXPANSION_KIND_COUNT; k++) {
        assert(!standard_form_suffix_length_typical((ExternalExpansionKind)k, 3));
    }
    assert(!standard_form_suffix_length_typical(EXTERNAL_EXPANSION_KIND_PREFIX, 4));
    assert(!standard_form_suffix_length_typical(EXTERNAL_EXPANSION_KIND_COUNT, 4));

    assert(standard_form_suffix_length_agrees_with_location(STANDARD_FORM_SUFFIX_LOCATION_WITHIN_SECTION, 2));
    assert(standard_form_suffix_length_agrees_with_location(STANDARD_FORM_SUFFIX_LOCATION_AFTER_SECTION, 8));
    assert(standard_form_suffix_length_agrees_with_location(STANDARD_FORM_SUFFIX_LOCATION_AFTER_PIECE, 8));
    assert(!standard_form_suffix_length_agrees_with_location(STANDARD_FORM_SUFFIX_LOCATION_AFTER_SECTION, 2));
    assert(!standard_form_suffix_length_agrees_with_location(STANDARD_FORM_SUFFIX_LOCATION_COUNT, 4));
}

static void test_diagram_line(void) {
    StandardFormDiagramLine line = STANDARD_FORM_DIAGRAM_LINE_COUNT;

    assert(standard_form_placement_diagram_line(EXPANSION_PLACEMENT_EXTERNAL, &line));
    assert(line == STANDARD_FORM_DIAGRAM_LINE_DASHED);
    assert(standard_form_placement_diagram_line(EXPANSION_PLACEMENT_INTERNAL, &line));
    assert(line == STANDARD_FORM_DIAGRAM_LINE_SOLID);

    line = STANDARD_FORM_DIAGRAM_LINE_COUNT;
    assert(!standard_form_placement_diagram_line(EXPANSION_PLACEMENT_COUNT, &line));
    assert(line == STANDARD_FORM_DIAGRAM_LINE_COUNT);
}

int main(void) {
    test_rules_reference_framework();
    test_is_suffix();
    test_outside();
    test_classify_suffix();
    test_suffix_length();
    test_diagram_line();
    printf("test_external_expansion_rules (standard-form): ok\n");
    return 0;
}
