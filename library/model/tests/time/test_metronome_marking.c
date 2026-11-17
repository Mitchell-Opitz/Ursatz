#include "model/time/metronome_marking.h"

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

int main(void) {
    test_create_rejects_invalid();
    test_accessors_reduce_to_lowest_terms();
    return 0;
}
