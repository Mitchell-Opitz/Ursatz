#ifndef URSATZ_STANDARD_NOTATION_TESTS_SPELLING_TEST_UTIL_H
#define URSATZ_STANDARD_NOTATION_TESTS_SPELLING_TEST_UTIL_H

#include <assert.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "pitch/staff_position.h"

/* Test helpers shared by the standard-notation tests. */

static inline PitchSpelling *spell(PitchLetter letter, int accidental, int octave) {
    PitchSpelling *spelling = NULL;
    assert(pitch_spelling_create(letter, accidental, octave, &spelling) == PITCH_SPELLING_OK);
    return spelling;
}

static inline StaffPosition *staff_at(int steps) {
    StaffPosition *position = NULL;
    assert(staff_position_create(steps, &position) == STAFF_POSITION_OK);
    return position;
}

#endif
