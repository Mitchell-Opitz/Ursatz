#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interpretation/interpretation.h"
#include "interpretation/uncertainty.h"
#include "triad_identification.h"
#include "weighted_chord_identification.h"

/*
 * Weighted chord identification result tests: the best and runner-up chord, both scores, the uncertainty
 * (runner-up score over best score), the Interpretation's confidence, no match below the minimum, and the
 * two-pass sequence with leftover tones labeled. Key of C major unless stated.
 */

static bool close_to(double a, double b) {
    return fabs(a - b) < 1e-9;
}

static CommonPracticeMember note(const char *id, int pitch, bool held, double duration, BeatStrength strength,
                                 const char *voice, double onset) {
    CommonPracticeMember m;
    memset(&m, 0, sizeof(m));
    m.note_id = id;
    m.pitch = pitch;
    m.held = held;
    m.duration = duration;
    m.strength = strength;
    m.voice_id = voice;
    m.onset = onset;
    return m;
}

#define S BEAT_STRENGTH_STRONG
#define M BEAT_STRENGTH_MEDIUM
#define W BEAT_STRENGTH_WEAK

static CommonPracticeSegment segment(const CommonPracticeMember *members, size_t count, const char *id) {
    CommonPracticeSegment s;
    s.members = members;
    s.member_count = count;
    s.id = id;
    s.beat = 0;
    return s;
}

static void test_a_pure_triad_resolves_to_its_chord(void) {
    CommonPracticeMember members[3] = {note("c", 48, false, 1.0, S, NULL, 0.0), note("e", 52, false, 1.0, S, NULL, 0.0),
                                       note("g", 55, false, 1.0, S, NULL, 0.0)};
    CommonPracticeSegment seg = segment(members, 3, "sonority-1");
    CommonPracticeWeightedResult result;
    assert(common_practice_identify_weighted(&seg, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &result) ==
           COMMON_PRACTICE_WEIGHTED_OK);

    assert(result.matched);
    assert(result.best.degree == 1 && !result.best.is_seventh && result.best.triad == COMMON_PRACTICE_TRIAD_I);
    assert(close_to(result.best_score, 1.05)); /* coverage 1: everything sounding is a chord tone */
    assert(strcmp(chord_label(result.best_chord), "I") == 0);
    assert(result.runner_up.root_pitch_class != result.best.root_pitch_class); /* a different harmony */
    assert(result.runner_up_chord != NULL);
    assert(close_to(result.uncertainty, result.runner_up_score / result.best_score));
    assert(result.uncertainty >= 0.0 && result.uncertainty < 1.0);
    assert(result.leftover_count == 0 && result.leftover == NULL);

    const Interpretation *interpretation = chord_interpretation(result.best_chord);
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation)), "sonority-1") == 0);
    common_practice_weighted_result_destroy(&result);
}

static void test_the_interpretation_carries_the_confidence(void) {
    CommonPracticeMember members[3] = {note("c", 48, false, 1.0, S, NULL, 0.0), note("e", 52, false, 1.0, S, NULL, 0.0),
                                       note("g", 55, false, 1.0, S, NULL, 0.0)};
    CommonPracticeSegment seg = segment(members, 3, "sonority-1");
    CommonPracticeWeightedResult result;
    assert(common_practice_identify_weighted(&seg, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &result) ==
           COMMON_PRACTICE_WEIGHTED_OK);

    const Uncertainty *best = interpretation_uncertainty(chord_interpretation(result.best_chord));
    const Uncertainty *runner = interpretation_uncertainty(chord_interpretation(result.runner_up_chord));
    assert(uncertainty_kind(best) == UNCERTAINTY_KIND_SCALAR_CONFIDENCE);
    assert(uncertainty_kind(runner) == UNCERTAINTY_KIND_SCALAR_CONFIDENCE);
    assert(uncertainty_scalar_confidence_denominator(best) == 1000000UL);
    double best_confidence = (double)uncertainty_scalar_confidence_numerator(best) / 1000000.0;
    double runner_confidence = (double)uncertainty_scalar_confidence_numerator(runner) / 1000000.0;
    assert(fabs(best_confidence - (1.0 - result.uncertainty)) < 1e-6); /* confidence = 1 - uncertainty */
    assert(fabs(runner_confidence - result.uncertainty) < 1e-6);       /* the runner-up's plausibility */

    /* The two Chords over one Sonority have distinct Interpretation ids. */
    assert(!entity_id_equals(interpretation_id(chord_interpretation(result.best_chord)),
                             interpretation_id(chord_interpretation(result.runner_up_chord))));
    common_practice_weighted_result_destroy(&result);
}

static void test_a_near_tie_reports_the_runner_up_and_a_high_uncertainty(void) {
    /* C3 bass, E, G and a little A (the default bass weight is 1.1): vi7 (A C E G) covers everything; I (C E G)
       all but the A. The scores
       are close, so the uncertainty is high, and the runner-up is I (a different root from vi7's A). */
    CommonPracticeMember members[4] = {note("c", 48, false, 1.0, S, NULL, 0.0), note("e", 52, false, 1.0, S, NULL, 0.0),
                                       note("g", 55, false, 1.0, S, NULL, 0.0),
                                       note("a", 57, false, 0.25, W, NULL, 0.0)};
    CommonPracticeSegment seg = segment(members, 4, "sonority-2");
    CommonPracticeWeightedResult result;
    assert(common_practice_identify_weighted(&seg, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &result) ==
           COMMON_PRACTICE_WEIGHTED_OK);

    assert(result.best.degree == 6 && result.best.is_seventh); /* vi7: A C E G */
    assert(result.runner_up.degree == 1);                      /* I */
    assert(close_to(result.best_score, 1.05));
    assert(close_to(result.runner_up_score, 3.1 / 3.2 + 0.05));
    assert(close_to(result.uncertainty, (3.1 / 3.2 + 0.05) / 1.05));
    assert(result.uncertainty > 0.9 && result.uncertainty < 1.0);
    common_practice_weighted_result_destroy(&result);
}

static void test_below_the_minimum_coverage_is_no_match(void) {
    /* A chromatic cluster no diatonic chord covers. */
    CommonPracticeMember members[3] = {note("a", 61, false, 1.0, S, NULL, 0.0), note("b", 63, false, 1.0, S, NULL, 0.0),
                                       note("c", 66, false, 1.0, S, NULL, 0.0)};
    CommonPracticeSegment seg = segment(members, 3, "sonority-3");
    CommonPracticeWeightedResult result;
    assert(common_practice_identify_weighted(&seg, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &result) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(!result.matched);
    assert(result.best_chord == NULL && result.runner_up_chord == NULL && result.leftover_count == 0);
    assert(result.best_score < 0.5); /* the scores are still reported */
    common_practice_weighted_result_destroy(&result);

    /* The minimum is configurable: a triad with half its weight on a foreign note falls under 0.99 but
       clears 0.5. */
    CommonPracticeMember mixed[4] = {note("c", 48, false, 1.0, S, NULL, 0.0), note("e", 52, false, 1.0, S, NULL, 0.0),
                                     note("g", 55, false, 1.0, S, NULL, 0.0), note("f#", 66, false, 1.0, S, NULL, 0.0)};
    CommonPracticeSegment mixed_segment = segment(mixed, 4, "sonority-4");
    assert(common_practice_identify_weighted(&mixed_segment, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &result) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(result.matched);
    common_practice_weighted_result_destroy(&result);

    CommonPracticeWeightedConfig strict = common_practice_weighted_config_default();
    strict.min_coverage = 0.99;
    assert(common_practice_identify_weighted(&mixed_segment, 0, COMMON_PRACTICE_KEY_MAJOR, &strict, &result) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(!result.matched);
    common_practice_weighted_result_destroy(&result);
}

static void test_an_empty_segment_is_no_match(void) {
    CommonPracticeSegment seg = segment(NULL, 0, "sonority-5");
    CommonPracticeWeightedResult result;
    assert(common_practice_identify_weighted(&seg, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &result) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(!result.matched && result.best_chord == NULL && close_to(result.uncertainty, 1.0));
    common_practice_weighted_result_destroy(&result);
}

static void test_two_passing_tones_on_a_weak_beat_still_pick_the_triad(void) {
    /* C major in five voices over three segments: the alto passes E F G and the soprano G F E, so the
       middle (weak) segment has two passing tones (F4 and F5) among the held bass, tenor and inner voice. */
    CommonPracticeMember m0[5] = {note("b", 48, false, 0.25, S, "b", 0.0), note("e3", 52, false, 0.25, S, "e", 0.0),
                                  note("t", 55, false, 0.25, S, "t", 0.0), note("a0", 64, false, 0.25, S, "a", 0.0),
                                  note("s0", 79, false, 0.25, S, "s", 0.0)};
    CommonPracticeMember m1[5] = {note("b", 48, true, 0.25, S, "b", 0.0), note("e3", 52, true, 0.25, S, "e", 0.0),
                                  note("t", 55, true, 0.25, S, "t", 0.0), note("a1", 65, false, 0.25, W, "a", 0.25),
                                  note("s1", 77, false, 0.25, W, "s", 0.25)};
    CommonPracticeMember m2[5] = {note("b", 48, true, 0.25, S, "b", 0.0), note("e3", 52, true, 0.25, S, "e", 0.0),
                                  note("t", 55, true, 0.25, S, "t", 0.0), note("a2", 67, false, 0.25, S, "a", 0.5),
                                  note("s2", 76, false, 0.25, S, "s", 0.5)};
    CommonPracticeSegment segments[3] = {segment(m0, 5, "s0"), segment(m1, 5, "s1"), segment(m2, 5, "s2")};

    CommonPracticeWeightedResults results = {0};
    assert(common_practice_identify_weighted_sequence(segments, 3, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &results) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(results.count == 3);
    for (size_t i = 0; i < 3; i++) {
        assert(results.items[i].matched);
        assert(results.items[i].best.degree == 1 && !results.items[i].best.is_seventh); /* I each time */
    }
    assert(results.items[0].leftover_count == 0 && results.items[2].leftover_count == 0);

    const CommonPracticeWeightedResult *middle = &results.items[1];
    assert(middle->leftover_count == 2);
    for (size_t k = 0; k < 2; k++) {
        assert(middle->leftover[k].classified && middle->leftover[k].tone == NON_CHORD_TONE_PASSING);
    }
    assert(strcmp(middle->leftover[0].note_id, "a1") == 0);
    assert(strcmp(middle->leftover[1].note_id, "s1") == 0);
    common_practice_weighted_results_destroy(&results);
}

static void test_a_suspension_4_3_is_labeled_across_the_sequence(void) {
    /* IV (F A C), then C major with the soprano's F4 held over, then the soprano falls to E4. */
    CommonPracticeMember m0[4] = {note("fb", 53, false, 0.5, S, "b", 0.0), note("a", 57, false, 0.5, S, "a", 0.0),
                                  note("c", 60, false, 0.5, S, "c", 0.0), note("f", 65, false, 0.5, S, "s", 0.0)};
    CommonPracticeMember m1[5] = {note("cb", 48, false, 0.5, S, "b", 0.5), note("e3", 52, false, 0.5, S, "a", 0.5),
                                  note("g", 55, false, 0.5, S, "c", 0.5), note("c4", 60, false, 0.5, S, "t", 0.5),
                                  note("f", 65, true, 0.5, S, "s", 0.0)};
    CommonPracticeMember m2[5] = {note("cb", 48, true, 0.5, S, "b", 0.5), note("e3", 52, true, 0.5, S, "a", 0.5),
                                  note("g", 55, true, 0.5, S, "c", 0.5), note("c4", 60, true, 0.5, S, "t", 0.5),
                                  note("e", 64, false, 0.5, M, "s", 1.0)};
    CommonPracticeSegment segments[3] = {segment(m0, 4, "s0"), segment(m1, 5, "s1"), segment(m2, 5, "s2")};

    CommonPracticeWeightedResults results = {0};
    assert(common_practice_identify_weighted_sequence(segments, 3, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &results) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(results.items[0].matched && results.items[0].best.degree == 4); /* IV */
    assert(results.items[1].matched && results.items[1].best.degree == 1); /* I */
    assert(results.items[1].leftover_count == 1);
    assert(results.items[1].leftover[0].classified && results.items[1].leftover[0].tone == NON_CHORD_TONE_SUSPENSION);
    assert(strcmp(results.items[1].leftover[0].note_id, "f") == 0);
    common_practice_weighted_results_destroy(&results);
}

static void test_the_sequence_skips_unmatched_segments_when_labeling(void) {
    CommonPracticeMember cluster[2] = {note("x", 61, false, 1.0, S, "s", 0.0), note("y", 63, false, 1.0, S, "a", 0.0)};
    CommonPracticeSegment segments[2] = {segment(cluster, 2, "s0"), segment(NULL, 0, "s1")};
    CommonPracticeWeightedResults results = {0};
    assert(common_practice_identify_weighted_sequence(segments, 2, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &results) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(results.count == 2 && !results.items[0].matched && !results.items[1].matched);
    assert(results.items[0].leftover_count == 0);
    common_practice_weighted_results_destroy(&results);

    CommonPracticeWeightedResults none = {0};
    assert(common_practice_identify_weighted_sequence(NULL, 0, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &none) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(none.count == 0 && none.items == NULL);
}

static void test_rejects_bad_input_and_an_unknown_mode(void) {
    CommonPracticeMember members[1] = {note("c", 60, false, 1.0, S, NULL, 0.0)};
    CommonPracticeSegment seg = segment(members, 1, "s0");
    CommonPracticeWeightedResult result;
    CommonPracticeWeightedResults results = {NULL, 7};
    assert(common_practice_identify_weighted(NULL, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &result) ==
           COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT);
    assert(common_practice_identify_weighted(&seg, 0, (CommonPracticeKeyMode)99, NULL, &result) ==
           COMMON_PRACTICE_WEIGHTED_ERR_UNSUPPORTED_MODE);
    assert(common_practice_identify_weighted(&seg, 12, COMMON_PRACTICE_KEY_MAJOR, NULL, &result) ==
           COMMON_PRACTICE_WEIGHTED_ERR_BAD_TONIC);
    assert(common_practice_identify_weighted_sequence(&seg, 1, 0, (CommonPracticeKeyMode)99, NULL, &results) ==
           COMMON_PRACTICE_WEIGHTED_ERR_UNSUPPORTED_MODE);
    assert(results.items == NULL && results.count == 7); /* untouched */

    CommonPracticeSegment no_id = segment(members, 1, NULL);
    assert(common_practice_identify_weighted(&no_id, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &result) ==
           COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT);
    common_practice_weighted_result_destroy(NULL);
    common_practice_weighted_results_destroy(NULL);
}

static CommonPracticeSegment in_beat(CommonPracticeSegment s, long beat) {
    s.beat = beat;
    return s;
}

static void test_an_incomplete_segment_takes_the_previous_chord(void) {
    /* I (C E G), then G3 under E4 alone: on its own iii (E G B) with two tones and its root above the bass.
       Both its pitch classes are in I, so it takes I, as context from the previous segment. */
    CommonPracticeMember m0[3] = {note("c", 48, false, 1.0, S, NULL, 0.0), note("e", 52, false, 1.0, S, NULL, 0.0),
                                  note("g", 55, false, 1.0, S, NULL, 0.0)};
    CommonPracticeMember m1[2] = {note("g1", 55, false, 0.5, W, NULL, 1.0), note("e1", 64, false, 0.5, W, NULL, 1.0)};
    CommonPracticeSegment segments[2] = {segment(m0, 3, "s0"), segment(m1, 2, "s1")};
    CommonPracticeWeightedResults results = {0};
    assert(common_practice_identify_weighted_sequence(segments, 2, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &results) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(results.items[0].context == COMMON_PRACTICE_CONTEXT_NONE && results.items[0].best.degree == 1);
    assert(results.items[1].matched && results.items[1].best.degree == 1 && !results.items[1].best.is_seventh);
    assert(results.items[1].context == COMMON_PRACTICE_CONTEXT_PREVIOUS);
    assert(results.items[1].best_chord != NULL);
    common_practice_weighted_results_destroy(&results);

    /* Alone, the same segment is iii. */
    CommonPracticeWeightedResult alone;
    assert(common_practice_identify_weighted(&segments[1], 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &alone) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(alone.best.degree == 3 && alone.context == COMMON_PRACTICE_CONTEXT_NONE);
    common_practice_weighted_result_destroy(&alone);
}

static void test_an_incomplete_first_segment_takes_the_next_chord(void) {
    CommonPracticeMember m0[2] = {note("g0", 55, false, 0.5, S, NULL, 0.0), note("e0", 64, false, 0.5, S, NULL, 0.0)};
    CommonPracticeMember m1[3] = {note("c", 48, false, 1.0, S, NULL, 0.5), note("e", 52, false, 1.0, S, NULL, 0.5),
                                  note("g", 55, false, 1.0, S, NULL, 0.5)};
    CommonPracticeSegment segments[2] = {segment(m0, 2, "s0"), segment(m1, 3, "s1")};
    CommonPracticeWeightedResults results = {0};
    assert(common_practice_identify_weighted_sequence(segments, 2, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &results) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(results.items[0].best.degree == 1 && results.items[0].context == COMMON_PRACTICE_CONTEXT_NEXT);
    common_practice_weighted_results_destroy(&results);
}

static void test_a_root_in_the_bass_keeps_its_own_reading(void) {
    /* vi (A C E), then C3 under E4: two tones of I with its root in the bass, a triad without its fifth. It is
       not incomplete, so it stays I rather than taking vi. */
    CommonPracticeMember m0[3] = {note("a", 57, false, 1.0, S, NULL, 0.0), note("c", 60, false, 1.0, S, NULL, 0.0),
                                  note("e", 64, false, 1.0, S, NULL, 0.0)};
    CommonPracticeMember m1[2] = {note("c1", 48, false, 1.0, S, NULL, 1.0), note("e1", 64, false, 1.0, S, NULL, 1.0)};
    CommonPracticeSegment segments[2] = {segment(m0, 3, "s0"), segment(m1, 2, "s1")};
    CommonPracticeWeightedResults results = {0};
    assert(common_practice_identify_weighted_sequence(segments, 2, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &results) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(results.items[0].best.degree == 6);
    assert(results.items[1].best.degree == 1 && results.items[1].context == COMMON_PRACTICE_CONTEXT_NONE);
    common_practice_weighted_results_destroy(&results);
}

static void test_an_arpeggio_inside_one_beat_is_one_chord(void) {
    /* E4 alone, then G4 and C5, both inside beat 5: neither is complete, their union is I (C E G). */
    CommonPracticeMember m0[1] = {note("e", 64, false, 0.125, S, NULL, 0.0)};
    CommonPracticeMember m1[2] = {note("g", 67, false, 0.125, W, NULL, 0.125), note("c", 72, false, 0.125, W, NULL, 0.125)};
    CommonPracticeSegment segments[2] = {in_beat(segment(m0, 1, "s0"), 5), in_beat(segment(m1, 2, "s1"), 5)};
    CommonPracticeWeightedResults results = {0};
    assert(common_practice_identify_weighted_sequence(segments, 2, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &results) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    for (size_t i = 0; i < 2; i++) {
        assert(results.items[i].matched && results.items[i].best.degree == 1 && !results.items[i].best.is_seventh);
        assert(results.items[i].context == COMMON_PRACTICE_CONTEXT_ARPEGGIO);
    }
    common_practice_weighted_results_destroy(&results);

    /* In different beats there is no arpeggio: the E stays its own reading. */
    segments[1].beat = 6;
    assert(common_practice_identify_weighted_sequence(segments, 2, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &results) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(results.items[0].context != COMMON_PRACTICE_CONTEXT_ARPEGGIO);
    common_practice_weighted_results_destroy(&results);
}

static void test_the_existing_subset_match_functions_are_unchanged(void) {
    /* The weighted path sits beside them: the degrees {1, 3, 5, 6} still resolve to the first match, I. */
    int degrees[4] = {1, 3, 5, 6};
    CommonPracticeTriad triad;
    assert(common_practice_identify_triad(degrees, 4, &triad));
    assert(triad == COMMON_PRACTICE_TRIAD_I);
}

int main(void) {
    test_a_pure_triad_resolves_to_its_chord();
    test_the_interpretation_carries_the_confidence();
    test_a_near_tie_reports_the_runner_up_and_a_high_uncertainty();
    test_below_the_minimum_coverage_is_no_match();
    test_an_empty_segment_is_no_match();
    test_two_passing_tones_on_a_weak_beat_still_pick_the_triad();
    test_a_suspension_4_3_is_labeled_across_the_sequence();
    test_the_sequence_skips_unmatched_segments_when_labeling();
    test_rejects_bad_input_and_an_unknown_mode();
    test_an_incomplete_segment_takes_the_previous_chord();
    test_an_incomplete_first_segment_takes_the_next_chord();
    test_a_root_in_the_bass_keeps_its_own_reading();
    test_an_arpeggio_inside_one_beat_is_one_chord();
    test_the_existing_subset_match_functions_are_unchanged();
    printf("test_weighted_chord_identification: ok\n");
    return 0;
}
