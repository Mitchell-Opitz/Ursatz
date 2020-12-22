#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"
#include "minor_scale_degree_rules.h"
#include "modal_solfege_rules.h"
#include "scale_degree_rules.h"

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_MODAL_SOLFEGE_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_modal_solfege_rule((StandardTonalityModalSolfegeRule)i);
        const Constraint *constraint = standard_tonality_modal_solfege_constraint((StandardTonalityModalSolfegeRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_modal_solfege_rule(STANDARD_TONALITY_MODAL_SOLFEGE_RULE_COUNT) == NULL);
}

/* Example 9: every mode, brightest to darkest, with solfege. */
static void test_syllables(void) {
    const char *expected[STANDARD_TONALITY_MODE_COUNT][8] = {
        {"do", "re", "mi", "fi", "sol", "la", "ti", "do"},
        {"do", "re", "mi", "fa", "sol", "la", "ti", "do"},
        {"do", "re", "mi", "fa", "sol", "la", "te", "do"},
        {"do", "re", "me", "fa", "sol", "la", "te", "do"},
        {"do", "re", "me", "fa", "sol", "le", "te", "do"},
        {"do", "ra", "me", "fa", "sol", "le", "te", "do"},
        {"do", "ra", "me", "fa", "se", "le", "te", "do"},
    };
    for (int mode = 0; mode < STANDARD_TONALITY_MODE_COUNT; mode++) {
        for (int degree = 1; degree <= 8; degree++) {
            const char *syllable = standard_tonality_mode_solfege((StandardTonalityMode)mode, degree);
            assert(syllable != NULL);
            assert(strcmp(syllable, expected[mode][degree - 1]) == 0);
        }
    }
}

static void test_degree_alterations_against_major(void) {
    /* Lydian raises 4; mixolydian lowers 7; locrian lowers 2 3 5 6 7. */
    assert(standard_tonality_mode_degree_alteration(STANDARD_TONALITY_MODE_LYDIAN, 4) == 1);
    assert(standard_tonality_mode_degree_alteration(STANDARD_TONALITY_MODE_LYDIAN, 5) == 0);
    assert(standard_tonality_mode_degree_alteration(STANDARD_TONALITY_MODE_MIXOLYDIAN, 7) == -1);
    int lowered[] = {2, 3, 5, 6, 7};
    for (size_t i = 0; i < 5; i++) {
        assert(standard_tonality_mode_degree_alteration(STANDARD_TONALITY_MODE_LOCRIAN, lowered[i]) == -1);
    }
    assert(standard_tonality_mode_degree_alteration(STANDARD_TONALITY_MODE_LOCRIAN, 1) == 0);
    assert(standard_tonality_mode_degree_alteration(STANDARD_TONALITY_MODE_LOCRIAN, 4) == 0);

    for (int degree = 1; degree <= 7; degree++) {
        assert(standard_tonality_mode_degree_alteration(STANDARD_TONALITY_MODE_IONIAN, degree) == 0);
    }
    assert(standard_tonality_mode_degree_alteration((StandardTonalityMode)99, 3) == 0);
    assert(standard_tonality_mode_degree_alteration(STANDARD_TONALITY_MODE_DORIAN, 0) == 0);
    assert(standard_tonality_mode_degree_alteration(STANDARD_TONALITY_MODE_DORIAN, 8) == 0);
}

/* The bright modes have mi, the dark modes me. */
static void test_mi_or_me(void) {
    for (int mode = 0; mode < STANDARD_TONALITY_MODE_COUNT; mode++) {
        StandardTonalityMode m = (StandardTonalityMode)mode;
        const char *third = standard_tonality_mode_solfege(m, 3);
        assert(strcmp(third, standard_tonality_mode_is_bright(m) ? "mi" : "me") == 0);
    }
}

/* Ionian and aeolian are the major and natural minor scales, and carry
   their solfege. */
static void test_agrees_with_major_and_natural_minor_solfege(void) {
    for (int degree = 1; degree <= 8; degree++) {
        assert(strcmp(standard_tonality_mode_solfege(STANDARD_TONALITY_MODE_IONIAN, degree),
                      standard_tonality_solfege(degree)) == 0);
        assert(strcmp(standard_tonality_mode_solfege(STANDARD_TONALITY_MODE_AEOLIAN, degree),
                      standard_tonality_minor_solfege(STANDARD_TONALITY_MINOR_NATURAL, degree)) == 0);
    }
}

static void test_invalid_arguments(void) {
    assert(standard_tonality_mode_solfege((StandardTonalityMode)99, 1) == NULL);
    assert(standard_tonality_mode_solfege(STANDARD_TONALITY_MODE_DORIAN, 0) == NULL);
    assert(standard_tonality_mode_solfege(STANDARD_TONALITY_MODE_DORIAN, 9) == NULL);
}

int main(void) {
    test_rules_reference_framework();
    test_syllables();
    test_degree_alterations_against_major();
    test_mi_or_me();
    test_agrees_with_major_and_natural_minor_solfege();
    test_invalid_arguments();
    printf("test_modal_solfege_rules (standard-tonality): ok\n");
    return 0;
}
