#include <assert.h>
#include <stdio.h>

#include "scale_harmonization.h"
#include "scale_test_util.h"

static void test_valid(void) {
    const PitchSpelling *bass[] = {P(C, 0, 3), P(D, 0, 3)};
    const PitchSpelling *upper[] = {P(A, 0, 3), P(B, 0, 3)};
    ScaleFixture f;
    pair_fixture(&f, SCALE_DIRECTION_ASCENDING, P(C, 0, 4), bass, upper, 2);
    assert(scale_harmonization_valid(&f.harmonization));
    assert(!scale_harmonization_valid(NULL));
}

static void test_invalid(void) {
    const PitchSpelling *bass[] = {P(C, 0, 3), P(D, 0, 3)};
    const PitchSpelling *upper[] = {P(A, 0, 3), P(B, 0, 3)};
    ScaleFixture f;
    pair_fixture(&f, SCALE_DIRECTION_ASCENDING, P(C, 0, 4), bass, upper, 2);

    ScaleHarmonization no_stages = f.harmonization;
    no_stages.stage_count = 0;
    assert(!scale_harmonization_valid(&no_stages));

    ScaleHarmonization too_many = f.harmonization;
    too_many.stage_count = SCALE_MAX_STAGES + 1;
    assert(!scale_harmonization_valid(&too_many));

    ScaleHarmonization one_voice = f.harmonization;
    one_voice.voice_count = 1;
    assert(!scale_harmonization_valid(&one_voice));

    ScaleHarmonization many_voices = f.harmonization;
    many_voices.voice_count = SCALE_MAX_VOICES + 1;
    assert(!scale_harmonization_valid(&many_voices));

    ScaleHarmonization bad_direction = f.harmonization;
    bad_direction.direction = SCALE_DIRECTION_COUNT;
    assert(!scale_harmonization_valid(&bad_direction));

    ScaleHarmonization no_tonic = f.harmonization;
    no_tonic.tonic = NULL;
    assert(!scale_harmonization_valid(&no_tonic));

    ScaleHarmonization no_stage_array = f.harmonization;
    no_stage_array.stages = NULL;
    assert(!scale_harmonization_valid(&no_stage_array));

    f.stages[1].pitches[1] = NULL;
    assert(!scale_harmonization_valid(&f.harmonization));
    f.stages[1].pitches[1] = P(B, 0, 3);
    f.stages[0].pitches[0] = NULL;
    assert(!scale_harmonization_valid(&f.harmonization));
}

static void test_interval_above_bass(void) {
    const PitchSpelling *c3 = P(C, 0, 3);
    const PitchSpelling *bass[] = {c3, c3, c3, c3, c3, c3};
    const PitchSpelling *upper[] = {P(E, 0, 4), P(G, 0, 3), P(C, 0, 4), P(C, 0, 3), P(C, 0, 5), P(D, 0, 3)};
    ScaleFixture f;
    pair_fixture(&f, SCALE_DIRECTION_ASCENDING, P(C, 0, 4), bass, upper, 6);

    const int expected[6] = {3, 5, 8, 1, 8, 2};
    for (size_t i = 0; i < 6; i++) {
        int interval = 0;
        assert(scale_interval_above_bass(&f.harmonization, i, 1, &interval));
        assert(interval == expected[i]);
    }
}

static void test_interval_above_bass_arguments(void) {
    const PitchSpelling *bass[] = {P(C, 0, 3), P(C, 0, 3)};
    const PitchSpelling *upper[] = {P(E, 0, 3), P(B, 0, 2)};
    ScaleFixture f;
    pair_fixture(&f, SCALE_DIRECTION_ASCENDING, P(C, 0, 4), bass, upper, 2);

    int interval = 99;
    assert(scale_interval_above_bass(&f.harmonization, 0, 1, &interval) && interval == 3);
    interval = 99;
    /* An upper voice below the bass. */
    assert(!scale_interval_above_bass(&f.harmonization, 1, 1, &interval));
    /* The bass is not an upper voice, and a voice or stage out of range. */
    assert(!scale_interval_above_bass(&f.harmonization, 0, 0, &interval));
    assert(!scale_interval_above_bass(&f.harmonization, 0, 2, &interval));
    assert(!scale_interval_above_bass(&f.harmonization, 2, 1, &interval));
    assert(!scale_interval_above_bass(&f.harmonization, 0, 1, NULL));
    assert(!scale_interval_above_bass(NULL, 0, 1, &interval));
    assert(interval == 99);
}

int main(void) {
    test_valid();
    test_invalid();
    test_interval_above_bass();
    test_interval_above_bass_arguments();
    printf("test_scale_harmonization: ok\n");
    return 0;
}
