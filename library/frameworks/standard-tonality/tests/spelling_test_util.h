#ifndef URSATZ_STANDARD_TONALITY_TESTS_SPELLING_TEST_UTIL_H
#define URSATZ_STANDARD_TONALITY_TESTS_SPELLING_TEST_UTIL_H

#include <assert.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"

/* Test helpers shared by the standard-tonality tests. */

static inline PitchSpelling *spell(PitchLetter letter, int accidental, int octave) {
    PitchSpelling *spelling = NULL;
    assert(pitch_spelling_create(letter, accidental, octave, &spelling) == PITCH_SPELLING_OK);
    return spelling;
}

#endif
