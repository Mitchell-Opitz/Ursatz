#include <stdlib.h>

#include "analysis_cadence_claims.h"
#include "analysis_cadence_readings.h"
#include "analysis_runs.h"
#include "analysis_test_util.h"

/* Half readings and cadences claimed at phrase points. C major, 4/4. A Piece holds segments placed by measure and
   beat, each with its own length in beats; a bar() is two half-bar segments of one chord. */

#define PIECE_MAX 64

typedef struct {
    HarmonySegmentInput inputs[PIECE_MAX];
    char ids[PIECE_MAX][16];
    char key_ref[8];
    TimeSpan *spans[PIECE_MAX];
    MetricalPosition *positions[PIECE_MAX];
    size_t count;
} Piece;

typedef struct {
    int degree;
    const int *intervals;
    size_t interval_count;
    int inversion;
    int bass_degree;
    int soprano;
} Chord;

static const Chord I = {1, TRIAD_MAJOR, 3, 0, 1, 1};
static const Chord I3 = {1, TRIAD_MAJOR, 3, 0, 1, 3};
static const Chord I6 = {1, TRIAD_MAJOR, 3, 1, 3, 3};
static const Chord I64 = {1, TRIAD_MAJOR, 3, 2, 5, 3};
static const Chord IV = {4, TRIAD_MAJOR, 3, 0, 4, 6};
static const Chord V = {5, TRIAD_MAJOR, 3, 0, 5, 7};
static const Chord V7 = {5, SEVENTH_DOMINANT, 4, 0, 5, 7};

static void piece_init(Piece *p) {
    memset(p, 0, sizeof(*p));
    snprintf(p->key_ref, sizeof(p->key_ref), "C");
}

static void piece_destroy(Piece *p) {
    for (size_t i = 0; i < p->count; i++) {
        time_span_destroy(p->spans[i]);
        metrical_position_destroy(p->positions[i]);
    }
}

/* A segment of `beats` quarter notes that starts on beat `beat` (1..4) of `measure`. */
static void put(Piece *p, int measure, int beat, int beats, Chord chord) {
    size_t n = p->count++;
    assert(n < PIECE_MAX);
    HarmonySegmentInput *in = &p->inputs[n];
    long from = (long)(measure - 1) * 4 + (beat - 1);
    snprintf(p->ids[n], sizeof(p->ids[n]), "seg-%zu", n);
    MusicalTime *start = NULL;
    MusicalTime *end = NULL;
    assert(musical_time_create(from, 4, &start) == MUSICAL_TIME_OK);
    assert(musical_time_create(from + beats, 4, &end) == MUSICAL_TIME_OK);
    assert(time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &p->spans[n]) == TIME_SPAN_OK);
    musical_time_destroy(start);
    musical_time_destroy(end);
    Duration *offset = NULL;
    assert(duration_create((long)(beat - 1), 4, &offset) == DURATION_OK);
    assert(metrical_position_create(measure, offset, &p->positions[n]) == METRICAL_POSITION_OK);
    duration_destroy(offset);

    memset(in, 0, sizeof(*in));
    in->segment_id = p->ids[n];
    in->span = p->spans[n];
    in->position = p->positions[n];
    in->beat_strength = fixture_strength(beat);
    in->degree = chord.degree;
    for (size_t i = 0; i < chord.interval_count; i++) {
        in->intervals[i] = chord.intervals[i];
    }
    in->interval_count = chord.interval_count;
    in->inversion = chord.inversion;
    in->bass_degree = chord.bass_degree;
    in->soprano_degree = chord.soprano;
    in->key_ref = p->key_ref;
    in->mode = HARMONY_MODE_MAJOR;
}

/* A measure of two half-bar segments of one chord. */
static void bar(Piece *p, int measure, Chord chord) {
    put(p, measure, 1, 2, chord);
    put(p, measure, 3, 2, chord);
}

/* The index of the segment that starts on `beat` of `measure`. */
static size_t index_of(const Piece *p, int measure, int beat) {
    for (size_t i = 0; i < p->count; i++) {
        const Duration *offset = metrical_position_offset(p->inputs[i].position);
        if (metrical_position_measure(p->inputs[i].position) == measure &&
            duration_numerator(offset) * 4 == (long)(beat - 1) * duration_denominator(offset)) {
            return i;
        }
    }
    assert(0);
    return 0;
}

typedef struct {
    Piece piece;
    HarmonySegment *segments;
    HarmonyCadences found;
    HarmonyCadences readings;
} Run;

static void run_init(Run *r) {
    piece_init(&r->piece);
    r->segments = NULL;
    r->found.items = NULL;
    r->found.count = 0;
    r->readings = r->found;
}

/* Prepares the piece, finds the cadences and the half readings. */
static void run_prepare(Run *r) {
    assert(harmony_analysis_prepare(r->piece.inputs, r->piece.count, &r->segments) == HARMONY_ANALYSIS_OK);
    assert(harmony_analysis_find_cadences(r->segments, r->piece.count, NULL, &r->found) == HARMONY_CADENCE_ANALYSIS_OK);
    assert(harmony_analysis_half_readings(r->segments, r->piece.count, NULL, &r->found, &r->readings) ==
           HARMONY_CADENCE_ANALYSIS_OK);
}

static void run_destroy(Run *r) {
    harmony_cadences_destroy(&r->readings);
    harmony_cadences_destroy(&r->found);
    harmony_analysis_destroy(r->segments);
    piece_destroy(&r->piece);
}

static HarmonyCadences claim(Run *r, const HarmonyPhrasePoint *points, size_t point_count) {
    HarmonyCadences claimed;
    assert(harmony_analysis_claim_cadences(r->segments, r->piece.count, NULL, &r->found, &r->readings, points,
                                           point_count, &claimed) == HARMONY_CADENCE_ANALYSIS_OK);
    return claimed;
}

static size_t count_type(const HarmonyCadences *set, CadenceType type) {
    size_t n = 0;
    for (size_t i = 0; i < set->count; i++) {
        n += set->items[i].type == type ? 1 : 0;
    }
    return n;
}

static const HarmonyCadenceResult *arriving_at(const HarmonyCadences *set, size_t segment) {
    for (size_t i = 0; i < set->count; i++) {
        if (set->items[i].arrival_first == segment) {
            return &set->items[i];
        }
    }
    return NULL;
}

/* m1 to m3 I, IV, I; V in m4; I in m5 (soprano 1). */
static void v_then_i(Piece *p) {
    bar(p, 1, I);
    bar(p, 2, IV);
    bar(p, 3, I);
    bar(p, 4, V);
    bar(p, 5, I);
}

static void test_half_reading_before_an_authentic_cadence(void) {
    Run r;
    run_init(&r);
    v_then_i(&r.piece);
    run_prepare(&r);
    assert(count_type(&r.found, CADENCE_TYPE_PERFECT_AUTHENTIC) == 1);
    assert(r.readings.count == 1);
    const HarmonyCadenceResult *h = &r.readings.items[0];
    assert(h->type == CADENCE_TYPE_HALF && h->share == 1000 && !h->alternative.present);
    assert(h->arrival_first == index_of(&r.piece, 4, 1) && h->arrival_last == index_of(&r.piece, 4, 3));
    assert(h->cadence != NULL && h->interpretation != NULL && h->span != NULL);
    HarmonyCadenceClaim c;
    assert(harmony_cadence_claim_read(h->interpretation, &c) && c.type == (int)CADENCE_TYPE_HALF && c.share == 1000);
    run_destroy(&r);
}

static void test_claim_the_half_at_a_half_point_and_the_pac_at_the_unit_end(void) {
    Run r;
    run_init(&r);
    v_then_i(&r.piece);
    bar(&r.piece, 6, IV);
    put(&r.piece, 7, 1, 2, IV);
    put(&r.piece, 7, 3, 2, V);
    bar(&r.piece, 8, I);
    run_prepare(&r);
    assert(count_type(&r.found, CADENCE_TYPE_PERFECT_AUTHENTIC) == 2);
    HarmonyPhrasePoint points[2] = {{4, false, 0, false}, {8, true, 0, false}};
    HarmonyCadences claimed = claim(&r, points, 2);
    assert(claimed.count == 2);
    assert(claimed.items[0].type == CADENCE_TYPE_HALF && claimed.items[0].arrival_first == index_of(&r.piece, 4, 1));
    assert(claimed.items[0].share == 1000 && !claimed.items[0].alternative.present);
    assert(claimed.items[1].type == CADENCE_TYPE_PERFECT_AUTHENTIC &&
           claimed.items[1].arrival_first == index_of(&r.piece, 8, 1));
    assert(arriving_at(&claimed, index_of(&r.piece, 5, 1)) == NULL);
    /* the caller's sets are untouched */
    assert(count_type(&r.found, CADENCE_TYPE_PERFECT_AUTHENTIC) == 2);
    harmony_cadences_destroy(&claimed);
    run_destroy(&r);
}

static void test_a_unit_end_with_a_section_after_it_keeps_both_readings(void) {
    Run r;
    run_init(&r);
    v_then_i(&r.piece);
    run_prepare(&r);
    HarmonyPhrasePoint points[1] = {{4, true, 0, true}};
    HarmonyCadences claimed = claim(&r, points, 1);
    assert(claimed.count == 1);
    const HarmonyCadenceResult *c = &claimed.items[0];
    assert(c->type == CADENCE_TYPE_HALF && c->arrival_first == index_of(&r.piece, 4, 1));
    assert(c->alternative.present && c->alternative.reading == HARMONY_CADENCE_READING_CADENCE &&
           c->alternative.type == CADENCE_TYPE_PERFECT_AUTHENTIC &&
           c->alternative.arrival_first == index_of(&r.piece, 5, 1));
    /* the shares are harmony_cadence_boundary_half_share's: a tie, the HALF primary */
    HarmonyRun runs[PIECE_MAX];
    size_t run_count = harmony_analysis_build_runs(r.segments, r.piece.count, runs);
    size_t v_run = 0;
    while (runs[v_run].first != index_of(&r.piece, 4, 1)) {
        v_run++;
    }
    int half_share = harmony_cadence_boundary_half_share(r.segments, r.piece.count, runs, run_count, v_run, NULL);
    assert(half_share == 500 && c->share == half_share && c->alternative.share == 1000 - half_share);
    harmony_cadences_destroy(&claimed);
    run_destroy(&r);
}

static void test_v_then_an_inverted_tonic_claims_the_half(void) {
    Run r;
    run_init(&r);
    bar(&r.piece, 1, I);
    bar(&r.piece, 2, IV);
    bar(&r.piece, 3, I);
    bar(&r.piece, 4, V);
    bar(&r.piece, 5, I6);
    run_prepare(&r);
    assert(count_type(&r.found, CADENCE_TYPE_SUBVERTED) == 1);
    assert(r.readings.count == 1 && r.readings.items[0].type == CADENCE_TYPE_HALF);
    HarmonyPhrasePoint points[1] = {{4, true, 0, false}};
    HarmonyCadences claimed = claim(&r, points, 1);
    assert(claimed.count == 1 && claimed.items[0].type == CADENCE_TYPE_HALF);
    assert(claimed.items[0].arrival_first == index_of(&r.piece, 4, 1));
    harmony_cadences_destroy(&claimed);
    /* without the readings the found SUBVERTED is replaced all the same */
    HarmonyCadences alone;
    assert(harmony_analysis_claim_cadences(r.segments, r.piece.count, NULL, &r.found, NULL, points, 1, &alone) ==
           HARMONY_CADENCE_ANALYSIS_OK);
    assert(alone.count == 1 && alone.items[0].type == CADENCE_TYPE_HALF &&
           alone.items[0].arrival_first == index_of(&r.piece, 4, 1));
    harmony_cadences_destroy(&alone);
    run_destroy(&r);
}

/* Mozart m10 to m12: IV; V, I6/4, V, I6/4 in m11; V in m12; nothing in m13. */
static void mozart(Piece *p) {
    bar(p, 10, IV);
    put(p, 11, 1, 1, V);
    put(p, 11, 2, 1, I64);
    put(p, 11, 3, 1, V);
    put(p, 11, 4, 1, I64);
    put(p, 12, 1, 2, V);
}

static void test_a_dominant_again_on_a_downbeat_is_a_half_reading(void) {
    Run r;
    run_init(&r);
    mozart(&r.piece);
    run_prepare(&r);
    size_t m12 = index_of(&r.piece, 12, 1);
    const HarmonyCadenceResult *h = arriving_at(&r.readings, m12);
    assert(h != NULL && h->type == CADENCE_TYPE_HALF && h->arrival_last == m12);
    /* a reading is made once per arrival, and none where a HALF was found */
    for (size_t i = 0; i < r.readings.count; i++) {
        for (size_t j = i + 1; j < r.readings.count; j++) {
            assert(r.readings.items[i].arrival_first != r.readings.items[j].arrival_first);
        }
        assert(r.readings.items[i].type == CADENCE_TYPE_HALF);
    }
    for (size_t i = 0; i < r.found.count; i++) {
        if (r.found.items[i].type == CADENCE_TYPE_HALF) {
            assert(arriving_at(&r.readings, r.found.items[i].arrival_first) == NULL);
        }
    }
    run_destroy(&r);
}

static void test_the_half_arrives_at_the_unit_end_in_mozart_m12(void) {
    Run r;
    run_init(&r);
    mozart(&r.piece);
    run_prepare(&r);
    HarmonyPhrasePoint points[1] = {{12, true, 0, false}};
    HarmonyCadences claimed = claim(&r, points, 1);
    assert(claimed.count == 1 && claimed.items[0].type == CADENCE_TYPE_HALF);
    assert(claimed.items[0].arrival_first == index_of(&r.piece, 12, 1));
    harmony_cadences_destroy(&claimed);
    run_destroy(&r);
}

static void test_repeated_closing_bars_give_one_pac_at_the_point(void) {
    Run r;
    run_init(&r);
    put(&r.piece, 25, 1, 2, IV);
    put(&r.piece, 25, 3, 2, V7);
    put(&r.piece, 26, 1, 2, I);
    put(&r.piece, 26, 3, 2, V7);
    put(&r.piece, 27, 1, 2, I);
    put(&r.piece, 27, 3, 2, V7);
    put(&r.piece, 28, 1, 2, I);
    run_prepare(&r);
    assert(count_type(&r.found, CADENCE_TYPE_PERFECT_AUTHENTIC) == 3);
    HarmonyPhrasePoint points[1] = {{28, true, 0, false}};
    HarmonyCadences claimed = claim(&r, points, 1);
    assert(claimed.count == 1 && claimed.items[0].type == CADENCE_TYPE_PERFECT_AUTHENTIC);
    assert(claimed.items[0].arrival_first == index_of(&r.piece, 28, 1));
    harmony_cadences_destroy(&claimed);
    run_destroy(&r);
}

static void test_two_halves_in_one_unit_claim_only_the_end(void) {
    Run r;
    run_init(&r);
    bar(&r.piece, 18, I);
    bar(&r.piece, 19, V);
    bar(&r.piece, 20, IV);
    bar(&r.piece, 21, IV);
    bar(&r.piece, 22, I);
    bar(&r.piece, 23, V);
    bar(&r.piece, 24, IV);
    run_prepare(&r);
    assert(count_type(&r.found, CADENCE_TYPE_HALF) == 2);
    HarmonyPhrasePoint points[2] = {{19, false, 0, false}, {23, true, 0, false}};
    HarmonyCadences claimed = claim(&r, points, 2);
    assert(claimed.count == 1 && claimed.items[0].type == CADENCE_TYPE_HALF);
    assert(claimed.items[0].arrival_first == index_of(&r.piece, 23, 1));
    harmony_cadences_destroy(&claimed);
    /* with the end of another unit, the half point of this one keeps its half */
    HarmonyPhrasePoint other[2] = {{19, false, 1, false}, {23, true, 0, false}};
    claimed = claim(&r, other, 2);
    assert(claimed.count == 2 && claimed.items[0].arrival_first == index_of(&r.piece, 19, 1));
    harmony_cadences_destroy(&claimed);
    run_destroy(&r);
}

static void test_a_half_held_over_the_barline_is_claimed_at_its_point(void) {
    Run r;
    run_init(&r);
    bar(&r.piece, 6, I);
    put(&r.piece, 7, 1, 8, V); /* held through m8 */
    put(&r.piece, 9, 1, 2, IV);
    run_prepare(&r);
    assert(count_type(&r.found, CADENCE_TYPE_HALF) == 1);
    HarmonyPhrasePoint points[1] = {{8, true, 0, false}};
    HarmonyCadences claimed = claim(&r, points, 1);
    assert(claimed.count == 1 && claimed.items[0].type == CADENCE_TYPE_HALF);
    assert(claimed.items[0].arrival_first == index_of(&r.piece, 7, 1));
    harmony_cadences_destroy(&claimed);
    /* a point at m7 or m9 alone does not claim it at m8 */
    HarmonyPhrasePoint elsewhere[1] = {{9, true, 0, false}};
    claimed = claim(&r, elsewhere, 1);
    assert(claimed.count == 0);
    harmony_cadences_destroy(&claimed);
    run_destroy(&r);
}

static void test_a_half_moves_to_the_dominant_that_starts_the_point(void) {
    Run r;
    run_init(&r);
    bar(&r.piece, 6, IV);
    put(&r.piece, 7, 1, 4, V);
    put(&r.piece, 8, 1, 1, V);
    put(&r.piece, 8, 2, 1, I64);
    put(&r.piece, 8, 3, 1, V);
    put(&r.piece, 8, 4, 1, I64);
    run_prepare(&r);
    HarmonyPhrasePoint points[1] = {{8, true, 0, false}};
    HarmonyCadences claimed = claim(&r, points, 1);
    assert(claimed.count == 1 && claimed.items[0].type == CADENCE_TYPE_HALF);
    assert(claimed.items[0].arrival_first == index_of(&r.piece, 8, 1));
    harmony_cadences_destroy(&claimed);
    run_destroy(&r);
}

static void test_a_post_cadential_plagal_after_a_kept_pac_is_kept(void) {
    Run r;
    run_init(&r);
    bar(&r.piece, 1, IV);
    bar(&r.piece, 2, V);
    bar(&r.piece, 3, I);
    bar(&r.piece, 4, IV);
    bar(&r.piece, 5, I3);
    run_prepare(&r);
    assert(count_type(&r.found, CADENCE_TYPE_PLAGAL) == 1);
    HarmonyPhrasePoint points[1] = {{3, true, 0, false}};
    HarmonyCadences claimed = claim(&r, points, 1);
    assert(claimed.count == 2 && claimed.items[0].type == CADENCE_TYPE_PERFECT_AUTHENTIC &&
           claimed.items[1].type == CADENCE_TYPE_PLAGAL && claimed.items[1].post_cadential);
    harmony_cadences_destroy(&claimed);
    /* a PAC that is no point keeps nothing with it */
    HarmonyPhrasePoint none[1] = {{9, true, 0, false}};
    claimed = claim(&r, none, 1);
    assert(claimed.count == 0);
    harmony_cadences_destroy(&claimed);
    run_destroy(&r);
}

static void test_null_arguments_and_no_points(void) {
    Run r;
    run_init(&r);
    v_then_i(&r.piece);
    run_prepare(&r);
    size_t n = r.piece.count;
    HarmonyCadences out = {NULL, 0};
    HarmonyPhrasePoint point = {4, true, 0, false};

    assert(harmony_analysis_half_readings(r.segments, n, NULL, &r.found, NULL) == HARMONY_CADENCE_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(harmony_analysis_half_readings(NULL, n, NULL, &r.found, &out) == HARMONY_CADENCE_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(harmony_analysis_half_readings(NULL, 0, NULL, NULL, &out) == HARMONY_CADENCE_ANALYSIS_OK && out.count == 0);
    harmony_cadences_destroy(&out);
    /* found NULL gives the dominants that come back on a downbeat only: none here */
    assert(harmony_analysis_half_readings(r.segments, n, NULL, NULL, &out) == HARMONY_CADENCE_ANALYSIS_OK && out.count == 0);

    assert(harmony_analysis_claim_cadences(r.segments, n, NULL, &r.found, &r.readings, &point, 1, NULL) ==
           HARMONY_CADENCE_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(harmony_analysis_claim_cadences(NULL, n, NULL, &r.found, &r.readings, &point, 1, &out) ==
           HARMONY_CADENCE_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(harmony_analysis_claim_cadences(r.segments, n, NULL, &r.found, &r.readings, NULL, 1, &out) ==
           HARMONY_CADENCE_ANALYSIS_ERR_NULL_ARGUMENT);

    /* no points, no cadences; NULL found and readings are empty */
    assert(harmony_analysis_claim_cadences(r.segments, n, NULL, &r.found, &r.readings, NULL, 0, &out) ==
           HARMONY_CADENCE_ANALYSIS_OK && out.count == 0 && out.items == NULL);
    assert(harmony_analysis_claim_cadences(r.segments, n, NULL, NULL, NULL, &point, 1, &out) ==
           HARMONY_CADENCE_ANALYSIS_OK && out.count == 0);

    HarmonyCadenceConfig bad = harmony_cadence_config_default();
    bad.beat_length = -1.0;
    assert(harmony_analysis_half_readings(r.segments, n, &bad, &r.found, &out) == HARMONY_CADENCE_ANALYSIS_ERR_BAD_CONFIG);
    assert(harmony_analysis_claim_cadences(r.segments, n, &bad, &r.found, &r.readings, &point, 1, &out) ==
           HARMONY_CADENCE_ANALYSIS_ERR_BAD_CONFIG);
    run_destroy(&r);
}

int main(void) {
    test_half_reading_before_an_authentic_cadence();
    test_claim_the_half_at_a_half_point_and_the_pac_at_the_unit_end();
    test_a_unit_end_with_a_section_after_it_keeps_both_readings();
    test_v_then_an_inverted_tonic_claims_the_half();
    test_a_dominant_again_on_a_downbeat_is_a_half_reading();
    test_the_half_arrives_at_the_unit_end_in_mozart_m12();
    test_repeated_closing_bars_give_one_pac_at_the_point();
    test_two_halves_in_one_unit_claim_only_the_end();
    test_a_half_held_over_the_barline_is_claimed_at_its_point();
    test_a_half_moves_to_the_dominant_that_starts_the_point();
    test_a_post_cadential_plagal_after_a_kept_pac_is_kept();
    test_null_arguments_and_no_points();
    printf("analysis cadence claims: OK\n");
    return 0;
}
