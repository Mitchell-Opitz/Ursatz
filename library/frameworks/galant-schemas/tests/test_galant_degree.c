#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "galant_degree.h"
#include "galant_exemplar.h"
#include "galant_test_util.h"

static GalantPitchDegree degree(const PitchSpelling *pitch, const PitchSpelling *tonic) {
    GalantPitchDegree d = {0, 99};
    assert(galant_degree_of(pitch, tonic, &d));
    return d;
}

static void test_major_scale(void) {
    const PitchSpelling *c4 = P(C, 0, 4);
    const PitchLetter letters[7] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                                    PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    for (int i = 0; i < 7; i++) {
        GalantPitchDegree d = degree(note(letters[i], 0, 4), c4);
        assert(d.degree == i + 1 && d.alteration == 0);
    }
}

static void test_alterations(void) {
    const PitchSpelling *c4 = P(C, 0, 4);
    GalantPitchDegree me = degree(P(E, -1, 4), c4);
    assert(me.degree == 3 && me.alteration == -1);
    GalantPitchDegree te = degree(P(B, -1, 4), c4);
    assert(te.degree == 7 && te.alteration == -1);
    GalantPitchDegree fi = degree(P(F, 1, 4), c4);
    assert(fi.degree == 4 && fi.alteration == 1);
    /* B sharp just below C is a raised ti, not a lowered do. */
    GalantPitchDegree raised_ti = degree(P(B, 1, 3), c4);
    assert(raised_ti.degree == 7 && raised_ti.alteration == 1);
}

static void test_octave_does_not_matter(void) {
    const PitchSpelling *tonic = P(C, 0, 4);
    GalantPitchDegree low = degree(P(G, 0, 2), tonic);
    GalantPitchDegree high = degree(P(G, 0, 6), tonic);
    assert(low.degree == 5 && low.alteration == 0);
    assert(high.degree == 5 && high.alteration == 0);
    GalantPitchDegree below = degree(P(G, 0, 3), tonic);
    assert(below.degree == 5 && below.alteration == 0);
}

static void test_minor_key(void) {
    const PitchSpelling *a4 = P(A, 0, 4);
    GalantPitchDegree ti = degree(P(G, 1, 4), a4);
    assert(ti.degree == 7 && ti.alteration == 0);
    GalantPitchDegree me = degree(P(C, 0, 5), a4);
    assert(me.degree == 3 && me.alteration == -1);
    GalantPitchDegree le = degree(P(F, 0, 4), a4);
    assert(le.degree == 6 && le.alteration == -1);
}

static void test_degree_of_arguments(void) {
    const PitchSpelling *c4 = P(C, 0, 4);
    GalantPitchDegree d = {0, 99};
    assert(!galant_degree_of(NULL, c4, &d));
    assert(!galant_degree_of(c4, NULL, &d));
    assert(!galant_degree_of(c4, c4, NULL));
    assert(d.degree == 0 && d.alteration == 99);
}

static void test_degree_allows(void) {
    GalantPitchDegree natural = {3, 0};
    GalantPitchDegree lowered = {3, -1};
    GalantPitchDegree raised = {3, 1};
    GalantPitchDegree other = {4, 0};

    GalantDegree want_natural = {3, GALANT_ALTERATION_NATURAL};
    assert(galant_degree_allows(&want_natural, &natural));
    assert(!galant_degree_allows(&want_natural, &lowered));
    assert(!galant_degree_allows(&want_natural, &other));

    GalantDegree want_lowered = {3, GALANT_ALTERATION_LOWERED};
    assert(!galant_degree_allows(&want_lowered, &natural));
    assert(galant_degree_allows(&want_lowered, &lowered));

    GalantDegree want_either = {3, GALANT_ALTERATION_EITHER};
    assert(galant_degree_allows(&want_either, &natural));
    assert(galant_degree_allows(&want_either, &lowered));
    assert(!galant_degree_allows(&want_either, &raised));
    assert(!galant_degree_allows(&want_either, &other));

    GalantDegree invalid = {3, (GalantAlteration)9};
    assert(!galant_degree_allows(&invalid, &natural));
    assert(!galant_degree_allows(NULL, &natural));
    assert(!galant_degree_allows(&want_either, NULL));
}

static void test_alteration_names(void) {
    assert(strcmp(galant_alteration_name(GALANT_ALTERATION_NATURAL), "natural") == 0);
    assert(strcmp(galant_alteration_name(GALANT_ALTERATION_LOWERED), "lowered") == 0);
    assert(strcmp(galant_alteration_name(GALANT_ALTERATION_EITHER), "either") == 0);
    assert(galant_alteration_name((GalantAlteration)9) == NULL);
}

static void test_exemplar_valid(void) {
    GalantFixture f;
    meyer_in_c(&f);
    assert(galant_exemplar_valid(&f.exemplar));
    assert(!galant_exemplar_valid(NULL));

    GalantExemplar no_stages = f.exemplar;
    no_stages.stage_count = 0;
    assert(!galant_exemplar_valid(&no_stages));

    GalantExemplar too_many = f.exemplar;
    too_many.stage_count = GALANT_MAX_STAGES + 1;
    assert(!galant_exemplar_valid(&too_many));

    GalantExemplar no_tonic = f.exemplar;
    no_tonic.tonic = NULL;
    assert(!galant_exemplar_valid(&no_tonic));

    GalantExemplar no_stage_array = f.exemplar;
    no_stage_array.stages = NULL;
    assert(!galant_exemplar_valid(&no_stage_array));

    const PitchSpelling *saved = f.stages[2].melody;
    f.stages[2].melody = NULL;
    assert(!galant_exemplar_valid(&f.exemplar));
    f.stages[2].melody = saved;

    saved = f.stages[1].bass;
    f.stages[1].bass = NULL;
    assert(!galant_exemplar_valid(&f.exemplar));
    f.stages[1].bass = saved;

    f.stages[0].strength = BEAT_STRENGTH_COUNT;
    assert(!galant_exemplar_valid(&f.exemplar));
    f.stages[0].strength = BEAT_STRENGTH_MEDIUM;
    assert(galant_exemplar_valid(&f.exemplar));
}

int main(void) {
    test_major_scale();
    test_alterations();
    test_octave_does_not_matter();
    test_minor_key();
    test_degree_of_arguments();
    test_degree_allows();
    test_alteration_names();
    test_exemplar_valid();
    printf("test_galant_degree: ok\n");
    return 0;
}
