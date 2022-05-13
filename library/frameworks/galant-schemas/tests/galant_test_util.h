#ifndef URSATZ_GALANT_SCHEMAS_TESTS_GALANT_TEST_UTIL_H
#define URSATZ_GALANT_SCHEMAS_TESTS_GALANT_TEST_UTIL_H

#include <assert.h>
#include <stddef.h>

#include "galant_exemplar.h"
#include "galant_framework.h"
#include "pitch/pitch_spelling.h"
#include "semantics/beat_strength.h"

/* Test helpers shared by the galant-schemas tests. Notes live in a fixed pool
   for the life of the test process and are never freed. */

#define GALANT_TEST_POOL_SIZE 1024

static inline const PitchSpelling *note(PitchLetter letter, int accidental, int octave) {
    static PitchSpelling *pool[GALANT_TEST_POOL_SIZE];
    static int used = 0;
    assert(used < GALANT_TEST_POOL_SIZE);
    assert(pitch_spelling_create(letter, accidental, octave, &pool[used]) == PITCH_SPELLING_OK);
    return pool[used++];
}

/* P(C, 0, 4) is C natural in octave 4; P(B, -1, 3) is B-flat 3. */
#define P(letter, accidental, octave) note(PITCH_LETTER_##letter, (accidental), (octave))

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

#define S BEAT_STRENGTH_STRONG
#define W BEAT_STRENGTH_WEAK

/* A four-stage exemplar, set up in place with galant_fixture. */
typedef struct {
    GalantExemplarStage stages[4];
    GalantExemplar exemplar;
} GalantFixture;

static inline void galant_fixture(GalantFixture *f, const PitchSpelling *tonic, const PitchSpelling *const *melody,
                                  const PitchSpelling *const *bass, const BeatStrength *strengths) {
    for (size_t i = 0; i < 4; i++) {
        f->stages[i].melody = melody[i];
        f->stages[i].bass = bass[i];
        f->stages[i].strength = strengths[i];
        f->stages[i].melody_next = NULL;
    }
    f->exemplar = (GalantExemplar){f->stages, 4, tonic};
}

/* A Meyer in C: do ti fa mi over do re ti do, on weak, strong, weak, strong. */
static inline void meyer_in_c(GalantFixture *f) {
    const PitchSpelling *melody[4] = {P(C, 0, 5), P(B, 0, 4), P(F, 0, 4), P(E, 0, 4)};
    const PitchSpelling *bass[4] = {P(C, 0, 3), P(D, 0, 3), P(B, 0, 2), P(C, 0, 3)};
    const BeatStrength strengths[4] = {W, S, W, S};
    galant_fixture(f, P(C, 0, 4), melody, bass, strengths);
}

/* A Prinner in C: la sol fa mi over fa mi re do, in parallel tenths, on strong,
   weak, strong, weak. */
static inline void prinner_in_c(GalantFixture *f) {
    const PitchSpelling *melody[4] = {P(A, 0, 4), P(G, 0, 4), P(F, 0, 4), P(E, 0, 4)};
    const PitchSpelling *bass[4] = {P(F, 0, 3), P(E, 0, 3), P(D, 0, 3), P(C, 0, 3)};
    const BeatStrength strengths[4] = {S, W, S, W};
    galant_fixture(f, P(C, 0, 4), melody, bass, strengths);
}

/* A Prinner in A minor: le sol fa me over fa me re do. */
static inline void prinner_in_a_minor(GalantFixture *f) {
    const PitchSpelling *melody[4] = {P(F, 0, 5), P(E, 0, 5), P(D, 0, 5), P(C, 0, 5)};
    const PitchSpelling *bass[4] = {P(D, 0, 4), P(C, 0, 4), P(B, 0, 3), P(A, 0, 3)};
    const BeatStrength strengths[4] = {S, W, S, W};
    galant_fixture(f, P(A, 0, 4), melody, bass, strengths);
}

/* A modulating Prinner in C: mi re do ti over do ti la sol. */
static inline void modulating_prinner_in_c(GalantFixture *f) {
    const PitchSpelling *melody[4] = {P(E, 0, 5), P(D, 0, 5), P(C, 0, 5), P(B, 0, 4)};
    const PitchSpelling *bass[4] = {P(C, 0, 3), P(B, 0, 2), P(A, 0, 2), P(G, 0, 2)};
    const BeatStrength strengths[4] = {S, W, S, W};
    galant_fixture(f, P(C, 0, 4), melody, bass, strengths);
}

#endif
