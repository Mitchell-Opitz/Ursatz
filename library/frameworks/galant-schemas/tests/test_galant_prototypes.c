#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "galant_prototypes.h"
#include "galant_specs_a.h"
#include "galant_test_util.h"

static void assert_degree(const GalantDegree *d, int degree, GalantAlteration alteration) {
    assert(d->degree == degree && d->alteration == alteration);
}

static void test_valid_and_names(void) {
    for (int i = 0; i < GALANT_SCHEMA_COUNT; i++) {
        assert(galant_schema_valid((GalantSchemaId)i));
        assert(galant_prototype((GalantSchemaId)i) != NULL);
        assert(galant_prototype((GalantSchemaId)i)->id == (GalantSchemaId)i);
    }
    assert(!galant_schema_valid(GALANT_SCHEMA_COUNT));
    assert(!galant_schema_valid((GalantSchemaId)-1));
    assert(galant_prototype(GALANT_SCHEMA_COUNT) == NULL);

    assert(strcmp(galant_schema_name(GALANT_SCHEMA_MEYER), "meyer") == 0);
    assert(strcmp(galant_schema_name(GALANT_SCHEMA_PRINNER), "prinner") == 0);
    assert(strcmp(galant_schema_name(GALANT_SCHEMA_MODULATING_PRINNER), "modulating-prinner") == 0);
    assert(galant_schema_name(GALANT_SCHEMA_COUNT) == NULL);
    assert(strcmp(galant_prototype(GALANT_SCHEMA_PRINNER)->name, "prinner") == 0);
}

static void test_meyer(void) {
    const GalantPrototype *p = galant_prototype(GALANT_SCHEMA_MEYER);
    assert(p->function == SCHEMA_FUNCTION_OPENING_GAMBIT);
    assert(p->stage_count == 4);
    assert(!p->bass_defining && !p->melody_in_tenths);

    const BeatStrength strengths[4] = {W, S, W, S};
    const int melody[4] = {1, 7, 4, 3};
    const int bass[4] = {1, 2, 7, 1};
    const char *numerals[4] = {"I", "V", "V", "I"};
    for (size_t i = 0; i < 4; i++) {
        assert(p->stages[i].strength == strengths[i]);
        assert_degree(&p->stages[i].melody, melody[i], GALANT_ALTERATION_NATURAL);
        assert_degree(&p->stages[i].bass, bass[i], GALANT_ALTERATION_NATURAL);
        assert(strcmp(p->stages[i].numeral, numerals[i]) == 0);
        assert(p->stages[i].numeral_dominant == NULL);
    }
    /* The first and last stages are tonic chords; the two in the middle are not. */
    assert(strcmp(p->stages[0].numeral, p->stages[3].numeral) == 0);
}

static void test_prinner(void) {
    const GalantPrototype *p = galant_prototype(GALANT_SCHEMA_PRINNER);
    assert(p->function == SCHEMA_FUNCTION_CONTINUATION);
    assert(p->stage_count == 4);
    assert(p->bass_defining && p->melody_in_tenths);

    const BeatStrength strengths[4] = {S, W, S, W};
    for (size_t i = 0; i < 4; i++) {
        assert(p->stages[i].strength == strengths[i]);
        assert(p->stages[i].numeral_dominant == NULL);
    }
    /* la/le sol fa mi/me over fa mi/me re do. */
    assert_degree(&p->stages[0].melody, 6, GALANT_ALTERATION_EITHER);
    assert_degree(&p->stages[1].melody, 5, GALANT_ALTERATION_NATURAL);
    assert_degree(&p->stages[2].melody, 4, GALANT_ALTERATION_NATURAL);
    assert_degree(&p->stages[3].melody, 3, GALANT_ALTERATION_EITHER);
    assert_degree(&p->stages[0].bass, 4, GALANT_ALTERATION_NATURAL);
    assert_degree(&p->stages[1].bass, 3, GALANT_ALTERATION_EITHER);
    assert_degree(&p->stages[2].bass, 2, GALANT_ALTERATION_NATURAL);
    assert_degree(&p->stages[3].bass, 1, GALANT_ALTERATION_NATURAL);

    const char *figures[4] = {"5", "6", "7-6", "5"};
    const char *numerals[4] = {"IV", "I", "vii", "I"};
    for (size_t i = 0; i < 4; i++) {
        assert(strcmp(p->stages[i].figures, figures[i]) == 0);
        assert(strcmp(p->stages[i].numeral, numerals[i]) == 0);
    }
}

static void test_modulating_prinner(void) {
    const GalantPrototype *p = galant_prototype(GALANT_SCHEMA_MODULATING_PRINNER);
    assert(p->function == SCHEMA_FUNCTION_CONTINUATION);
    assert(p->stage_count == 4);
    assert(p->bass_defining && p->melody_in_tenths);

    const BeatStrength strengths[4] = {S, W, S, W};
    const int melody[4] = {3, 2, 1, 7};
    const int bass[4] = {1, 7, 6, 5};
    for (size_t i = 0; i < 4; i++) {
        assert(p->stages[i].strength == strengths[i]);
        assert_degree(&p->stages[i].melody, melody[i], GALANT_ALTERATION_NATURAL);
        assert_degree(&p->stages[i].bass, bass[i], GALANT_ALTERATION_NATURAL);
    }
    const char *figures[4] = {"5", "6", "7-#6", "5"};
    const char *tonic_key[4] = {"I", "V", "vii/V", "V"};
    const char *dominant_key[4] = {"IV", "I", "vii", "I"};
    for (size_t i = 0; i < 4; i++) {
        assert(strcmp(p->stages[i].figures, figures[i]) == 0);
        assert(strcmp(p->stages[i].numeral, tonic_key[i]) == 0);
        assert(strcmp(p->stages[i].numeral_dominant, dominant_key[i]) == 0);
    }
}

static void test_opening_and_continuation_data(void) {
    for (size_t i = 0; i < SPECS_A_COUNT; i++) {
        verify_spec(&SPECS_A[i]);
    }
}

static void test_dominant_key_numerals(void) {
    /* Only the modulating Prinner has numerals in the dominant key. */
    for (size_t i = 0; i < SPECS_A_COUNT; i++) {
        const GalantPrototype *p = galant_prototype(SPECS_A[i].id);
        for (size_t s = 0; s < p->stage_count; s++) {
            assert((p->stages[s].numeral_dominant != NULL) == (p->id == GALANT_SCHEMA_MODULATING_PRINNER));
        }
    }
}

static void test_every_prototype_is_well_formed(void) {
    for (int i = 0; i < GALANT_SCHEMA_COUNT; i++) {
        const GalantPrototype *p = galant_prototype((GalantSchemaId)i);
        assert(schema_function_valid(p->function));
        assert(p->stage_count >= 2 && p->stage_count <= GALANT_MAX_STAGES);
        for (size_t s = 0; s < p->stage_count; s++) {
            assert(p->stages[s].strength == BEAT_STRENGTH_STRONG || p->stages[s].strength == BEAT_STRENGTH_WEAK);
            assert(p->stages[s].melody.degree >= 1 && p->stages[s].melody.degree <= 7);
            assert(p->stages[s].bass.degree >= 1 && p->stages[s].bass.degree <= 7);
            assert(p->stages[s].numeral != NULL);
        }
    }
}

int main(void) {
    test_valid_and_names();
    test_meyer();
    test_prinner();
    test_modulating_prinner();
    test_opening_and_continuation_data();
    test_dominant_key_numerals();
    test_every_prototype_is_well_formed();
    printf("test_galant_prototypes: ok\n");
    return 0;
}
