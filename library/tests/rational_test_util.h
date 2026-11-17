#ifndef URSATZ_TESTS_RATIONAL_TEST_UTIL_H
#define URSATZ_TESTS_RATIONAL_TEST_UTIL_H

#include <assert.h>
#include <stdlib.h>

#include "model/core/rational.h"
#include "model/time/duration.h"
#include "model/time/musical_time.h"

/* The exact value of a time or duration in a test. */
static inline Rational test_rational_of_time(const MusicalTime *time) {
    return time->value;
}

static inline Rational test_rational_of_duration(const Duration *duration) {
    return duration->value;
}

#endif
