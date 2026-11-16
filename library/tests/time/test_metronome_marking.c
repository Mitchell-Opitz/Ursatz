#include "time/metronome_marking.h"

#include <assert.h>
#include <stddef.h>

static Duration *duration(long numerator, unsigned long denominator) {
    Duration *d = NULL;
    assert(duration_create(numerator, denominator, &d) == DURATION_OK);
    return d;
}

static MetronomeMarking *marking(long unit_n, unsigned long unit_d, unsigned long bpm_n, unsigned long bpm_d) {
    Duration *unit = duration(unit_n, unit_d);
    MetronomeMarking *m = NULL;
    assert(metronome_marking_create(unit, bpm_n, bpm_d, &m) == METRONOME_MARKING_OK);
    duration_destroy(unit);
    return m;
}

static void test_create_rejects_invalid(void) {
    MetronomeMarking *m = NULL;
    Duration *quarter = duration(1, 4);
    Duration *zero = duration(0, 1);

    assert(metronome_marking_create(NULL, 60, 1, &m) == METRONOME_MARKING_ERR_NULL_BEAT_UNIT);
    assert(metronome_marking_create(zero, 60, 1, &m) == METRONOME_MARKING_ERR_ZERO_BEAT_UNIT);
    assert(metronome_marking_create(quarter, 0, 1, &m) == METRONOME_MARKING_ERR_NON_POSITIVE_BPM);
    assert(metronome_marking_create(quarter, 60, 0, &m) == METRONOME_MARKING_ERR_NON_POSITIVE_BPM);
    assert(m == NULL);

    duration_destroy(quarter);
    duration_destroy(zero);
}

static void test_accessors_reduce_to_lowest_terms(void) {
    /* Beat unit 2/8 reduces to 1/4; bpm 120/2 reduces to 60/1. */
    MetronomeMarking *m = marking(2, 8, 120, 2);
    assert(metronome_marking_beat_unit_numerator(m) == 1);
    assert(metronome_marking_beat_unit_denominator(m) == 4);
    assert(metronome_marking_bpm_numerator(m) == 60);
    assert(metronome_marking_bpm_denominator(m) == 1);
    metronome_marking_destroy(m);
}

/* Quarter = 60: sixty quarter notes in a minute, one per second. */
static void test_quarter_equals_sixty_is_one_second(void) {
    MetronomeMarking *m = marking(1, 4, 60, 1);
    unsigned long n, d;

    metronome_marking_seconds_per_beat_unit(m, &n, &d);
    assert(n == 1 && d == 1);

    metronome_marking_destroy(m);
}

static void test_other_beat_units_and_rates(void) {
    unsigned long n, d;

    /* Quarter = 120: half a second per quarter. */
    MetronomeMarking *fast = marking(1, 4, 120, 1);
    metronome_marking_seconds_per_beat_unit(fast, &n, &d);
    assert(n == 1 && d == 2);
    metronome_marking_destroy(fast);

    /* Dotted quarter (3/8) = 60: a dotted quarter lasts one second. */
    MetronomeMarking *dotted = marking(3, 8, 60, 1);
    metronome_marking_seconds_per_beat_unit(dotted, &n, &d);
    assert(n == 1 && d == 1);
    metronome_marking_destroy(dotted);

    /* A fractional rate is exact: quarter = 72.5 (145/2) -> 120/145 = 24/29 s per quarter. */
    MetronomeMarking *fractional = marking(1, 4, 145, 2);
    metronome_marking_seconds_per_beat_unit(fractional, &n, &d);
    assert(n == 24 && d == 29);
    metronome_marking_destroy(fractional);
}

int main(void) {
    test_create_rejects_invalid();
    test_accessors_reduce_to_lowest_terms();
    test_quarter_equals_sixty_is_one_second();
    test_other_beat_units_and_rates();
    return 0;
}
