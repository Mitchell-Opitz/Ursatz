#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"
#include "identity/entity_id.h"
#include "interpretation/scale_degree.h"
#include "major_scale_rules.h"
#include "minor_scale_degree_rules.h"
#include "pitch/pitch_identity.h"
#include "scale_degree_rules.h"
#include "spelling_test_util.h"

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_MINOR_DEGREE_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_minor_degree_rule((StandardTonalityMinorDegreeRule)i);
        const Constraint *constraint = standard_tonality_minor_degree_constraint((StandardTonalityMinorDegreeRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_minor_degree_rule(STANDARD_TONALITY_MINOR_DEGREE_RULE_COUNT) == NULL);
}

/* Natural: me le te. Harmonic: ti replaces te. Melodic ascending: la ti
   replace le te. Melodic descending is natural again. */
static void test_solfege(void) {
    const char *expected[STANDARD_TONALITY_MINOR_FORM_COUNT][8] = {
        {"do", "re", "me", "fa", "sol", "le", "te", "do"},
        {"do", "re", "me", "fa", "sol", "le", "ti", "do"},
        {"do", "re", "me", "fa", "sol", "la", "ti", "do"},
        {"do", "re", "me", "fa", "sol", "le", "te", "do"},
    };
    for (int form = 0; form < STANDARD_TONALITY_MINOR_FORM_COUNT; form++) {
        for (int degree = 1; degree <= 8; degree++) {
            assert(strcmp(standard_tonality_minor_solfege((StandardTonalityMinorForm)form, degree),
                          expected[form][degree - 1]) == 0);
        }
        assert(standard_tonality_minor_solfege((StandardTonalityMinorForm)form, 0) == NULL);
        assert(standard_tonality_minor_solfege((StandardTonalityMinorForm)form, 9) == NULL);
    }
    assert(standard_tonality_minor_solfege((StandardTonalityMinorForm)99, 1) == NULL);
    assert(standard_tonality_minor_solfege((StandardTonalityMinorForm)-1, 1) == NULL);
}

/* The upper half of the melodic form uses the leading tone ascending and
   the subtonic descending. */
static void test_names(void) {
    for (int form = 0; form < STANDARD_TONALITY_MINOR_FORM_COUNT; form++) {
        StandardTonalityMinorForm f = (StandardTonalityMinorForm)form;
        for (int degree = 1; degree <= 8; degree++) {
            if (degree == 7) {
                continue;
            }
            /* Everything but the seventh is named as in major. */
            assert(strcmp(standard_tonality_minor_degree_name(f, degree), standard_tonality_degree_name(degree)) == 0);
        }
        assert(standard_tonality_minor_degree_name(f, 0) == NULL);
        assert(standard_tonality_minor_degree_name(f, 9) == NULL);
    }
    assert(strcmp(standard_tonality_minor_degree_name(STANDARD_TONALITY_MINOR_NATURAL, 7), "subtonic") == 0);
    assert(strcmp(standard_tonality_minor_degree_name(STANDARD_TONALITY_MINOR_HARMONIC, 7), "leading tone") == 0);
    assert(strcmp(standard_tonality_minor_degree_name(STANDARD_TONALITY_MINOR_MELODIC_ASCENDING, 7), "leading tone") ==
           0);
    assert(strcmp(standard_tonality_minor_degree_name(STANDARD_TONALITY_MINOR_MELODIC_DESCENDING, 7), "subtonic") == 0);
    assert(standard_tonality_minor_degree_name((StandardTonalityMinorForm)99, 1) == NULL);
}

/* Example 9: B melodic minor. A-sharp (leading tone) ascending, A
   (subtonic) descending. */
static void test_subtonic(void) {
    PitchSpelling *b3 = spell(PITCH_LETTER_B, 0, 3);
    PitchSpelling *a_sharp3 = spell(PITCH_LETTER_A, 1, 3);
    PitchSpelling *a3 = spell(PITCH_LETTER_A, 0, 3);
    assert(standard_tonality_is_subtonic(b3, a3));
    assert(!standard_tonality_is_subtonic(b3, a_sharp3)); /* a half step: the leading tone */
    assert(!standard_tonality_is_subtonic(b3, b3));
    assert(!standard_tonality_is_subtonic(b3, NULL));
    assert(!standard_tonality_is_subtonic(NULL, a3));
    pitch_spelling_destroy(b3);
    pitch_spelling_destroy(a_sharp3);
    pitch_spelling_destroy(a3);
}

/* Natural 3 lowered scale degrees, harmonic 2, melodic ascending 1. */
static void test_lowered_degrees(void) {
    int degrees[3];

    assert(standard_tonality_minor_lowered_degrees(STANDARD_TONALITY_MINOR_NATURAL, degrees) == 3);
    assert(degrees[0] == 3 && degrees[1] == 6 && degrees[2] == 7);

    assert(standard_tonality_minor_lowered_degrees(STANDARD_TONALITY_MINOR_HARMONIC, degrees) == 2);
    assert(degrees[0] == 3 && degrees[1] == 6);

    assert(standard_tonality_minor_lowered_degrees(STANDARD_TONALITY_MINOR_MELODIC_ASCENDING, degrees) == 1);
    assert(degrees[0] == 3);

    /* Descending melodic is natural minor: three lowered degrees. */
    assert(standard_tonality_minor_lowered_degrees(STANDARD_TONALITY_MINOR_MELODIC_DESCENDING, degrees) == 3);
    assert(degrees[0] == 3 && degrees[1] == 6 && degrees[2] == 7);

    assert(standard_tonality_minor_lowered_degrees((StandardTonalityMinorForm)99, degrees) == 0);
    assert(standard_tonality_minor_lowered_degrees(STANDARD_TONALITY_MINOR_NATURAL, NULL) == 0);
}

static void test_degree_is_lowered(void) {
    assert(standard_tonality_minor_degree_is_lowered(STANDARD_TONALITY_MINOR_NATURAL, 7));
    assert(!standard_tonality_minor_degree_is_lowered(STANDARD_TONALITY_MINOR_HARMONIC, 7));
    assert(standard_tonality_minor_degree_is_lowered(STANDARD_TONALITY_MINOR_HARMONIC, 6));
    assert(!standard_tonality_minor_degree_is_lowered(STANDARD_TONALITY_MINOR_MELODIC_ASCENDING, 6));
    assert(standard_tonality_minor_degree_is_lowered(STANDARD_TONALITY_MINOR_MELODIC_ASCENDING, 3));
    assert(!standard_tonality_minor_degree_is_lowered(STANDARD_TONALITY_MINOR_NATURAL, 1));
    assert(!standard_tonality_minor_degree_is_lowered(STANDARD_TONALITY_MINOR_NATURAL, 0));
    assert(!standard_tonality_minor_degree_is_lowered((StandardTonalityMinorForm)99, 3));
}

/* Lowered degrees versus the major scale on the same tonic: property over
   every form, checked against the spelled scales. */
static void test_lowered_degrees_match_the_scales(void) {
    PitchSpelling *tonic = spell(PITCH_LETTER_C, 0, 4);
    for (int form = 0; form < STANDARD_TONALITY_MINOR_MELODIC_DESCENDING; form++) {
        StandardTonalityMinorForm f = (StandardTonalityMinorForm)form;
        PitchSpelling *minor[8];
        PitchSpelling *major[8];
        assert(standard_tonality_build_minor_scale(f, tonic, minor));
        assert(standard_tonality_build_major_scale(tonic, major));
        for (int degree = 1; degree <= 7; degree++) {
            int flat_by = pitch_spelling_accidental(major[degree - 1]) - pitch_spelling_accidental(minor[degree - 1]);
            assert((flat_by == 1) == standard_tonality_minor_degree_is_lowered(f, degree));
            assert(flat_by == 0 || flat_by == 1);
        }
        for (int i = 0; i < 8; i++) {
            pitch_spelling_destroy(minor[i]);
            pitch_spelling_destroy(major[i]);
        }
    }
    pitch_spelling_destroy(tonic);
}

static void test_scale_degree_of(void) {
    PitchSpelling *tonic = spell(PITCH_LETTER_A, 0, 3);
    PitchSpelling *g = spell(PITCH_LETTER_G, 0, 5); /* any octave */
    PitchSpelling *g_sharp = spell(PITCH_LETTER_G, 1, 2);
    PitchSpelling *f = spell(PITCH_LETTER_F, 0, 4);
    PitchSpelling *f_sharp = spell(PITCH_LETTER_F, 1, 4);

    assert(standard_tonality_minor_scale_degree_of(STANDARD_TONALITY_MINOR_NATURAL, tonic, g) == 7);
    assert(standard_tonality_minor_scale_degree_of(STANDARD_TONALITY_MINOR_NATURAL, tonic, g_sharp) == 0);
    assert(standard_tonality_minor_scale_degree_of(STANDARD_TONALITY_MINOR_HARMONIC, tonic, g_sharp) == 7);
    assert(standard_tonality_minor_scale_degree_of(STANDARD_TONALITY_MINOR_HARMONIC, tonic, g) == 0);
    assert(standard_tonality_minor_scale_degree_of(STANDARD_TONALITY_MINOR_HARMONIC, tonic, f) == 6);
    assert(standard_tonality_minor_scale_degree_of(STANDARD_TONALITY_MINOR_MELODIC_ASCENDING, tonic, f_sharp) == 6);
    assert(standard_tonality_minor_scale_degree_of(STANDARD_TONALITY_MINOR_MELODIC_ASCENDING, tonic, f) == 0);
    assert(standard_tonality_minor_scale_degree_of(STANDARD_TONALITY_MINOR_MELODIC_DESCENDING, tonic, f) == 6);
    assert(standard_tonality_minor_scale_degree_of(STANDARD_TONALITY_MINOR_MELODIC_DESCENDING, tonic, f_sharp) == 0);

    assert(standard_tonality_minor_scale_degree_of(STANDARD_TONALITY_MINOR_NATURAL, tonic, NULL) == 0);
    assert(standard_tonality_minor_scale_degree_of(STANDARD_TONALITY_MINOR_NATURAL, NULL, g) == 0);
    assert(standard_tonality_minor_scale_degree_of((StandardTonalityMinorForm)99, tonic, g) == 0);

    pitch_spelling_destroy(tonic);
    pitch_spelling_destroy(g);
    pitch_spelling_destroy(g_sharp);
    pitch_spelling_destroy(f);
    pitch_spelling_destroy(f_sharp);
}

static void test_build_minor_scale_degree(void) {
    for (int form = 0; form < STANDARD_TONALITY_MINOR_FORM_COUNT; form++) {
        for (int degree = 1; degree <= 8; degree++) {
            EntityID *scale_id;
            assert(entity_id_create("scale-test-a-minor", &scale_id) == ENTITY_ID_OK);
            PitchIdentity *pitch;
            assert(pitch_identity_create("pitch-token", 11, &pitch) == PITCH_IDENTITY_OK);

            ScaleDegree *scale_degree = NULL;
            assert(standard_tonality_build_minor_scale_degree((StandardTonalityMinorForm)form, degree, scale_id, pitch,
                                                              &scale_degree) == SCALE_DEGREE_OK);
            assert(strcmp(scale_degree_label(scale_degree),
                          standard_tonality_minor_degree_name((StandardTonalityMinorForm)form, degree)) == 0);
            assert(scale_degree_pitch(scale_degree) == pitch);

            const Interpretation *interpretation = scale_degree_interpretation(scale_degree);
            assert(interpretation_framework_reference(interpretation) != NULL);
            assert(entity_id_equals(interpretation_target_id(interpretation), scale_id));
            assert(strcmp(interpretation_method(interpretation), "minor-scale-degree-vocabulary") == 0);

            scale_degree_destroy(scale_degree);
        }
    }
}

/* Forms of the same degree get different interpretation ids. */
static void test_interpretation_ids_differ_by_form(void) {
    ScaleDegree *natural = NULL;
    ScaleDegree *harmonic = NULL;
    EntityID *id_a;
    EntityID *id_b;
    PitchIdentity *pitch_a;
    PitchIdentity *pitch_b;
    assert(entity_id_create("scale-a", &id_a) == ENTITY_ID_OK);
    assert(entity_id_create("scale-b", &id_b) == ENTITY_ID_OK);
    assert(pitch_identity_create("pitch-a", 7, &pitch_a) == PITCH_IDENTITY_OK);
    assert(pitch_identity_create("pitch-b", 7, &pitch_b) == PITCH_IDENTITY_OK);
    assert(standard_tonality_build_minor_scale_degree(STANDARD_TONALITY_MINOR_NATURAL, 7, id_a, pitch_a, &natural) ==
           SCALE_DEGREE_OK);
    assert(standard_tonality_build_minor_scale_degree(STANDARD_TONALITY_MINOR_HARMONIC, 7, id_b, pitch_b, &harmonic) ==
           SCALE_DEGREE_OK);
    assert(!entity_id_equals(interpretation_id(scale_degree_interpretation(natural)),
                             interpretation_id(scale_degree_interpretation(harmonic))));
    assert(strcmp(scale_degree_label(natural), "subtonic") == 0);
    assert(strcmp(scale_degree_label(harmonic), "leading tone") == 0);
    scale_degree_destroy(natural);
    scale_degree_destroy(harmonic);
}

static void test_build_rejects_invalid_arguments(void) {
    EntityID *scale_id;
    assert(entity_id_create("scale-test-reject", &scale_id) == ENTITY_ID_OK);
    PitchIdentity *pitch;
    assert(pitch_identity_create("pitch-token", 11, &pitch) == PITCH_IDENTITY_OK);

    ScaleDegree *scale_degree = NULL;
    assert(standard_tonality_build_minor_scale_degree(STANDARD_TONALITY_MINOR_NATURAL, 0, scale_id, pitch,
                                                      &scale_degree) == SCALE_DEGREE_ERR_NULL_LABEL);
    assert(standard_tonality_build_minor_scale_degree(STANDARD_TONALITY_MINOR_NATURAL, 9, scale_id, pitch,
                                                      &scale_degree) == SCALE_DEGREE_ERR_NULL_LABEL);
    assert(standard_tonality_build_minor_scale_degree((StandardTonalityMinorForm)99, 1, scale_id, pitch,
                                                      &scale_degree) == SCALE_DEGREE_ERR_NULL_LABEL);
    assert(scale_degree == NULL);

    /* Caller still owns both on failure. */
    entity_id_destroy(scale_id);
    pitch_identity_destroy(pitch);
}

int main(void) {
    test_rules_reference_framework();
    test_solfege();
    test_names();
    test_subtonic();
    test_lowered_degrees();
    test_degree_is_lowered();
    test_lowered_degrees_match_the_scales();
    test_scale_degree_of();
    test_build_minor_scale_degree();
    test_interpretation_ids_differ_by_form();
    test_build_rejects_invalid_arguments();
    printf("test_minor_scale_degree_rules (standard-tonality): ok\n");
    return 0;
}
