#ifndef URSATZ_FRAMEWORKS_STANDARD_METER_TESTS_METER_TEST_UTIL_H
#define URSATZ_FRAMEWORKS_STANDARD_METER_TESTS_METER_TEST_UTIL_H

#include <assert.h>

#include "time/duration.h"
#include "time/note_value.h"
#include "time/time_signature.h"

/* beats over the beat unit with the given NoteValue exponent: (3, 2) is 3/4. */
static inline TimeSignature *make_signature(unsigned beats, int beat_unit_exponent) {
    NoteValue *beat_unit = NULL;
    TimeSignature *time_signature = NULL;
    assert(note_value_create(beat_unit_exponent, &beat_unit) == NOTE_VALUE_OK);
    assert(time_signature_create(beats, beat_unit, &time_signature) == TIME_SIGNATURE_OK);
    note_value_destroy(beat_unit);
    return time_signature;
}

static inline Duration *make_duration(long numerator, unsigned long denominator) {
    Duration *duration = NULL;
    assert(duration_create(numerator, denominator, &duration) == DURATION_OK);
    return duration;
}

static inline void assert_duration(const Duration *duration, long numerator, unsigned long denominator) {
    assert(duration_numerator(duration) == numerator);
    assert(duration_denominator(duration) == denominator);
}

#endif
