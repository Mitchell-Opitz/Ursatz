#ifndef URSATZ_SCALE_HARMONIZATION_TESTS_SCALE_TEST_UTIL_H
#define URSATZ_SCALE_HARMONIZATION_TESTS_SCALE_TEST_UTIL_H

#include <assert.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "scale_framework.h"
#include "scale_harmonization.h"

/* Test helpers shared by the scale-harmonization tests. Notes live in a fixed
   pool for the life of the test process and are never freed. */

#define SCALE_TEST_POOL_SIZE 2048

static inline const PitchSpelling *note(PitchLetter letter, int accidental, int octave) {
    static PitchSpelling *pool[SCALE_TEST_POOL_SIZE];
    static int used = 0;
    assert(used < SCALE_TEST_POOL_SIZE);
    assert(pitch_spelling_create(letter, accidental, octave, &pool[used]) == PITCH_SPELLING_OK);
    return pool[used++];
}

/* P(C, 0, 4) is C natural in octave 4; P(B, -1, 3) is B-flat 3. */
#define P(letter, accidental, octave) note(PITCH_LETTER_##letter, (accidental), (octave))

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

/* A harmonization over stages kept beside it, set up in place. */
typedef struct {
    ScaleStage stages[SCALE_MAX_STAGES];
    ScaleHarmonization harmonization;
} ScaleFixture;

/* A bass and one upper voice, a stage for each pair of pitches. */
static inline void pair_fixture(ScaleFixture *f, ScaleDirection direction, const PitchSpelling *tonic,
                                const PitchSpelling *const *bass, const PitchSpelling *const *upper, size_t count) {
    for (size_t i = 0; i < count; i++) {
        f->stages[i] = (ScaleStage){{bass[i], upper[i]}};
    }
    f->harmonization = (ScaleHarmonization){f->stages, count, 2, direction, tonic};
}

/* Eight chords of four voices: the bass and three upper voices from the lowest. */
static inline void octave_fixture(ScaleFixture *f, ScaleDirection direction, const PitchSpelling *tonic,
                                  const PitchSpelling *const *bass, const PitchSpelling *const (*chords)[3]) {
    for (size_t i = 0; i < 8; i++) {
        f->stages[i] = (ScaleStage){{bass[i], chords[i][0], chords[i][1], chords[i][2]}};
    }
    f->harmonization = (ScaleHarmonization){f->stages, 8, 4, direction, tonic};
}

#endif
