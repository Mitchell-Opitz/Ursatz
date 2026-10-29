#ifndef URSATZ_TESTS_RATIONAL_TEST_UTIL_H
#define URSATZ_TESTS_RATIONAL_TEST_UTIL_H

#include <assert.h>
#include <stdlib.h>

#include "core/rational.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/time_rational.h"

/* The exact value of a time or duration in a test; aborts when it does not fit a Rational. */
static inline Rational test_rational_of_time(const MusicalTime *time) {
    Rational r = {0, 1};
    if (rational_of_musical_time(time, &r) != RATIONAL_OK) {
        abort();
    }
    return r;
}

static inline Rational test_rational_of_duration(const Duration *duration) {
    Rational r = {0, 1};
    if (rational_of_duration(duration, &r) != RATIONAL_OK) {
        abort();
    }
    return r;
}

#endif
