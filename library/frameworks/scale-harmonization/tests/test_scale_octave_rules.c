#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "scale_octave_rules.h"
#include "scale_rule_check.h"
#include "scale_test_util.h"

#define OCTAVE(name) SCALE_OCTAVE_##name

static bool holds(ScaleOctaveRule rule, const ScaleHarmonization *harmonization) {
    return scale_octave_rule_holds(rule, harmonization);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(scale_octave_rule, scale_octave_constraint, SCALE_OCTAVE_RULE_COUNT);
    for (int i = 0; i < SCALE_OCTAVE_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(scale_octave_constraint((ScaleOctaveRule)i));
        assert(type == (i <= OCTAVE(NO_PARALLELS) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(scale_octave_rule_name(OCTAVE(FOUR_VOICES)), "four-voices") == 0);
    assert(strcmp(scale_octave_rule_name(OCTAVE(BASS_IS_SCALE)), "bass-is-scale") == 0);
    assert(strcmp(scale_octave_rule_name(OCTAVE(TONIC_CHORDS)), "tonic-chords") == 0);
    assert(strcmp(scale_octave_rule_name(OCTAVE(NO_PARALLELS)), "no-parallels") == 0);
    assert(strcmp(scale_octave_rule_name(OCTAVE(FIRST_INVERSION)), "first-inversion") == 0);
    assert(scale_octave_rule_name(OCTAVE(RULE_COUNT)) == NULL);
}

/* A realization over the ascending scale in C: a 5/3 chord on the tonic, first
   inversion chords on the degrees between, and a 5/3 chord on the octave, in
   four voices with no parallel fifths or octaves. Upper voices from the lowest. */
static void ascending(ScaleFixture *f) {
    const PitchSpelling *bass[8] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3), P(F, 0, 3),
                                    P(G, 0, 3), P(A, 0, 3), P(B, 0, 3), P(C, 0, 4)};
    const PitchSpelling *const chords[8][3] = {
        {P(E, 0, 3), P(G, 0, 3), P(C, 0, 4)}, {P(F, 0, 3), P(B, 0, 3), P(F, 0, 4)},
        {P(G, 0, 3), P(C, 0, 4), P(E, 0, 4)}, {P(A, 0, 3), P(D, 0, 4), P(A, 0, 4)},
        {P(B, 0, 3), P(E, 0, 4), P(G, 0, 4)}, {P(C, 0, 4), P(F, 0, 4), P(C, 0, 5)},
        {P(D, 0, 4), P(G, 0, 4), P(B, 0, 4)}, {P(E, 0, 4), P(G, 0, 4), P(E, 0, 5)}};
    octave_fixture(f, SCALE_DIRECTION_ASCENDING, P(C, 0, 4), bass, chords);
}

static void descending(ScaleFixture *f) {
    const PitchSpelling *bass[8] = {P(C, 0, 4), P(B, 0, 3), P(A, 0, 3), P(G, 0, 3),
                                    P(F, 0, 3), P(E, 0, 3), P(D, 0, 3), P(C, 0, 3)};
    const PitchSpelling *const chords[8][3] = {
        {P(E, 0, 4), P(G, 0, 4), P(C, 0, 5)}, {P(D, 0, 4), P(G, 0, 4), P(D, 0, 5)},
        {P(C, 0, 4), P(F, 0, 4), P(A, 0, 4)}, {P(B, 0, 3), P(E, 0, 4), P(B, 0, 4)},
        {P(A, 0, 3), P(D, 0, 4), P(D, 0, 5)}, {P(G, 0, 3), P(C, 0, 4), P(E, 0, 5)},
        {P(F, 0, 3), P(B, 0, 3), P(B, 0, 4)}, {P(E, 0, 3), P(G, 0, 3), P(C, 0, 5)}};
    octave_fixture(f, SCALE_DIRECTION_DESCENDING, P(C, 0, 4), bass, chords);
}

static void test_good_realizations(void) {
    ScaleFixture up, down;
    ascending(&up);
    descending(&down);
    assert(scale_harmonization_valid(&up.harmonization));
    assert(scale_harmonization_valid(&down.harmonization));
    for (int i = 0; i < SCALE_OCTAVE_RULE_COUNT; i++) {
        assert(holds((ScaleOctaveRule)i, &up.harmonization));
        assert(holds((ScaleOctaveRule)i, &down.harmonization));
    }
}

static void test_four_voices(void) {
    ScaleFixture f;
    ascending(&f);
    f.harmonization.voice_count = 3;
    assert(!holds(OCTAVE(FOUR_VOICES), &f.harmonization));
    f.harmonization.voice_count = 5;
    f.stages[0].pitches[4] = P(C, 0, 5);
    for (size_t i = 1; i < 8; i++) {
        f.stages[i].pitches[4] = P(C, 0, 5);
    }
    assert(!holds(OCTAVE(FOUR_VOICES), &f.harmonization));
}

static void test_bass_is_scale(void) {
    ScaleFixture f;
    ascending(&f);
    /* Seven stages, not eight. */
    f.harmonization.stage_count = 7;
    assert(!holds(OCTAVE(BASS_IS_SCALE), &f.harmonization));

    /* A degree out of place: the sixth where the fifth belongs. */
    ascending(&f);
    f.stages[4].pitches[0] = P(A, 0, 3);
    assert(!holds(OCTAVE(BASS_IS_SCALE), &f.harmonization));

    /* The ascending scale harmonized as a descending one. */
    ascending(&f);
    f.harmonization.direction = SCALE_DIRECTION_DESCENDING;
    assert(!holds(OCTAVE(BASS_IS_SCALE), &f.harmonization));

    /* A raised degree is not in the scale. */
    ascending(&f);
    f.stages[3].pitches[0] = P(F, 1, 3);
    assert(!holds(OCTAVE(BASS_IS_SCALE), &f.harmonization));
}

static void test_bass_scale_in_minor(void) {
    /* The A minor scale: the third, sixth, and seventh lowered going up the
       natural form. */
    const PitchSpelling *bass[8] = {P(A, 0, 2), P(B, 0, 2), P(C, 0, 3), P(D, 0, 3),
                                    P(E, 0, 3), P(F, 0, 3), P(G, 0, 3), P(A, 0, 3)};
    const PitchSpelling *const chords[8][3] = {
        {P(C, 0, 3), P(E, 0, 3), P(A, 0, 3)}, {P(D, 0, 3), P(G, 0, 3), P(D, 0, 4)},
        {P(E, 0, 3), P(A, 0, 3), P(C, 0, 4)}, {P(F, 0, 3), P(B, 0, 3), P(D, 0, 4)},
        {P(G, 0, 3), P(C, 0, 4), P(E, 0, 4)}, {P(A, 0, 3), P(D, 0, 4), P(F, 0, 4)},
        {P(B, 0, 3), P(E, 0, 4), P(G, 0, 4)}, {P(C, 0, 4), P(E, 0, 4), P(A, 0, 4)}};
    ScaleFixture f;
    octave_fixture(&f, SCALE_DIRECTION_ASCENDING, P(A, 0, 3), bass, chords);
    assert(holds(OCTAVE(BASS_IS_SCALE), &f.harmonization));
}

static void test_tonic_chords(void) {
    ScaleFixture f;
    ascending(&f);
    /* A first inversion chord at the start: a sixth above the bass. */
    f.stages[0].pitches[2] = P(A, 0, 3);
    assert(!holds(OCTAVE(TONIC_CHORDS), &f.harmonization));

    /* No fifth in the last chord. */
    ascending(&f);
    f.stages[7].pitches[2] = P(E, 0, 4);
    assert(!holds(OCTAVE(TONIC_CHORDS), &f.harmonization));

    /* An upper voice below the bass. */
    ascending(&f);
    f.stages[0].pitches[1] = P(E, 0, 2);
    assert(!holds(OCTAVE(TONIC_CHORDS), &f.harmonization));
}

static void test_no_parallels(void) {
    ScaleFixture f;
    ascending(&f);
    /* Parallel octaves between the bass and the top voice: the top voice runs C
       to D an octave above the bass. */
    f.stages[0].pitches[3] = P(C, 0, 4);
    f.stages[1].pitches[3] = P(D, 0, 4);
    assert(!holds(OCTAVE(NO_PARALLELS), &f.harmonization));

    /* Parallel fifths between the bass and an inner voice: G up to A over C up to D. */
    ascending(&f);
    f.stages[0].pitches[1] = P(G, 0, 3);
    f.stages[1].pitches[1] = P(A, 0, 3);
    assert(!holds(OCTAVE(NO_PARALLELS), &f.harmonization));

    /* A bass that holds its note, so the voices do not move together, makes no
       parallels. */
    ascending(&f);
    f.stages[1].pitches[0] = P(C, 0, 3);
    f.stages[1].pitches[1] = P(F, 0, 3);
    assert(holds(OCTAVE(NO_PARALLELS), &f.harmonization));
}

static void test_first_inversion(void) {
    ScaleFixture f;
    ascending(&f);
    /* A root position chord between the ends: a fifth above the bass. */
    f.stages[3].pitches[2] = P(C, 0, 4);
    assert(!holds(OCTAVE(FIRST_INVERSION), &f.harmonization));
    /* That is advice only: the hard rules still hold. */
    assert(holds(OCTAVE(TONIC_CHORDS), &f.harmonization));
    assert(holds(OCTAVE(NO_PARALLELS), &f.harmonization));
}

static void test_invalid_and_check(void) {
    ScaleFixture f;
    ascending(&f);
    assert(!holds(OCTAVE(RULE_COUNT), &f.harmonization));
    assert(!holds((ScaleOctaveRule)-1, &f.harmonization));
    assert(!holds(OCTAVE(FOUR_VOICES), NULL));

    unsigned hard = 7, soft = 7;
    assert(!scale_octave_check(NULL, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!scale_octave_check(&f.harmonization, NULL, &soft));
    assert(!scale_octave_check(&f.harmonization, &hard, NULL));

    assert(scale_octave_check(&f.harmonization, &hard, &soft));
    assert(hard == 0 && soft == 0);

    /* A root position chord in the middle: advice. */
    f.stages[3].pitches[2] = P(C, 0, 4);
    assert(scale_octave_check(&f.harmonization, &hard, &soft));
    assert(hard == 0);
    assert(soft == (1u << OCTAVE(FIRST_INVERSION)));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_realizations();
    test_four_voices();
    test_bass_is_scale();
    test_bass_scale_in_minor();
    test_tonic_chords();
    test_no_parallels();
    test_first_inversion();
    test_invalid_and_check();
    printf("test_scale_octave_rules: ok\n");
    return 0;
}
