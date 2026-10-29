#include <assert.h>
#include <stdio.h>

#include "meter_test_util.h"
#include "metric_position_rules.h"

/*
 * standard_meter_accent_beats: the beats of a bar that read STRONG or MEDIUM, in every signature
 * the meter tables cover (simple, compound, irregular).
 */

static void check(unsigned beats, int exponent, size_t expected_count, unsigned second) {
    TimeSignature *ts = make_signature(beats, exponent);
    unsigned got[8] = {99, 99, 99, 99, 99, 99, 99, 99};
    BeatStrength strengths[8];
    size_t count = 99;
    assert(standard_meter_accent_beats(ts, got, strengths, 8, &count));
    assert(count == expected_count);
    assert(got[0] == 0 && strengths[0] == BEAT_STRENGTH_STRONG);
    if (expected_count == 2) {
        assert(got[1] == second && strengths[1] == BEAT_STRENGTH_MEDIUM);
    }

    /* The strengths are optional, and the beats are the same without them. */
    unsigned again[8];
    size_t again_count = 0;
    assert(standard_meter_accent_beats(ts, again, NULL, 8, &again_count));
    assert(again_count == count && again[0] == got[0]);

    /* A capacity that holds the accents is enough; one short of them is refused untouched. */
    unsigned one[1] = {99};
    size_t one_count = 99;
    if (expected_count == 1) {
        assert(standard_meter_accent_beats(ts, one, NULL, 1, &one_count) && one_count == 1);
    } else {
        assert(!standard_meter_accent_beats(ts, one, NULL, 1, &one_count));
        assert(one[0] == 99 && one_count == 99);
    }
    time_signature_destroy(ts);
}

static void test_simple(void) {
    check(2, 2, 1, 0); /* 2/4 */
    check(3, 2, 1, 0); /* 3/4 */
    check(4, 2, 2, 2); /* 4/4: S W M W */
    check(2, 1, 1, 0); /* 2/2 */
    check(3, 1, 1, 0); /* 3/2 */
    check(3, 3, 1, 0); /* 3/8 */
    check(4, 3, 2, 2); /* 4/8 */
}

static void test_compound(void) {
    check(6, 3, 1, 0);  /* 6/8: the second dotted beat is weak, as the metric level has it */
    check(9, 3, 1, 0);  /* 9/8 */
    check(12, 3, 2, 2); /* 12/8: four dotted beats, the third is secondary */
}

static void test_irregular(void) {
    check(5, 3, 1, 0); /* 5/8: every pulse a beat, only the downbeat accented */
    check(7, 3, 1, 0); /* 7/8 */
}

static void test_agrees_with_beat_strength(void) {
    for (unsigned beats = 2; beats <= 12; beats++) {
        TimeSignature *ts = make_signature(beats, 3);
        Duration *beat = NULL;
        unsigned per_measure = 0;
        assert(standard_meter_beat_grid(ts, &beat, &per_measure));
        unsigned got[16];
        BeatStrength strengths[16];
        size_t count = 0;
        assert(standard_meter_accent_beats(ts, got, strengths, 16, &count));
        size_t next = 0;
        Duration *start = make_duration(0, 1);
        for (unsigned i = 0; i < per_measure; i++) {
            MetricalPosition *position = NULL;
            BeatStrength strength;
            assert(metrical_position_create(1, start, &position) == METRICAL_POSITION_OK);
            assert(standard_meter_beat_strength(ts, position, &strength));
            if (strength != BEAT_STRENGTH_WEAK) {
                assert(next < count && got[next] == i && strengths[next] == strength);
                next++;
            }
            metrical_position_destroy(position);
            Duration *advanced = NULL;
            assert(duration_add(start, beat, &advanced) == DURATION_OK);
            duration_destroy(start);
            start = advanced;
        }
        duration_destroy(start);
        duration_destroy(beat);
        assert(next == count);
        time_signature_destroy(ts);
    }
}

static void test_invalid(void) {
    TimeSignature *ts = make_signature(4, 2);
    unsigned beats[4];
    size_t count = 99;
    assert(!standard_meter_accent_beats(NULL, beats, NULL, 4, &count));
    assert(!standard_meter_accent_beats(ts, NULL, NULL, 4, &count));
    assert(!standard_meter_accent_beats(ts, beats, NULL, 4, NULL));
    assert(!standard_meter_accent_beats(ts, beats, NULL, 0, &count));
    assert(count == 99);
    time_signature_destroy(ts);
}

int main(void) {
    test_simple();
    test_compound();
    test_irregular();
    test_agrees_with_beat_strength();
    test_invalid();
    printf("standard-meter accent beats tests passed\n");
    return 0;
}
