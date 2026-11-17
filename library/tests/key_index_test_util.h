#ifndef URSATZ_TESTS_KEY_INDEX_TEST_UTIL_H
#define URSATZ_TESTS_KEY_INDEX_TEST_UTIL_H

#include "analysis/harmony/key_regions.h"

/* The key index (0..11 major, 12..23 minor, key_regions.h) of a tonic pitch class and mode, or -1. */
static inline int test_key_index_of(int tonic_pitch_class, CommonPracticeKeyMode mode) {
    if (tonic_pitch_class < 0 || tonic_pitch_class > 11 ||
        (mode != COMMON_PRACTICE_KEY_MAJOR && mode != COMMON_PRACTICE_KEY_MINOR)) {
        return -1;
    }
    return tonic_pitch_class + (mode == COMMON_PRACTICE_KEY_MINOR ? 12 : 0);
}

#endif
