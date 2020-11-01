#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "framework.h"
#include "identity/entity_id.h"
#include "interpretation/scale_degree.h"
#include "pitch/pitch_identity.h"
#include "scale_degree_rules.h"
#include "spelling_test_util.h"

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_SCALE_DEGREE_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_scale_degree_rule((StandardTonalityScaleDegreeRule)i);
        const Constraint *constraint = standard_tonality_scale_degree_constraint((StandardTonalityScaleDegreeRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_scale_degree_rule(STANDARD_TONALITY_SCALE_DEGREE_RULE_COUNT) == NULL);
}

static void test_solfege(void) {
    const char *expected[] = {"do", "re", "mi", "fa", "sol", "la", "ti", "do"};
    for (int degree = 1; degree <= 8; degree++) {
        assert(strcmp(standard_tonality_solfege(degree), expected[degree - 1]) == 0);
    }
    assert(standard_tonality_solfege(0) == NULL);
    assert(standard_tonality_solfege(9) == NULL);
    assert(standard_tonality_solfege(-1) == NULL);
}

static void test_degree_names(void) {
    const char *expected[] = {"tonic",    "supertonic", "mediant", "subdominant",
                              "dominant", "submediant", "leading tone", "tonic"};
    for (int degree = 1; degree <= 8; degree++) {
        assert(strcmp(standard_tonality_degree_name(degree), expected[degree - 1]) == 0);
    }
    assert(standard_tonality_degree_name(0) == NULL);
    assert(standard_tonality_degree_name(9) == NULL);
}

static void test_name_derivation(void) {
    int interval = 0;
    bool below = true;

    assert(standard_tonality_degree_derivation(2, &interval, &below)); /* supertonic: a second above */
    assert(interval == 2 && !below);
    assert(standard_tonality_degree_derivation(3, &interval, &below)); /* mediant: a third above */
    assert(interval == 3 && !below);
    assert(standard_tonality_degree_derivation(5, &interval, &below)); /* dominant: a fifth above */
    assert(interval == 5 && !below);
    assert(standard_tonality_degree_derivation(4, &interval, &below)); /* subdominant: a fifth below */
    assert(interval == 5 && below);
    assert(standard_tonality_degree_derivation(6, &interval, &below)); /* submediant: a third below */
    assert(interval == 3 && below);

    interval = 99;
    below = false;
    assert(!standard_tonality_degree_derivation(1, &interval, &below));
    assert(!standard_tonality_degree_derivation(7, &interval, &below));
    assert(!standard_tonality_degree_derivation(8, &interval, &below));
    assert(!standard_tonality_degree_derivation(0, &interval, &below));
    assert(interval == 99 && !below); /* untouched */
    assert(!standard_tonality_degree_derivation(2, NULL, &below));
    assert(!standard_tonality_degree_derivation(2, &interval, NULL));
}

static void test_leading_tone(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *b3 = spell(PITCH_LETTER_B, 0, 3);
    PitchSpelling *b_flat3 = spell(PITCH_LETTER_B, -1, 3);
    PitchSpelling *d4 = spell(PITCH_LETTER_D, 0, 4);
    assert(standard_tonality_is_leading_tone(c4, b3));
    assert(!standard_tonality_is_leading_tone(c4, b_flat3)); /* a whole step below: the subtonic */
    assert(!standard_tonality_is_leading_tone(c4, d4));
    assert(!standard_tonality_is_leading_tone(c4, c4));
    assert(!standard_tonality_is_leading_tone(c4, NULL));
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(b3);
    pitch_spelling_destroy(b_flat3);
    pitch_spelling_destroy(d4);
}

static void test_scale_degree_of(void) {
    /* D major: D E F-sharp G A B C-sharp. */
    PitchSpelling *tonic = spell(PITCH_LETTER_D, 0, 4);
    PitchLetter letters[] = {PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F, PITCH_LETTER_G,
                             PITCH_LETTER_A, PITCH_LETTER_B, PITCH_LETTER_C};
    int accidentals[] = {0, 0, 1, 0, 0, 0, 1};
    for (int i = 0; i < 7; i++) {
        PitchSpelling *note = spell(letters[i], accidentals[i], 2); /* any octave */
        assert(standard_tonality_scale_degree_of(tonic, note) == i + 1);
        pitch_spelling_destroy(note);
    }

    PitchSpelling *f_natural = spell(PITCH_LETTER_F, 0, 4);
    PitchSpelling *c_natural = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *g_flat = spell(PITCH_LETTER_G, -1, 4);
    assert(standard_tonality_scale_degree_of(tonic, f_natural) == 0);
    assert(standard_tonality_scale_degree_of(tonic, c_natural) == 0);
    assert(standard_tonality_scale_degree_of(tonic, g_flat) == 0);
    assert(standard_tonality_scale_degree_of(tonic, NULL) == 0);
    assert(standard_tonality_scale_degree_of(NULL, f_natural) == 0);

    PitchSpelling *g_double_sharp = spell(PITCH_LETTER_G, 2, 4); /* has no major scale */
    assert(standard_tonality_scale_degree_of(g_double_sharp, f_natural) == 0);

    pitch_spelling_destroy(tonic);
    pitch_spelling_destroy(f_natural);
    pitch_spelling_destroy(c_natural);
    pitch_spelling_destroy(g_flat);
    pitch_spelling_destroy(g_double_sharp);
}

/* A-flat major, per the source's example: A-flat is do, G is ti. */
static void test_a_flat_major_degrees(void) {
    PitchSpelling *tonic = spell(PITCH_LETTER_A, -1, 3);
    PitchSpelling *d_flat = spell(PITCH_LETTER_D, -1, 4);
    PitchSpelling *e_flat = spell(PITCH_LETTER_E, -1, 4);
    PitchSpelling *g = spell(PITCH_LETTER_G, 0, 4);
    assert(standard_tonality_scale_degree_of(tonic, d_flat) == 4); /* subdominant: a fifth below A-flat */
    assert(standard_tonality_scale_degree_of(tonic, e_flat) == 5); /* dominant: a fifth above A-flat */
    assert(standard_tonality_scale_degree_of(tonic, g) == 7);      /* leading tone */
    assert(standard_tonality_is_leading_tone(tonic, g));           /* a half step below A-flat */
    pitch_spelling_destroy(tonic);
    pitch_spelling_destroy(d_flat);
    pitch_spelling_destroy(e_flat);
    pitch_spelling_destroy(g);
}

static void test_build_scale_degree(void) {
    for (int degree = 1; degree <= 8; degree++) {
        EntityID *scale_id;
        assert(entity_id_create("scale-test-d-major", &scale_id) == ENTITY_ID_OK);
        PitchIdentity *pitch;
        assert(pitch_identity_create("pitch-token", 11, &pitch) == PITCH_IDENTITY_OK);

        ScaleDegree *scale_degree = NULL;
        assert(standard_tonality_build_scale_degree(degree, scale_id, pitch, &scale_degree) == SCALE_DEGREE_OK);
        assert(strcmp(scale_degree_label(scale_degree), standard_tonality_degree_name(degree)) == 0);
        assert(scale_degree_pitch(scale_degree) == pitch);

        const Interpretation *interpretation = scale_degree_interpretation(scale_degree);
        assert(interpretation_framework_reference(interpretation) != NULL);
        assert(entity_id_equals(interpretation_target_id(interpretation), scale_id));
        assert(strcmp(interpretation_author(interpretation), "standard-tonality-v1") == 0);

        scale_degree_destroy(scale_degree);
    }
}

static void test_build_scale_degree_rejects_invalid_degree(void) {
    EntityID *scale_id;
    assert(entity_id_create("scale-test-reject", &scale_id) == ENTITY_ID_OK);
    PitchIdentity *pitch;
    assert(pitch_identity_create("pitch-token", 11, &pitch) == PITCH_IDENTITY_OK);

    ScaleDegree *scale_degree = NULL;
    assert(standard_tonality_build_scale_degree(0, scale_id, pitch, &scale_degree) == SCALE_DEGREE_ERR_NULL_LABEL);
    assert(standard_tonality_build_scale_degree(9, scale_id, pitch, &scale_degree) == SCALE_DEGREE_ERR_NULL_LABEL);
    assert(scale_degree == NULL);

    /* Caller still owns both on failure. */
    entity_id_destroy(scale_id);
    pitch_identity_destroy(pitch);
}

int main(void) {
    test_rules_reference_framework();
    test_solfege();
    test_degree_names();
    test_name_derivation();
    test_leading_tone();
    test_scale_degree_of();
    test_a_flat_major_degrees();
    test_build_scale_degree();
    test_build_scale_degree_rejects_invalid_degree();
    printf("test_scale_degree_rules (standard-tonality): ok\n");
    return 0;
}
