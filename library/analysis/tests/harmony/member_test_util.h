#ifndef URSATZ_ANALYSIS_TESTS_HARMONY_MEMBER_TEST_UTIL_H
#define URSATZ_ANALYSIS_TESTS_HARMONY_MEMBER_TEST_UTIL_H

#include <math.h>

#include "model/core/rational.h"

/* A time written as a decimal of whole notes (0.25, 0.1875, 0.3), read exactly on the 1/960 grid: every literal the
   member tests use is a multiple of it. */
static inline Rational member_whole_notes(double value) {
    return rational_ratio((long long)llround(value * 960.0), 960);
}

#endif
