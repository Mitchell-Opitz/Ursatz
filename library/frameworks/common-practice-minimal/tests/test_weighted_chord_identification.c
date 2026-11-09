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
    s.has_bass = false;
    s.bass_pitch = 0;
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

static bool same_ranked(const CommonPracticeRankedChord *a, const CommonPracticeRankedChord *b) {
    return a->degree == b->degree && a->is_seventh == b->is_seventh && a->triad == b->triad &&
           a->seventh == b->seventh && a->root_pitch_class == b->root_pitch_class && a->score == b->score &&
           a->tonic_offset == b->tonic_offset && a->form_rank == b->form_rank;
}

/* G2 held under D5 F#5 A5, in G major: without the bass requirement the upper voices make V (D F# A) the best
   score; with the bass given as G2 and required in the chord, only chords holding G are eligible and the G chord
   wins. */
static void test_a_required_bass_chord_tone_beats_the_upper_voices(void) {
    CommonPracticeMember members[4] = {note("g2", 43, true, 0.25, S, NULL, 0.0), note("d5", 74, false, 1.0, S, NULL, 0.0),
                                       note("fs5", 78, false, 1.0, S, NULL, 0.0),
                                       note("a5", 81, false, 1.0, S, NULL, 0.0)};
    CommonPracticeSegment seg = segment(members, 4, "sonority-bass");
    seg.has_bass = true;
    seg.bass_pitch = 43;
    CommonPracticeWeightedResult result;

    /* Without the flag: today's result (V, D F# A), also what the same segment gives with no override. */
    CommonPracticeWeightedResult plain;
    CommonPracticeSegment unset = segment(members, 4, "sonority-bass");
    assert(common_practice_identify_weighted(&seg, 7, COMMON_PRACTICE_KEY_MAJOR, NULL, &result) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(common_practice_identify_weighted(&unset, 7, COMMON_PRACTICE_KEY_MAJOR, NULL, &plain) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(result.matched && result.best.degree == 5 && result.best.root_pitch_class == 2); /* D */
    assert(same_ranked(&result.best, &plain.best));
    assert(same_ranked(&result.runner_up, &plain.runner_up));
    assert(result.best_score == plain.best_score && result.runner_up_score == plain.runner_up_score &&
           result.uncertainty == plain.uncertainty);
    common_practice_weighted_result_destroy(&result);
    common_practice_weighted_result_destroy(&plain);

    /* With it: G, not D. */
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    config.require_bass_chord_tone = true;
    assert(common_practice_identify_weighted(&seg, 7, COMMON_PRACTICE_KEY_MAJOR, &config, &result) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(result.matched && result.best.degree == 1 && result.best.root_pitch_class == 7); /* G */
    common_practice_weighted_result_destroy(&result);
}

/* A zero-initialized segment (has_bass false) reads exactly as one whose given bass is its lowest pitch, with the
   flag off; with the flag on, a root-position triad keeps its best chord. */
static void test_a_segment_without_a_bass_override_reads_as_before(void) {
    CommonPracticeMember members[3] = {note("c", 48, false, 1.0, S, NULL, 0.0), note("e", 52, false, 1.0, S, NULL, 0.0),
                                       note("g", 55, false, 1.0, S, NULL, 0.0)};
    CommonPracticeSegment seg = segment(members, 3, "sonority-1");
    CommonPracticeSegment given = seg;
    given.has_bass = true;
    given.bass_pitch = 48;
    CommonPracticeWeightedResult a;
    CommonPracticeWeightedResult b;
    assert(common_practice_identify_weighted(&seg, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &a) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(common_practice_identify_weighted(&given, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &b) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(same_ranked(&a.best, &b.best) && same_ranked(&a.runner_up, &b.runner_up) && a.best_score == b.best_score &&
           a.runner_up_score == b.runner_up_score && a.uncertainty == b.uncertainty);
    common_practice_weighted_result_destroy(&b);

    CommonPracticeWeightedConfig flagged = common_practice_weighted_config_default();
    flagged.require_bass_chord_tone = true;
    assert(common_practice_identify_weighted(&seg, 0, COMMON_PRACTICE_KEY_MAJOR, &flagged, &b) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(same_ranked(&a.best, &b.best) && a.best_score == b.best_score);
    common_practice_weighted_result_destroy(&a);
    common_practice_weighted_result_destroy(&b);
}

/* --- applied chords (R-380) --- */

static CommonPracticeWeightedConfig applied_config(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    config.applied_chords = true;
    return config;
}

static void assert_applied(const CommonPracticeWeightedResult *result, int root_pitch_class, bool is_seventh, int target) {
    assert(result->matched);
    assert(result->best.root_pitch_class == root_pitch_class && result->best.is_seventh == is_seventh &&
           result->best.applied_target_degree == target);
}

static void test_an_applied_dominant_seventh_is_ranked_when_asked(void) {
    /* D3 F#4 A4 C5 quarters on a strong beat in C major: V7/V. */
    CommonPracticeMember members[4] = {note("d", 50, false, 0.25, S, NULL, 0.0), note("f", 66, false, 0.25, S, NULL, 0.0),
                                       note("a", 69, false, 0.25, S, NULL, 0.0), note("c", 72, false, 0.25, S, NULL, 0.0)};
    CommonPracticeSegment seg = segment(members, 4, "sonority-1");
    CommonPracticeWeightedConfig config = applied_config();
    CommonPracticeWeightedResult result;
    assert(common_practice_identify_weighted(&seg, 0, COMMON_PRACTICE_KEY_MAJOR, &config, &result) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert_applied(&result, 2, true, 5);
    assert(result.best.seventh == COMMON_PRACTICE_SEVENTH_DOMINANT7);
    common_practice_weighted_result_destroy(&result);

    /* The same with the flag off is today's result: a diatonic chord. */
    CommonPracticeWeightedResult plain;
    assert(common_practice_identify_weighted(&seg, 0, COMMON_PRACTICE_KEY_MAJOR, NULL, &plain) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(plain.best.applied_target_degree == 0);
    common_practice_weighted_result_destroy(&plain);
}

static void test_clementi_m6_beat_4_reads_as_vii_dim_of_v(void) {
    /* F#3 quarter on a weak beat, E5 then C5 eighths, bass F#3 required: F# diminished, target V. */
    CommonPracticeMember members[3] = {note("f", 54, false, 0.25, W, NULL, 0.0), note("e", 76, false, 0.125, W, NULL, 0.0),
                                       note("c", 72, false, 0.125, W, NULL, 0.125)};
    CommonPracticeSegment seg = segment(members, 3, "sonority-1");
    seg.has_bass = true;
    seg.bass_pitch = 54;
    CommonPracticeWeightedConfig config = applied_config();
    config.require_bass_chord_tone = true;
    CommonPracticeWeightedResult result;
    assert(common_practice_identify_weighted(&seg, 0, COMMON_PRACTICE_KEY_MAJOR, &config, &result) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert_applied(&result, 6, false, 5);
    assert(result.best.triad == COMMON_PRACTICE_TRIAD_DIMINISHED);
    common_practice_weighted_result_destroy(&result);
}

static void test_a_chromatic_neighbour_does_not_make_an_applied_chord(void) {
    /* C3 half note, E4 quarter, F4 and F#4 eighths on weak positions, G4 quarter: I. */
    CommonPracticeMember members[5] = {note("c", 48, false, 0.5, S, NULL, 0.0), note("e", 64, false, 0.25, M, NULL, 0.0),
                                       note("f", 65, false, 0.125, W, NULL, 0.25), note("fs", 66, false, 0.125, W, NULL, 0.375),
                                       note("g", 67, false, 0.25, M, NULL, 0.5)};
    CommonPracticeSegment seg = segment(members, 5, "sonority-1");
    seg.has_bass = true;
    seg.bass_pitch = 48;
    CommonPracticeWeightedConfig config = applied_config();
    config.require_bass_chord_tone = true;
    CommonPracticeWeightedResult result;
    assert(common_practice_identify_weighted(&seg, 0, COMMON_PRACTICE_KEY_MAJOR, &config, &result) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(result.matched && result.best.degree == 1 && !result.best.is_seventh && result.best.applied_target_degree == 0);
    common_practice_weighted_result_destroy(&result);
}

static void test_an_applied_dominant_seventh_that_misses_the_bass_is_not_read(void) {
    /* F3 held, D4 F4 A4, a weak C#5 sixteenth, bass F3 required: D minor (V7/ii, A C# E G, does not hold the bass). */
    CommonPracticeMember members[5] = {note("f3", 53, true, 0.25, S, NULL, -0.25), note("d", 62, false, 0.25, S, NULL, 0.0),
                                       note("f", 65, false, 0.25, S, NULL, 0.0), note("a", 69, false, 0.25, S, NULL, 0.0),
                                       note("cs", 73, false, 0.0625, W, NULL, 0.1875)};
    CommonPracticeSegment seg = segment(members, 5, "sonority-1");
    seg.has_bass = true;
    seg.bass_pitch = 53;
    CommonPracticeWeightedConfig config = applied_config();
    config.require_bass_chord_tone = true;
    CommonPracticeWeightedResult result;
    assert(common_practice_identify_weighted(&seg, 0, COMMON_PRACTICE_KEY_MAJOR, &config, &result) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(result.matched && result.best.root_pitch_class == 2 && result.best.degree == 2 && !result.best.is_seventh &&
           result.best.applied_target_degree == 0);
    common_practice_weighted_result_destroy(&result);
}

static void test_a_bare_fifth_is_not_an_applied_chord(void) {
    /* D3 A3 alone: ii; V/V is not eligible (no tone outside the key sounds). */
    CommonPracticeMember members[2] = {note("d", 50, false, 0.25, S, NULL, 0.0), note("a", 57, false, 0.25, S, NULL, 0.0)};
    CommonPracticeSegment seg = segment(members, 2, "sonority-1");
    CommonPracticeWeightedConfig config = applied_config();
    CommonPracticeWeightedResult result;
    assert(common_practice_identify_weighted(&seg, 0, COMMON_PRACTICE_KEY_MAJOR, &config, &result) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(result.matched && result.best.degree == 2 && result.best.applied_target_degree == 0);
    assert(result.runner_up.applied_target_degree == 0);
    common_practice_weighted_result_destroy(&result);
}

/* Two segments, the second one chord after the first, read as a sequence. */
static void read_two(CommonPracticeMember *first, size_t first_count, CommonPracticeMember *second, size_t second_count,
                     int tonic, CommonPracticeKeyMode mode, CommonPracticeWeightedResults *out) {
    CommonPracticeSegment segments[2] = {segment(first, first_count, "sonority-1"),
                                         segment(second, second_count, "sonority-2")};
    CommonPracticeWeightedConfig config = applied_config();
    assert(common_practice_identify_weighted_sequence(segments, 2, tonic, mode, &config, out) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(out->count == 2);
}

static void test_a_diminished_seventh_is_rooted_by_where_it_resolves_in_major(void) {
    CommonPracticeMember dim[4] = {note("a", 54, false, 0.25, S, NULL, 0.0), note("b", 57, false, 0.25, S, NULL, 0.0),
                                   note("c", 60, false, 0.25, S, NULL, 0.0), note("d", 63, false, 0.25, S, NULL, 0.0)};
    CommonPracticeMember to_g[3] = {note("e", 55, false, 0.25, S, NULL, 0.25), note("f", 59, false, 0.25, S, NULL, 0.25),
                                    note("g", 62, false, 0.25, S, NULL, 0.25)};
    CommonPracticeMember to_e[3] = {note("h", 52, false, 0.25, S, NULL, 0.25), note("i", 55, false, 0.25, S, NULL, 0.25),
                                    note("j", 59, false, 0.25, S, NULL, 0.25)};
    CommonPracticeWeightedResults results;

    /* F# A C Eb then G B D: F#, target V. */
    read_two(dim, 4, to_g, 3, 0, COMMON_PRACTICE_KEY_MAJOR, &results);
    assert_applied(&results.items[0], 6, true, 5);
    assert(results.items[0].best.degree == 4 && results.items[0].best.tonic_offset == 6);
    assert(results.items[1].best.degree == 5 && results.items[1].best.applied_target_degree == 0);
    common_practice_weighted_results_destroy(&results);

    /* F# A C Eb then E G B: D# (pitch class 3), target iii. */
    read_two(dim, 4, to_e, 3, 0, COMMON_PRACTICE_KEY_MAJOR, &results);
    assert_applied(&results.items[0], 3, true, 3);
    assert(results.items[0].best.degree == 2 && results.items[0].best.tonic_offset == 3);
    assert(results.items[0].best.seventh == COMMON_PRACTICE_SEVENTH_DIMINISHED7);
    assert(results.items[0].best_chord != NULL);
    common_practice_weighted_results_destroy(&results);
}

static void test_a_diminished_seventh_is_rooted_by_where_it_resolves_in_minor(void) {
    /* C minor: E G Bb Db then Ab C Eb: G (pitch class 7), target VI. */
    CommonPracticeMember dim[4] = {note("a", 52, false, 0.25, S, NULL, 0.0), note("b", 55, false, 0.25, S, NULL, 0.0),
                                   note("c", 58, false, 0.25, S, NULL, 0.0), note("d", 61, false, 0.25, S, NULL, 0.0)};
    CommonPracticeMember to_ab[3] = {note("e", 44, false, 0.25, S, NULL, 0.25), note("f", 48, false, 0.25, S, NULL, 0.25),
                                     note("g", 51, false, 0.25, S, NULL, 0.25)};
    CommonPracticeWeightedResults results;
    read_two(dim, 4, to_ab, 3, 0, COMMON_PRACTICE_KEY_MINOR, &results);
    assert_applied(&results.items[0], 7, true, 6);
    assert(results.items[0].best.degree == 5 && results.items[0].best.tonic_offset == 7);
    common_practice_weighted_results_destroy(&results);

    /* The same chord followed by F minor (iv) keeps its own root E, target iv. */
    CommonPracticeMember to_f[3] = {note("h", 41, false, 0.25, S, NULL, 0.25), note("i", 44, false, 0.25, S, NULL, 0.25),
                                    note("j", 48, false, 0.25, S, NULL, 0.25)};
    read_two(dim, 4, to_f, 3, 0, COMMON_PRACTICE_KEY_MINOR, &results);
    assert_applied(&results.items[0], 4, true, 4);
    common_practice_weighted_results_destroy(&results);
}

static void test_applied_dominant_sevenths_in_minor(void) {
    CommonPracticeWeightedConfig config = applied_config();
    CommonPracticeWeightedResult result;

    /* C minor D3 F#3 A3 C4: D7, target V. */
    CommonPracticeMember d7[4] = {note("a", 50, false, 0.25, S, NULL, 0.0), note("b", 54, false, 0.25, S, NULL, 0.0),
                                  note("c", 57, false, 0.25, S, NULL, 0.0), note("d", 60, false, 0.25, S, NULL, 0.0)};
    CommonPracticeSegment seg = segment(d7, 4, "sonority-1");
    assert(common_practice_identify_weighted(&seg, 0, COMMON_PRACTICE_KEY_MINOR, &config, &result) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert_applied(&result, 2, true, 5);
    assert(result.best.seventh == COMMON_PRACTICE_SEVENTH_DOMINANT7);
    common_practice_weighted_result_destroy(&result);

    /* C minor C3 E3 G3 Bb3: C7, target iv. */
    CommonPracticeMember c7[4] = {note("e", 48, false, 0.25, S, NULL, 0.0), note("f", 52, false, 0.25, S, NULL, 0.0),
                                  note("g", 55, false, 0.25, S, NULL, 0.0), note("h", 58, false, 0.25, S, NULL, 0.0)};
    seg = segment(c7, 4, "sonority-1");
    assert(common_practice_identify_weighted(&seg, 0, COMMON_PRACTICE_KEY_MINOR, &config, &result) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert_applied(&result, 0, true, 4);
    common_practice_weighted_result_destroy(&result);
}

/* context_next false (R-379 (3)): an incomplete segment reads no chord from the segment that follows it, and still reads the one before. */
static void test_context_next_false_reads_no_chord_from_the_next_segment(void) {
    CommonPracticeWeightedConfig config = common_practice_weighted_config_default();
    assert(config.context_next);
    config.context_next = false;

    CommonPracticeMember m0[2] = {note("g0", 55, false, 0.5, S, NULL, 0.0), note("e0", 64, false, 0.5, S, NULL, 0.0)};
    CommonPracticeMember m1[3] = {note("c", 48, false, 1.0, S, NULL, 0.5), note("e", 52, false, 1.0, S, NULL, 0.5),
                                  note("g", 55, false, 1.0, S, NULL, 0.5)};
    CommonPracticeSegment segments[2] = {segment(m0, 2, "s0"), segment(m1, 3, "s1")};
    CommonPracticeWeightedResults results = {0};
    assert(common_practice_identify_weighted_sequence(segments, 2, 0, COMMON_PRACTICE_KEY_MAJOR, &config, &results) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(results.items[0].context == COMMON_PRACTICE_CONTEXT_NONE && results.items[0].best.degree == 3); /* its own reading: iii */
    assert(results.items[1].best.degree == 1);
    common_practice_weighted_results_destroy(&results);

    /* the previous segment's chord is still taken */
    CommonPracticeMember p0[3] = {note("c", 48, false, 1.0, S, NULL, 0.0), note("e", 52, false, 1.0, S, NULL, 0.0),
                                  note("g", 55, false, 1.0, S, NULL, 0.0)};
    CommonPracticeMember p1[2] = {note("g1", 55, false, 0.5, W, NULL, 1.0), note("e1", 64, false, 0.5, W, NULL, 1.0)};
    CommonPracticeSegment pair[2] = {segment(p0, 3, "p0"), segment(p1, 2, "p1")};
    assert(common_practice_identify_weighted_sequence(pair, 2, 0, COMMON_PRACTICE_KEY_MAJOR, &config, &results) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(results.items[1].context == COMMON_PRACTICE_CONTEXT_PREVIOUS && results.items[1].best.degree == 1);
    common_practice_weighted_results_destroy(&results);

    /* the default reads the next chord, as before */
    config.context_next = true;
    assert(common_practice_identify_weighted_sequence(segments, 2, 0, COMMON_PRACTICE_KEY_MAJOR, &config, &results) ==
           COMMON_PRACTICE_WEIGHTED_OK);
    assert(results.items[0].context == COMMON_PRACTICE_CONTEXT_NEXT && results.items[0].best.degree == 1);
    common_practice_weighted_results_destroy(&results);
}

int main(void) {
    test_a_required_bass_chord_tone_beats_the_upper_voices();
    test_a_segment_without_a_bass_override_reads_as_before();
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
    test_an_applied_dominant_seventh_is_ranked_when_asked();
    test_clementi_m6_beat_4_reads_as_vii_dim_of_v();
    test_a_chromatic_neighbour_does_not_make_an_applied_chord();
    test_an_applied_dominant_seventh_that_misses_the_bass_is_not_read();
    test_a_bare_fifth_is_not_an_applied_chord();
    test_a_diminished_seventh_is_rooted_by_where_it_resolves_in_major();
    test_a_diminished_seventh_is_rooted_by_where_it_resolves_in_minor();
    test_applied_dominant_sevenths_in_minor();
    test_context_next_false_reads_no_chord_from_the_next_segment();
    printf("test_weighted_chord_identification: ok\n");
    return 0;
}
