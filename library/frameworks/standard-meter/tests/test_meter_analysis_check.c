#include <limits.h>
#include <math.h>
#include <string.h>

#include "analysis_test_util.h"
#include "interpretation/interpretation.h"
#include "meter_analysis_check.h"

static Rational q(long long n, long long d) {
    return rational_ratio(n, d);
}

/* Changes at every multiple of `step` below `end`, from `first`. */
static size_t times_every(Rational *out, Rational first, Rational step, Rational end) {
    size_t n = 0;
    for (Rational t = first; rational_cmp(t, end) < 0; n++) {
        out[n] = t;
        assert(rational_add(t, step, &t) == RATIONAL_OK);
    }
    return n;
}

static StandardMeterCheck run(const MeterMap *map, const Rational *changes, size_t change_count, const Rational *arrivals,
                              size_t arrival_count, const Rational *onsets, size_t onset_count) {
    StandardMeterCheckEvidence evidence = {changes, change_count, arrivals, arrival_count, onsets, onset_count};
    StandardMeterCheck check;
    assert(standard_meter_check_meter(map, &evidence, &check) == STANDARD_METER_ANALYSIS_OK);
    return check;
}

/* 4/4 music whose chords change once a bar and whose cadences arrive on bars: the declared reading stands alone. */
static void test_effective_reading_alone(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    Rational changes[16];
    size_t n = times_every(changes, q(0, 1), q(1, 1), q(8, 1));
    Rational arrivals[2] = {q(2, 1), q(4, 1)};
    StandardMeterCheck check = run(piece.map, changes, n, arrivals, 2, NULL, 0);
    assert(check.present);
    assert(check.effective.beats == 4 && check.effective.beat_type == 4);
    assert(rational_equals(check.effective.scale, q(1, 1)) && rational_is_zero(check.effective.pickup));
    assert(check.effective.score > 0.9 && check.effective.score <= 1.0);
    assert(!check.has_alternative);
    assert(check.uncertainty >= 0.0 && check.uncertainty <= 1.0);
    piece_destroy(&piece);
}

/* One chord a bar of three quarters, on the first quarter of one bar and the third of the next: the changes sit on the beats
   of 3/4, not on those of 6/8, so a piece in 3/4 that is written in 6/8 is offered 3/4. */
static void test_three_four_written_in_six_eight(void) {
    Piece piece;
    piece_init(&piece, 6, 3);
    Rational changes[8];
    for (long long bar = 0; bar < 8; bar++) {
        changes[bar] = q(3 * bar + (bar % 2 == 0 ? 0 : 2), 4);
    }
    StandardMeterCheck check = run(piece.map, changes, 8, NULL, 0, NULL, 0);
    assert(check.present);
    assert(check.effective.beats == 6 && check.effective.beat_type == 8);
    assert(check.has_other && check.has_alternative);
    assert(check.other.beats == 3 && check.other.beat_type == 4);
    assert(rational_equals(check.other.scale, q(1, 1)) && rational_is_zero(check.other.pickup));
    assert(check.other.score - check.effective.score >= STANDARD_METER_CHECK_MIN_MARGIN);
    assert(fabs(check.uncertainty - (1.0 - (check.other.score - check.effective.score))) < 1e-12);
    piece_destroy(&piece);
}

/* Chords change on the second beat of every bar: the downbeat is one beat late, so a pickup of one beat is offered. */
static void test_pickup_candidate(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    Rational changes[16];
    size_t n = times_every(changes, q(1, 4), q(1, 1), q(8, 1));
    StandardMeterCheck check = run(piece.map, changes, n, NULL, 0, NULL, 0);
    assert(check.present && check.has_other && check.has_alternative);
    assert(check.other.beats == 4 && check.other.beat_type == 4);
    assert(rational_equals(check.other.scale, q(1, 1)) && rational_equals(check.other.pickup, q(1, 4)));
    piece_destroy(&piece);
}

static void test_no_changes_is_not_present(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    StandardMeterCheck check = run(piece.map, NULL, 0, NULL, 0, NULL, 0);
    assert(!check.present && !check.has_other && !check.has_alternative);
    Rational arrivals[1] = {q(1, 1)};
    check = run(piece.map, NULL, 0, arrivals, 1, NULL, 0);
    assert(!check.present);
    piece_destroy(&piece);
}

/* Two readings with the same barlines (3/4 and 3/8 at scale 1/2) score alike; the quarter-beat signature is the one shown. */
static void test_equal_scores_choose_the_quarter_beat_signature(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    Rational changes[32];
    size_t n = times_every(changes, q(0, 1), q(3, 4), q(6, 1));
    StandardMeterCheck check = run(piece.map, changes, n, NULL, 0, NULL, 0);
    assert(check.has_other && check.has_alternative);
    assert(check.other.beats == 3 && check.other.beat_type == 4 && rational_equals(check.other.scale, q(1, 1)));
    piece_destroy(&piece);
}

/* The common onset gap breaks a tie the other way: quarter-note gaps put the halved reading (3/8 at scale 1/2) at an eighth. */
static void test_onset_gap_decides_between_equal_barlines(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    Rational changes[32];
    size_t n = times_every(changes, q(0, 1), q(3, 4), q(6, 1));
    Rational onsets[32];
    size_t on = times_every(onsets, q(0, 1), q(1, 4), q(6, 1));
    StandardMeterCheck check = run(piece.map, changes, n, NULL, 0, onsets, on);
    assert(check.has_other && check.has_alternative);
    assert(check.other.beats == 3 && check.other.beat_type == 8 && rational_equals(check.other.scale, q(1, 2)));
    piece_destroy(&piece);
}

/* A time the arithmetic cannot hold is reported, never wrapped. */
static void test_overflow_is_reported(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    Rational changes[2] = {q(0, 1), q(1, 9223372036854775783LL)};
    StandardMeterCheckEvidence evidence = {changes, 2, NULL, 0, NULL, 0};
    StandardMeterCheck check;
    memset(&check, 0xff, sizeof(check));
    assert(standard_meter_check_meter(piece.map, &evidence, &check) == STANDARD_METER_ANALYSIS_ERR_OVERFLOW);
    assert(!check.present && !check.has_other);
    piece_destroy(&piece);
}

static void test_bad_arguments(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    StandardMeterCheckEvidence evidence = {NULL, 0, NULL, 0, NULL, 0};
    StandardMeterCheck check;
    assert(standard_meter_check_meter(NULL, &evidence, &check) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_meter_check_meter(piece.map, NULL, &check) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_meter_check_meter(piece.map, &evidence, NULL) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    evidence.change_count = 1;
    assert(standard_meter_check_meter(piece.map, &evidence, &check) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    piece_destroy(&piece);
}

/* The rule of a change: entry 0, a different region (-1 is none), a different label (NULL is unlabelled; two NULLs agree). */
static void test_changes(void) {
    Rational times[7] = {q(0, 1), q(1, 4), q(1, 2), q(3, 4), q(1, 1), q(5, 4), q(3, 2)};
    const char *labels[7] = {"I", "I", NULL, NULL, "V", "V", "V"};
    long regions[7] = {0, 0, 0, 0, 0, 1, 1};
    Rational *out = NULL;
    size_t count = 0;
    assert(standard_meter_check_changes(labels, regions, times, 7, &out, &count) == STANDARD_METER_ANALYSIS_OK);
    /* 0: first; 1/4: same; 1/2: "I" to NULL; 3/4: NULL and NULL; 1: NULL to "V"; 5/4: new region; 3/2: same */
    assert(count == 4);
    assert(rational_equals(out[0], q(0, 1)) && rational_equals(out[1], q(1, 2)) && rational_equals(out[2], q(1, 1)) &&
           rational_equals(out[3], q(5, 4)));
    free(out);

    long none[3] = {-1, -1, 2};
    assert(standard_meter_check_changes(NULL, none, times, 3, &out, &count) == STANDARD_METER_ANALYSIS_OK);
    assert(count == 2 && rational_equals(out[0], q(0, 1)) && rational_equals(out[1], q(1, 2)));
    free(out);

    assert(standard_meter_check_changes(NULL, NULL, times, 3, &out, &count) == STANDARD_METER_ANALYSIS_OK);
    assert(count == 1);
    free(out);

    assert(standard_meter_check_changes(NULL, NULL, NULL, 0, &out, &count) == STANDARD_METER_ANALYSIS_OK);
    assert(out == NULL && count == 0);
    assert(standard_meter_check_changes(NULL, NULL, NULL, 1, &out, &count) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_meter_check_changes(NULL, NULL, times, 1, NULL, &count) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
}

static void test_claim(void) {
    Piece piece;
    piece_init(&piece, 6, 3);
    Rational changes[32];
    size_t n = times_every(changes, q(0, 1), q(1, 4), q(6, 1));
    StandardMeterCheck check = run(piece.map, changes, n, NULL, 0, NULL, 0);
    assert(check.present);
    Interpretation *claim = standard_meter_check_interpretation(&check, "piece-1");
    assert(claim != NULL);
    StandardMeterCheck read;
    assert(standard_meter_check_claim_read(claim, &read));
    assert(memcmp(&read, &check, sizeof(check)) == 0);
    interpretation_destroy(claim);

    StandardMeterCheck absent;
    memset(&absent, 0, sizeof(absent));
    assert(standard_meter_check_interpretation(&absent, "piece-1") == NULL);
    assert(standard_meter_check_interpretation(&check, NULL) == NULL);
    assert(!standard_meter_check_claim_read(NULL, &read));
    piece_destroy(&piece);
}

int main(void) {
    test_effective_reading_alone();
    test_three_four_written_in_six_eight();
    test_pickup_candidate();
    test_no_changes_is_not_present();
    test_equal_scores_choose_the_quarter_beat_signature();
    test_onset_gap_decides_between_equal_barlines();
    test_overflow_is_reported();
    test_bad_arguments();
    test_changes();
    test_claim();
    printf("test_meter_analysis_check: ok\n");
    return 0;
}
