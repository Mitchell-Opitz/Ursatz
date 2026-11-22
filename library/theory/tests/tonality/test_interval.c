#include <assert.h>
#include <stdio.h>

#include "theory/tonality/interval.h"

/* The helpers that read an interval from a size or from semitones alone. */

static void test_interval_of_size(void) {
    StandardTonalityInterval interval = standard_tonality_interval_of_size(1);
    assert(interval.size == 1 && interval.simple_size == 1 && interval.quality == STANDARD_TONALITY_INTERVAL_QUALITY_PERFECT);
    interval = standard_tonality_interval_of_size(2);
    assert(interval.size == 2 && interval.simple_size == 2);
    interval = standard_tonality_interval_of_size(8);
    assert(interval.size == 8 && interval.simple_size == 8);
    interval = standard_tonality_interval_of_size(9);
    assert(interval.size == 9 && interval.simple_size == 2);
    interval = standard_tonality_interval_of_size(12);
    assert(interval.size == 12 && interval.simple_size == 5);
    interval = standard_tonality_interval_of_size(15);
    assert(interval.size == 15 && interval.simple_size == 8);
    interval = standard_tonality_interval_of_size(0);
    assert(interval.size == 1 && interval.simple_size == 1);
}

static void test_generic_size_of_semitones(void) {
    static const int EXPECTED[13] = {1, 2, 2, 3, 3, 4, 4, 5, 6, 6, 7, 7, 8};
    for (long semitones = 0; semitones <= 12; semitones++) {
        assert(standard_tonality_generic_size_of_semitones(semitones) == EXPECTED[semitones]);
        assert(standard_tonality_generic_size_of_semitones(-semitones) == EXPECTED[semitones]);
    }
    assert(standard_tonality_generic_size_of_semitones(19) == 12);
    assert(standard_tonality_generic_size_of_semitones(24) == 15);
}

/* A step is a second; a leap is a third or larger, compound included; a unison is neither. */
static void test_step_and_leap(void) {
    for (int size = 1; size <= 15; size++) {
        const StandardTonalityInterval interval = standard_tonality_interval_of_size(size);
        assert(standard_tonality_interval_is_step(&interval) == (size == 2));
        assert(standard_tonality_interval_is_leap(&interval) == (size >= 3));
    }
    assert(!standard_tonality_interval_is_step(NULL) && !standard_tonality_interval_is_leap(NULL));
}

int main(void) {
    test_step_and_leap();
    test_interval_of_size();
    test_generic_size_of_semitones();
    printf("test_interval: ok\n");
    return 0;
}
