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

    Duration *quarter = duration(1, 4);
    metronome_marking_seconds_for_duration(m, quarter, &n, &d);
    assert(n == 1 && d == 1);

    /* A half note is two beats: two seconds. A sixteenth is a quarter second. */
    Duration *half = duration(1, 2);
    metronome_marking_seconds_for_duration(m, half, &n, &d);
    assert(n == 2 && d == 1);
    Duration *sixteenth = duration(1, 16);
    metronome_marking_seconds_for_duration(m, sixteenth, &n, &d);
    assert(n == 1 && d == 4);

    duration_destroy(quarter);
    duration_destroy(half);
    duration_destroy(sixteenth);
    metronome_marking_destroy(m);
}

static void test_other_beat_units_and_rates(void) {
    unsigned long n, d;

    /* Quarter = 120: half a second per quarter. */
    MetronomeMarking *fast = marking(1, 4, 120, 1);
    metronome_marking_seconds_per_beat_unit(fast, &n, &d);
    assert(n == 1 && d == 2);
    metronome_marking_destroy(fast);

    /* Dotted quarter (3/8) = 60: a dotted quarter lasts one second, so an eighth lasts a third. */
    MetronomeMarking *dotted = marking(3, 8, 60, 1);
    Duration *eighth = duration(1, 8);
    metronome_marking_seconds_for_duration(dotted, eighth, &n, &d);
    assert(n == 1 && d == 3);
    duration_destroy(eighth);
    metronome_marking_destroy(dotted);

    /* A fractional rate is exact: quarter = 72.5 (145/2) -> 120/145 = 24/29 s per quarter. */
    MetronomeMarking *fractional = marking(1, 4, 145, 2);
    metronome_marking_seconds_per_beat_unit(fractional, &n, &d);
    assert(n == 24 && d == 29);
    metronome_marking_destroy(fractional);

    /* A zero duration lasts zero seconds. */
    MetronomeMarking *m = marking(1, 4, 60, 1);
    Duration *zero = duration(0, 1);
    metronome_marking_seconds_for_duration(m, zero, &n, &d);
    assert(n == 0 && d == 1);
    duration_destroy(zero);
    metronome_marking_destroy(m);
}

static void test_equality(void) {
    MetronomeMarking *a = marking(1, 4, 60, 1);
    MetronomeMarking *same = marking(2, 8, 120, 2); /* same value, unreduced inputs */
    MetronomeMarking *other_rate = marking(1, 4, 72, 1);
    MetronomeMarking *other_unit = marking(1, 8, 60, 1);

    assert(metronome_marking_equals(a, same));
    assert(!metronome_marking_equals(a, other_rate));
    assert(!metronome_marking_equals(a, other_unit));
    assert(!metronome_marking_equals(a, NULL));

    metronome_marking_destroy(a);
    metronome_marking_destroy(same);
    metronome_marking_destroy(other_rate);
    metronome_marking_destroy(other_unit);
}

int main(void) {
    test_create_rejects_invalid();
    test_accessors_reduce_to_lowest_terms();
    test_quarter_equals_sixty_is_one_second();
    test_other_beat_units_and_rates();
    test_equality();
    return 0;
}
