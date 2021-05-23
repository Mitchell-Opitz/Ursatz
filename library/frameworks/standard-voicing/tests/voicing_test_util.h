#ifndef URSATZ_STANDARD_VOICING_TESTS_VOICING_TEST_UTIL_H
#define URSATZ_STANDARD_VOICING_TESTS_VOICING_TEST_UTIL_H

#include <assert.h>

#include "pitch/pitch_spelling.h"
#include "voicing_chord.h"

/* Test helpers shared by the standard-voicing tests. Notes live in a
   fixed pool for the life of the test process and are never freed. */

#define VOICING_TEST_POOL_SIZE 256

static inline const PitchSpelling *note(PitchLetter letter, int accidental, int octave) {
    static PitchSpelling *pool[VOICING_TEST_POOL_SIZE];
    static int used = 0;
    assert(used < VOICING_TEST_POOL_SIZE);
    assert(pitch_spelling_create(letter, accidental, octave, &pool[used]) == PITCH_SPELLING_OK);
    return pool[used++];
}

/* P(C, 0, 4) is C natural in octave 4; P(B, -1, 3) is B-flat 3. */
#define P(letter, accidental, octave) note(PITCH_LETTER_##letter, (accidental), (octave))

static inline StandardVoicingChord make_chord(const PitchSpelling *s, const PitchSpelling *a, const PitchSpelling *t,
                                              const PitchSpelling *b) {
    StandardVoicingChord chord = {{s, a, t, b}};
    return chord;
}

#endif
