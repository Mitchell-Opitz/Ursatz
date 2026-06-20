#include <math.h>
#include <stdlib.h>

#include "analysis_cadence.h"
#include "analysis_test_util.h"

/* Cadences from real music: candidates at cadence points, classified by the existing rules. The fixture's segments are
   quarter notes in 4/4: index 0 is beat 1 (strong), 1 beat 2 (weak), 2 beat 3 (medium), 3 beat 4 (weak), and so on. */

static HarmonyCadences find(Fixture *f, HarmonySegment **segments, const HarmonyCadenceConfig *config) {
    assert(harmony_analysis_prepare(f->inputs, f->count, segments) == HARMONY_ANALYSIS_OK);
    HarmonyCadences found;
    assert(harmony_analysis_find_cadences(*segments, f->count, config, &found) == HARMONY_CADENCE_ANALYSIS_OK);
    return found;
}

static void test_authentic_v_to_i_on_a_strong_enough_beat(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV, beat 1 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 2 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I, beat 3 (medium) */
    HarmonySegment *segments = NULL;
    HarmonyCadences found = find(&f, &segments, NULL);
    assert(found.count == 1);
    const HarmonyCadenceResult *c = &found.items[0];
    assert(c->type == CADENCE_TYPE_PERFECT_AUTHENTIC && c->soprano_degree == 1);
    assert(c->dominant_first == 1 && c->dominant_last == 1 && !c->has_six_four);
    assert(c->arrival_first == 2 && c->arrival_last == 2);
    assert(strcmp(c->key_ref, "C") == 0 && c->uncertainty == 0.0);

    /* The L5 shell has the segment ids of the dominant and the arrival; the Interpretation targets its id. */
    assert(cadence_event_id_count(c->cadence) == 2);
    assert(strcmp(entity_id_value(cadence_event_id_at(c->cadence, 0)), "seg-1") == 0);
    assert(strcmp(entity_id_value(cadence_event_id_at(c->cadence, 1)), "seg-2") == 0);
    assert(strcmp(entity_id_value(interpretation_target_id(c->interpretation)),
                  entity_id_value(cadence_id(c->cadence))) == 0);
    assert(interpretation_framework_reference(c->interpretation) != NULL);
    HarmonyCadenceClaim claim;
    assert(harmony_cadence_claim_read(c->interpretation, &claim));
    assert(claim.type == (int)CADENCE_TYPE_PERFECT_AUTHENTIC && claim.soprano_degree == 1 &&
           strcmp(claim.arrival_segment, "seg-2") == 0 &&
           strcmp(claim.dominant_segment, "seg-1") == 0 && strcmp(claim.key_ref, "C") == 0);

    /* The TimeSpan runs from the dominant's start (1/4) to the arrival's end (3/4). */
    assert(musical_time_numerator(time_span_start(c->span)) == 1 && musical_time_denominator(time_span_start(c->span)) == 4);
    assert(musical_time_numerator(time_span_end(c->span)) == 3 && musical_time_denominator(time_span_end(c->span)) == 4);
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static CadenceType type_with_soprano(int soprano_degree, double *uncertainty) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, soprano_degree);
    HarmonySegment *segments = NULL;
    HarmonyCadences found = find(&f, &segments, NULL);
    assert(found.count == 1);
    CadenceType type = found.items[0].type;
    *uncertainty = found.items[0].uncertainty;
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
    return type;
}

static void test_pac_iac_by_the_soprano(void) {
    double u;
    assert(type_with_soprano(1, &u) == CADENCE_TYPE_PERFECT_AUTHENTIC && u == 0.0);
    assert(type_with_soprano(3, &u) == CADENCE_TYPE_IMPERFECT_AUTHENTIC && u == 0.0);
    assert(type_with_soprano(5, &u) == CADENCE_TYPE_IMPERFECT_AUTHENTIC && u == 0.0);
}

static void test_soprano_absent_is_authentic_unclassified_with_a_penalty(void) {
    double u;
    assert(type_with_soprano(0, &u) == CADENCE_TYPE_AUTHENTIC_UNSPECIFIED);
    assert(fabs(u - 0.3) < 1e-12); /* 1 - (1 - 0)(1 - 0.3) */

    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7)->uncertainty = 0.5;
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 0);
    HarmonySegment *segments = NULL;
    HarmonyCadenceConfig config = harmony_cadence_config_default();
    config.soprano_absent_penalty = 0.2;
    HarmonyCadences found = find(&f, &segments, &config);
    assert(found.count == 1 && fabs(found.items[0].uncertainty - (1.0 - 0.5 * 0.8)) < 1e-12);
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_a_weak_beat_arrival_is_not_a_cadence(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 1 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I, beat 2: weak */
    HarmonySegment *segments = NULL;
    HarmonyCadenceConfig config = harmony_cadence_config_default();
    config.allow_weak_arrival = false;
    HarmonyCadences found = find(&f, &segments, &config);
    assert(found.count == 0 && found.items == NULL);
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_a_weak_beat_arrival_after_a_stronger_dominant(void) {
    /* V on beat 1, I on beat 2 (weak) as the last chord of the piece: an arrival by default. */
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV, beat 1 */
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV, beat 2 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 3 (medium) */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I, beat 4: weak, ends the piece */
    HarmonySegment *segments = NULL;
    HarmonyCadences found = find(&f, &segments, NULL);
    assert(found.count == 1);
    const HarmonyCadenceResult *c = &found.items[0];
    assert(c->type == CADENCE_TYPE_PERFECT_AUTHENTIC && c->arrival_first == 3);
    assert(c->arrival_beat_strength == BEAT_STRENGTH_WEAK);
    HarmonyCadenceClaim claim;
    assert(harmony_cadence_claim_read(c->interpretation, &claim));
    assert(claim.arrival_beat_strength == (int)BEAT_STRENGTH_WEAK);
    harmony_cadences_destroy(&found);

    /* Switched off, it is not one. */
    HarmonyCadenceConfig config = harmony_cadence_config_default();
    config.allow_weak_arrival = false;
    assert(harmony_analysis_find_cadences(segments, f.count, &config, &found) == HARMONY_CADENCE_ANALYSIS_OK);
    assert(found.count == 1 && found.items[0].type == CADENCE_TYPE_HALF); /* the IV to V, not the weak I */
    harmony_cadences_destroy(&found);

    /* A tonic that is neither long (here: two beats), before a soprano rest, nor last is not an arrival. */
    HarmonyCadenceConfig strict = harmony_cadence_config_default();
    strict.arrival_min_beats = 2.0;
    harmony_analysis_destroy(segments);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV, beat 1 of the next bar */
    assert(harmony_analysis_prepare(f.inputs, f.count, &segments) == HARMONY_ANALYSIS_OK);
    assert(harmony_analysis_find_cadences(segments, f.count, &strict, &found) == HARMONY_CADENCE_ANALYSIS_OK);
    assert(found.count == 0);
    harmony_cadences_destroy(&found);

    /* ... but a soprano rest right after it makes it one. */
    f.inputs[4].soprano_degree = 0;
    harmony_analysis_destroy(segments);
    assert(harmony_analysis_prepare(f.inputs, f.count, &segments) == HARMONY_ANALYSIS_OK);
    assert(harmony_analysis_find_cadences(segments, f.count, &strict, &found) == HARMONY_CADENCE_ANALYSIS_OK);
    assert(found.count == 1 && found.items[0].arrival_first == 3);
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_a_weak_arrival_needs_a_stronger_dominant(void) {
    /* The V run starts on beat 2 (weak) and the I on beat 4 (weak): equal strength, so not an arrival. */
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV, beat 1 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 2: weak */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 3 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I, beat 4: weak, ends the piece */
    HarmonySegment *segments = NULL;
    HarmonyCadences found = find(&f, &segments, NULL);
    assert(found.count == 0);
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_a_weak_arrival_on_the_half_beat_after_a_weak_dominant(void) {
    /* 3/4 style: V on the weak beat 2, I on the weak half beat after it. The fixture is quarter-note based, so the
       half beat is made by moving the I's start to an eighth after the V's. */
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV, beat 1 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 2: weak, on the beat */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I, beat 3 */
    Duration *eighth = NULL;
    assert(duration_create(1, 8, &eighth) == DURATION_OK);
    metrical_position_destroy(f.positions[2]);
    assert(metrical_position_create(1, eighth, &f.positions[2]) == METRICAL_POSITION_OK);
    duration_destroy(eighth);
    f.inputs[2].position = f.positions[2];
    f.inputs[2].beat_strength = BEAT_STRENGTH_WEAK; /* off the beat: weak, like the beat before it */
    f.inputs[1].beat_strength = BEAT_STRENGTH_WEAK;
    HarmonySegment *segments = NULL;
    HarmonyCadences found = find(&f, &segments, NULL);
    assert(found.count == 1 && found.items[0].arrival_first == 2 &&
           found.items[0].arrival_beat_strength == BEAT_STRENGTH_WEAK);
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_an_inverted_v_to_i_is_not_a_cadence(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 1, 7, 7);   /* V6 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I, beat 3 */
    HarmonySegment *segments = NULL;
    HarmonyCadences found = find(&f, &segments, NULL);
    assert(found.count == 0);
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_cadential_six_four_is_part_of_the_dominant(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV, beat 1 */
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV, beat 2 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 2, 5, 3);   /* I6/4, beat 3 (medium) */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 2);   /* V, beat 4 (weak): the 6/4 is not weaker */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I, measure 2 beat 1 (strong) */
    HarmonySegment *segments = NULL;
    HarmonyCadences found = find(&f, &segments, NULL);
    assert(found.count == 1);
    const HarmonyCadenceResult *c = &found.items[0];
    assert(c->type == CADENCE_TYPE_PERFECT_AUTHENTIC && c->has_six_four);
    assert(c->six_four_first == 2 && c->six_four_last == 2);
    assert(c->dominant_first == 2 && c->dominant_last == 3 && c->arrival_first == 4);
    assert(cadence_event_id_count(c->cadence) == 3);
    HarmonyCadenceClaim claim;
    assert(harmony_cadence_claim_read(c->interpretation, &claim) && claim.has_six_four &&
           strcmp(claim.dominant_segment, "seg-2") == 0);
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_uncertainty_is_the_largest_of_the_chords_involved(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6)->uncertainty = 0.9; /* not part of the cadence */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7)->uncertainty = 0.2;
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1)->uncertainty = 0.4;
    HarmonySegment *segments = NULL;
    HarmonyCadences found = find(&f, &segments, NULL);
    assert(found.count == 1 && fabs(found.items[0].uncertainty - 0.4) < 1e-12);
    const Uncertainty *u = interpretation_uncertainty(found.items[0].interpretation);
    assert(uncertainty_scalar_confidence_numerator(u) == 600000UL);
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static CadenceType only_type(Fixture *f, bool *post_cadential, bool *six_four, size_t *count) {
    HarmonySegment *segments = NULL;
    HarmonyCadences found = find(f, &segments, NULL);
    *count = found.count;
    CadenceType type = found.count > 0 ? found.items[found.count - 1].type : CADENCE_TYPE_UNKNOWN;
    *post_cadential = found.count > 0 ? found.items[found.count - 1].post_cadential : false;
    *six_four = found.count > 0 ? found.items[found.count - 1].has_six_four : false;
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    return type;
}

static void test_half_cadence(void) {
    bool post, six_four;
    size_t n;
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV, beat 2 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 3 */
    assert(only_type(&f, &post, &six_four, &n) == CADENCE_TYPE_HALF && n == 1 && !six_four);
    fixture_destroy(&f);

    /* After a tonic, and on a V7 (the framework's soft rule allows it). */
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 5, SEVENTH_DOMINANT, 4, 0, 5, 7);
    assert(only_type(&f, &post, &six_four, &n) == CADENCE_TYPE_HALF && n == 1);
    fixture_destroy(&f);

    /* With a cadential 6/4 before the V: the 6/4 is part of the dominant. I6/4 on a strong beat, V on a medium one. */
    fixture_init(&f);
    for (int i = 0; i < 4; i++) {
        fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    }
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 2, 5, 3);   /* measure 2 beat 1 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 2, 5, 3);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 2);   /* measure 2 beat 3 */
    assert(only_type(&f, &post, &six_four, &n) == CADENCE_TYPE_HALF && n == 1 && six_four);
    fixture_destroy(&f);

    /* A neighbor 6/4 between two V is part of the V: the second V is not a half cadence. */
    fixture_init(&f);
    for (int i = 0; i < 4; i++) {
        fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    }
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* measure 2 beat 1: the half cadence */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 2, 5, 1);   /* I6/4 on beat 2: weaker than the V after it, a neighbor */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* beat 3 */
    assert(only_type(&f, &post, &six_four, &n) == CADENCE_TYPE_HALF && n == 1 && !six_four);
    fixture_destroy(&f);

    /* A V that the next cadence resolves is that cadence's dominant, not a half cadence. */
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V on beat 3 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I, measure 2 beat 1: a PAC */
    assert(only_type(&f, &post, &six_four, &n) == CADENCE_TYPE_PERFECT_AUTHENTIC && n == 1);
    fixture_destroy(&f);

    /* Not after a dominant, and not on a weak beat. */
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 2);   /* beat 3, after the tonic: a half cadence */
    fixture_add(&f, 2, TRIAD_MINOR, 3, 0, 2, 4);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* beat 1 of measure 2, after ii: another */
    assert(only_type(&f, &post, &six_four, &n) == CADENCE_TYPE_HALF && n == 2);
    fixture_destroy(&f);
}

static void test_deceptive(void) {
    bool post, six_four;
    size_t n;
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 6, TRIAD_MINOR, 3, 0, 6, 1);   /* vi, beat 3 */
    assert(only_type(&f, &post, &six_four, &n) == CADENCE_TYPE_DECEPTIVE && n == 1);
    fixture_destroy(&f);

    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, SEVENTH_DOMINANT, 4, 0, 5, 7);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 1, 6, 6);   /* IV6, the less common substitute */
    assert(only_type(&f, &post, &six_four, &n) == CADENCE_TYPE_DECEPTIVE && n == 1);
    fixture_destroy(&f);
}

static void test_plagal_after_a_pac_is_post_cadential(void) {
    bool post, six_four;
    size_t n;
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I, beat 3: a PAC */
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV, beat 4 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I, measure 2 beat 1 */
    assert(only_type(&f, &post, &six_four, &n) == CADENCE_TYPE_PLAGAL && n == 2 && post);
    fixture_destroy(&f);

    /* After an imperfect authentic cadence, or after no cadence, IV to I is a prolongation, not found. */
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);   /* I with mi: an IAC */
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);
    assert(only_type(&f, &post, &six_four, &n) == CADENCE_TYPE_IMPERFECT_AUTHENTIC && n == 1);
    fixture_destroy(&f);

    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* beat 3 */
    assert(only_type(&f, &post, &six_four, &n) == CADENCE_TYPE_UNKNOWN && n == 0);
    fixture_destroy(&f);
}

static void test_subverted(void) {
    bool post, six_four;
    size_t n;
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* a predominant */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 1, 3, 3);   /* I6, beat 3 */
    assert(only_type(&f, &post, &six_four, &n) == CADENCE_TYPE_SUBVERTED && n == 1);
    fixture_destroy(&f);

    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV, root position: neither tonic nor deceptive */
    assert(only_type(&f, &post, &six_four, &n) == CADENCE_TYPE_SUBVERTED && n == 1);
    fixture_destroy(&f);

    /* A V that is not cadential (after a tonic, no 6/4) resolving to I6 is still the SUBVERTED reading (C7, R-093);
       the prolongation reading carries the doubt (here 500 - 150 for the medium beat - 150 for no approach = 200). */
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 1, 3, 3);
    assert(only_type(&f, &post, &six_four, &n) == CADENCE_TYPE_SUBVERTED && n == 1);
    fixture_destroy(&f);
}

static void test_cadence_points_and_config(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);   /* I, beat 1 */
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV, beat 2: weak */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 3: a point */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I, beat 4: weak */
    HarmonySegment *segments = NULL;
    assert(harmony_analysis_prepare(f.inputs, f.count, &segments) == HARMONY_ANALYSIS_OK);
    HarmonyCadencePoint *points = NULL;
    size_t n = 0;
    HarmonyCadenceConfig config = harmony_cadence_config_default();
    config.allow_weak_arrival = false;
    assert(harmony_analysis_cadence_points(segments, f.count, &config, &points, &n) == HARMONY_CADENCE_ANALYSIS_OK);
    assert(n == 1 && points[0].arrival_first == 2 && points[0].before_first == 1);
    free(points);

    /* With weak arrivals allowed, the final I on beat 4 (after the V on the stronger beat 3) is a second point. */
    assert(harmony_analysis_cadence_points(segments, f.count, NULL, &points, &n) == HARMONY_CADENCE_ANALYSIS_OK);
    assert(n == 2 && points[1].arrival_first == 3);
    free(points);

    /* A longer minimum arrival drops it unless the soprano rests right after: here the next segment has a soprano. */
    config.arrival_min_beats = 2.0;
    config.allow_weak_arrival = false;
    assert(harmony_analysis_cadence_points(segments, f.count, &config, &points, &n) == HARMONY_CADENCE_ANALYSIS_OK);
    assert(n == 0 && points == NULL);
    f.inputs[3].soprano_degree = 0; /* the soprano rests after the V */
    harmony_analysis_destroy(segments);
    assert(harmony_analysis_prepare(f.inputs, f.count, &segments) == HARMONY_ANALYSIS_OK);
    assert(harmony_analysis_cadence_points(segments, f.count, &config, &points, &n) == HARMONY_CADENCE_ANALYSIS_OK);
    assert(n == 1);
    free(points);
    config.soprano_rest_counts = false;
    assert(harmony_analysis_cadence_points(segments, f.count, &config, &points, &n) == HARMONY_CADENCE_ANALYSIS_OK);
    assert(n == 0);

    /* A beat length on the segment overrides the config's: a half-note beat makes a quarter arrival half a beat. */
    Duration *half = NULL;
    assert(duration_create(1, 2, &half) == DURATION_OK);
    for (size_t i = 0; i < f.count; i++) {
        f.inputs[i].beat_length = half;
    }
    config = harmony_cadence_config_default();
    config.allow_weak_arrival = false;
    config.soprano_rest_counts = false;
    assert(harmony_analysis_cadence_points(segments, f.count, &config, &points, &n) == HARMONY_CADENCE_ANALYSIS_OK);
    assert(n == 0); /* one quarter is shorter than one half-note beat */
    duration_destroy(half);

    config = harmony_cadence_config_default();
    config.arrival_min_beats = -1.0;
    assert(harmony_analysis_cadence_points(segments, f.count, &config, &points, &n) ==
           HARMONY_CADENCE_ANALYSIS_ERR_BAD_CONFIG);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

/* Cadences at formal boundaries: a V before a section start is a half cadence of the earlier section. Segment n starts
   at n/4 whole notes. */
static HarmonyCadences boundary(Fixture *f, HarmonySegment **segments, const double *starts, size_t n) {
    HarmonyCadences found = find(f, segments, NULL);
    assert(harmony_analysis_add_boundary_cadences(*segments, f->count, NULL, starts, n, &found) ==
           HARMONY_CADENCE_ANALYSIS_OK);
    return found;
}

static void test_a_dominant_before_a_section_start_is_a_half_cadence(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);   /* I, beat 1 */
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);   /* IV, beat 2 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 3 */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 4 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* I, measure 2 beat 1: the new section */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);
    HarmonySegment *segments = NULL;
    HarmonyCadences before = find(&f, &segments, NULL);
    assert(before.count == 1 && before.items[0].type == CADENCE_TYPE_PERFECT_AUTHENTIC);
    harmony_cadences_destroy(&before);
    harmony_analysis_destroy(segments);

    double starts[1] = {1.0};
    HarmonyCadences found = boundary(&f, &segments, starts, 1);
    assert(found.count == 1);
    const HarmonyCadenceResult *c = &found.items[0];
    assert(c->type == CADENCE_TYPE_HALF && c->dominant_first == 2 && c->arrival_first == 2 && c->arrival_last == 3);
    HarmonyCadenceClaim claim;
    assert(harmony_cadence_claim_read(c->interpretation, &claim) && claim.type == (int)CADENCE_TYPE_HALF &&
           strcmp(claim.arrival_segment, "seg-2") == 0);
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_a_boundary_leaves_other_cadences_alone(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);   /* a PAC at beat 3 */
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);
    HarmonySegment *segments = NULL;

    /* no section start: nothing changes */
    HarmonyCadences found = boundary(&f, &segments, NULL, 0);
    assert(found.count == 1 && found.items[0].type == CADENCE_TYPE_PERFECT_AUTHENTIC);
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);

    /* a start that no segment has, and a start that is not right after a V: ignored */
    double starts[2] = {0.9, 0.75};
    found = boundary(&f, &segments, starts, 2);
    assert(found.count == 1 && found.items[0].type == CADENCE_TYPE_PERFECT_AUTHENTIC);
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);

    /* a start at the tonic's arrival with the V before it, but no tonic or predominant before the V: the PAC stays */
    fixture_destroy(&f);
    fixture_init(&f);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 1);
    double lone[1] = {0.5};
    found = boundary(&f, &segments, lone, 1);
    assert(found.count == 0 || found.items[0].type != CADENCE_TYPE_HALF);
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_a_recorded_half_cadence_is_kept_at_a_boundary(void) {
    Fixture f;
    fixture_init(&f);
    fixture_add(&f, 1, TRIAD_MAJOR, 3, 0, 1, 3);
    fixture_add(&f, 4, TRIAD_MAJOR, 3, 0, 4, 6);
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);   /* V, beat 3: a half cadence */
    fixture_add(&f, 5, TRIAD_MAJOR, 3, 0, 5, 7);
    fixture_add(&f, 6, TRIAD_MINOR, 3, 0, 6, 1);   /* vi at the section start */
    HarmonySegment *segments = NULL;
    HarmonyCadences plain = find(&f, &segments, NULL);
    size_t before = plain.count;
    harmony_cadences_destroy(&plain);
    harmony_analysis_destroy(segments);
    double starts[1] = {1.0};
    HarmonyCadences found = boundary(&f, &segments, starts, 1);
    size_t halves = 0;
    for (size_t i = 0; i < found.count; i++) {
        halves += found.items[i].type == CADENCE_TYPE_HALF ? 1 : 0;
    }
    assert(found.count >= before && halves <= 1);
    harmony_cadences_destroy(&found);
    harmony_analysis_destroy(segments);
    fixture_destroy(&f);
}

static void test_boundary_arguments(void) {
    HarmonyCadences found = {NULL, 0};
    double bad[1] = {NAN};
    assert(harmony_analysis_add_boundary_cadences(NULL, 0, NULL, NULL, 0, NULL) == HARMONY_CADENCE_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(harmony_analysis_add_boundary_cadences(NULL, 0, NULL, NULL, 1, &found) == HARMONY_CADENCE_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(harmony_analysis_add_boundary_cadences(NULL, 0, NULL, bad, 1, &found) == HARMONY_CADENCE_ANALYSIS_ERR_BAD_CONFIG);
    assert(harmony_analysis_add_boundary_cadences(NULL, 0, NULL, NULL, 0, &found) == HARMONY_CADENCE_ANALYSIS_OK);
}

int main(void) {
    test_authentic_v_to_i_on_a_strong_enough_beat();
    test_pac_iac_by_the_soprano();
    test_soprano_absent_is_authentic_unclassified_with_a_penalty();
    test_a_weak_beat_arrival_is_not_a_cadence();
    test_a_weak_beat_arrival_after_a_stronger_dominant();
    test_a_weak_arrival_needs_a_stronger_dominant();
    test_a_weak_arrival_on_the_half_beat_after_a_weak_dominant();
    test_an_inverted_v_to_i_is_not_a_cadence();
    test_half_cadence();
    test_deceptive();
    test_plagal_after_a_pac_is_post_cadential();
    test_subverted();
    test_cadential_six_four_is_part_of_the_dominant();
    test_uncertainty_is_the_largest_of_the_chords_involved();
    test_cadence_points_and_config();
    test_a_dominant_before_a_section_start_is_a_half_cadence();
    test_a_boundary_leaves_other_cadences_alone();
    test_a_recorded_half_cadence_is_kept_at_a_boundary();
    test_boundary_arguments();
    printf("standard-harmony analysis cadences: all tests passed\n");
    return 0;
}
