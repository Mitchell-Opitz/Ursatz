#ifndef URSATZ_STANDARD_COUNTERPOINT_TESTS_COUNTERPOINT_TEST_UTIL_H
#define URSATZ_STANDARD_COUNTERPOINT_TESTS_COUNTERPOINT_TEST_UTIL_H

#include <assert.h>
#include <stddef.h>

#include "counterpoint_line.h"
#include "pitch/pitch_spelling.h"

/* Test helpers shared by the standard-counterpoint tests. Notes live in a
   fixed pool for the life of the test process and are never freed. */

#define COUNTERPOINT_TEST_POOL_SIZE 1024

static inline const PitchSpelling *note(PitchLetter letter, int accidental, int octave) {
    static PitchSpelling *pool[COUNTERPOINT_TEST_POOL_SIZE];
    static int used = 0;
    assert(used < COUNTERPOINT_TEST_POOL_SIZE);
    assert(pitch_spelling_create(letter, accidental, octave, &pool[used]) == PITCH_SPELLING_OK);
    return pool[used++];
}

/* P(C, 0, 4) is C natural in octave 4; P(B, -1, 3) is B-flat 3. */
#define P(letter, accidental, octave) note(PITCH_LETTER_##letter, (accidental), (octave))

/* A line over a fixed array of notes: `const PitchSpelling *n[] = {...};
   StandardCounterpointLine line = LINE_OF(n);` */
#define LINE_OF(array) {(array), sizeof(array) / sizeof((array)[0])}

#endif
