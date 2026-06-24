#include <math.h>
#include <stdlib.h>

#include "analysis_cadence_readings.h"
#include "analysis_test_util.h"

/* The evidence for two competing cadence readings: exact shares from the default weights, clamping, config
   validation. Quarter-note segments in 4/4: index n is beat n % 4 + 1 of measure n / 4 + 1. */

typedef struct {
    HarmonySegment *segments;
    HarmonyRun *runs;
    size_t run_count;
    size_t count;
} Prepared;

static Prepared prepare(Fixture *f) {
    Prepared p;
    p.count = f->count;
    assert(harmony_analysis_prepare(f->inputs, f->count, &p.segments) == HARMONY_ANALYSIS_OK);
    p.runs = (HarmonyRun *)calloc(f->count, sizeof(HarmonyRun));
    assert(p.runs != NULL);
    p.run_count = harmony_analysis_build_runs(p.segments, f->count, p.runs);
    return p;
}

static void release(Prepared *p, Fixture *f) {
    free(p->runs);
    harmony_analysis_destroy(p->segments);
    fixture_destroy(f);
}

static int evaded(Prepared *p, size_t dominant_run, const HarmonyCadenceConfig *config) {
    return harmony_cadence_evaded_share(p->segments, p->count, p->runs, p->run_count, dominant_run, config);
}

static int half(Prepared *p, size_t dominant_run, const HarmonyCadenceConfig *config) {
    return harmony_cadence_boundary_half_share(p->segments, p->count, p->runs, p->run_count, dominant_run, config);
}

static void test_default_weights(void) {
    HarmonyCadenceConfig config = harmony_cadence_config_default();
    assert(config.reading_strong_arrival == 150 && config.reading_cadential_approach == 150 &&
           config.reading_followup_cadence == 150 && config.reading_tonic_continues == 100 &&
           config.reading_long_dominant == 150 && config.reading_break_after_dominant == 150 &&
           config.reading_pac_shape == 150 && config.reading_short_dominant == 100 &&
           config.reading_followup_beats == 8.0 && config.reading_long_dominant_beats == 4.0);
}

static void test_evaded_share_strong_arrival_after_a_predominant(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV, beat 1 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 2 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 3 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 4 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 1, 3, 3);   /* I6, beat 1 of measure 2: strong */
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV */
    Prepared p = prepare(&f);
    assert(p.run_count == 4);
    assert(evaded(&p, 1, NULL) == 800); /* 500 + 150 strong arrival + 150 predominant, no follow-up, no tonic after */
    release(&p, &f);
}

static void test_evaded_share_weak_approach_and_tonic_continues(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);   /* I, beat 1 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 2 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 1, 3, 3);   /* I6, beat 3: medium */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I, beat 4: the tonic goes on */
    Prepared p = prepare(&f);
    assert(evaded(&p, 1, NULL) == 100); /* 500 - 150 not strong - 150 no approach - 100 tonic continues */
    release(&p, &f);
}

static void test_evaded_share_with_a_cadential_six_four(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);   /* I, beat 1 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);   /* I, beat 2 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 2, 5, 3);   /* I6/4, beat 3: the cadential 6/4 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 2);   /* V, beat 4 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 1, 3, 3);   /* I6, beat 1: strong */
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    Prepared p = prepare(&f);
    assert(p.run_count == 5);
    /* no predominant before the 6/4, but the 6/4 is the approach: 500 + 150 strong + 150 approach */
    assert(evaded(&p, 2, NULL) == 800);
    release(&p, &f);
}

static void test_evaded_share_follow_up_cadence(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV, beat 1 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 2 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 1, 3, 3);   /* I6, beat 3: medium */
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV, beat 4 */
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV, beat 1 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 2 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I, beat 3: a PAC 4 beats after the I6 */
    Prepared p = prepare(&f);
    assert(evaded(&p, 1, NULL) == 650); /* 500 - 150 medium + 150 predominant + 150 follow-up */

    HarmonyCadenceConfig near = harmony_cadence_config_default();
    near.reading_followup_beats = 3.0; /* the PAC is 4 beats away: out of reach */
    assert(evaded(&p, 1, &near) == 500);
    release(&p, &f);
}

static void test_evaded_share_is_clamped(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 1, 3, 3);   /* beat 3 */
    Prepared p = prepare(&f);
    HarmonyCadenceConfig low = harmony_cadence_config_default();
    low.reading_strong_arrival = 600;
    assert(evaded(&p, 1, &low) == 50);
    release(&p, &f);

    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 1, 3, 3);   /* strong */
    p = prepare(&f);
    HarmonyCadenceConfig high = harmony_cadence_config_default();
    high.reading_strong_arrival = 600;
    assert(evaded(&p, 1, &high) == 950);
    release(&p, &f);
}

static void test_evaded_share_needs_the_shape(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I in root position: not an I6 */
    Prepared p = prepare(&f);
    assert(evaded(&p, 1, NULL) == 0);   /* V to I root */
    assert(evaded(&p, 0, NULL) == 0);   /* not a V */
    assert(evaded(&p, 2, NULL) == 0);   /* no run after */
    assert(harmony_cadence_evaded_share(NULL, 0, NULL, 0, 0, NULL) == 0);
    release(&p, &f);
}

static void test_half_share_long_dominant_with_a_soprano_rest(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, 4 beats from the downbeat */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 0);   /* the soprano rests at the end of the V */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);   /* the new section: soprano 3 */
    Prepared p = prepare(&f);
    assert(half(&p, 2, NULL) == 800); /* 500 + 150 long dominant + 150 soprano rest */
    release(&p, &f);
}

static void test_half_share_short_dominant_into_a_pac_shape(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, one beat */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I on the downbeat with the soprano on 1 */
    Prepared p = prepare(&f);
    assert(half(&p, 2, NULL) == 250); /* 500 - 150 PAC shape - 100 short dominant */
    release(&p, &f);
}

static void test_half_share_balanced_is_500(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, two beats */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);   /* I on the downbeat, soprano 3: no PAC shape */
    Prepared p = prepare(&f);
    assert(half(&p, 2, NULL) == 500);
    release(&p, &f);
}

static void test_half_share_is_clamped_and_needs_the_shape(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);
    Prepared p = prepare(&f);
    HarmonyCadenceConfig low = harmony_cadence_config_default();
    low.reading_short_dominant = 900;
    assert(half(&p, 2, &low) == 50);
    HarmonyCadenceConfig high = harmony_cadence_config_default();
    high.reading_long_dominant_beats = 0.0;
    high.reading_long_dominant = 900;
    assert(half(&p, 2, &high) == 950);
    assert(half(&p, 1, NULL) == 0);     /* not a V */
    assert(half(&p, 3, NULL) == 0);     /* nothing after */
    assert(harmony_cadence_boundary_half_share(NULL, 0, NULL, 0, 0, NULL) == 0);
    release(&p, &f);
}

static void test_config_validation(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);
    HarmonySegment *segments = NULL;
    assert(harmony_analysis_prepare(f.inputs, f.count, &segments) == HARMONY_ANALYSIS_OK);
    HarmonyCadences found;
    HarmonyCadenceConfig config;
    int *weights[8];
    for (int i = 0; i < 8; i++) {
        config = harmony_cadence_config_default();
        weights[0] = &config.reading_strong_arrival;
        weights[1] = &config.reading_cadential_approach;
        weights[2] = &config.reading_followup_cadence;
        weights[3] = &config.reading_tonic_continues;
        weights[4] = &config.reading_long_dominant;
        weights[5] = &config.reading_break_after_dominant;
        weights[6] = &config.reading_pac_shape;
        weights[7] = &config.reading_short_dominant;
        *weights[i] = -1;
        assert(harmony_analysis_find_cadences(segments, f.count, &config, &found) ==
               HARMONY_CADENCE_ANALYSIS_ERR_BAD_CONFIG);
    }
    config = harmony_cadence_config_default();
    config.reading_followup_beats = -1.0;
    assert(harmony_analysis_find_cadences(segments, f.count, &config, &found) == HARMONY_CADENCE_ANALYSIS_ERR_BAD_CONFIG);
    config = harmony_cadence_config_default();
    config.reading_long_dominant_beats = -0.5;
    assert(harmony_analysis_find_cadences(segments, f.count, &config, &found) == HARMONY_CADENCE_ANALYSIS_ERR_BAD_CONFIG);
    config = harmony_cadence_config_default();
    config.reading_followup_beats = NAN;
    assert(harmony_analysis_find_cadences(segments, f.count, &config, &found) == HARMONY_CADENCE_ANALYSIS_ERR_BAD_CONFIG);
    config = harmony_cadence_config_default();
    config.reading_strong_arrival = 0; /* zero is a valid weight */
    assert(harmony_analysis_find_cadences(segments, f.count, &config, &found) == HARMONY_CADENCE_ANALYSIS_OK);
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

int main(void) {
    test_default_weights();
    test_evaded_share_strong_arrival_after_a_predominant();
    test_evaded_share_weak_approach_and_tonic_continues();
    test_evaded_share_with_a_cadential_six_four();
    test_evaded_share_follow_up_cadence();
    test_evaded_share_is_clamped();
    test_evaded_share_needs_the_shape();
    test_half_share_long_dominant_with_a_soprano_rest();
    test_half_share_short_dominant_into_a_pac_shape();
    test_half_share_balanced_is_500();
    test_half_share_is_clamped_and_needs_the_shape();
    test_config_validation();
    printf("standard-harmony cadence readings: all tests passed\n");
    return 0;
}
