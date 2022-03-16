#ifndef URSATZ_HIGH_BAROQUE_FUGUE_TESTS_FUGUE_TEST_UTIL_H
#define URSATZ_HIGH_BAROQUE_FUGUE_TESTS_FUGUE_TEST_UTIL_H

#include <assert.h>
#include <stddef.h>

#include "fugue_exposition.h"
#include "fugue_framework.h"
#include "pitch/pitch_spelling.h"
#include "standard-counterpoint/counterpoint_slots.h"

/* Test helpers shared by the high-baroque-fugue tests. Notes live in a fixed
   pool for the life of the test process and are never freed. */

#define FUGUE_TEST_POOL_SIZE 4096

static inline const PitchSpelling *note(PitchLetter letter, int accidental, int octave) {
    static PitchSpelling *pool[FUGUE_TEST_POOL_SIZE];
    static int used = 0;
    assert(used < FUGUE_TEST_POOL_SIZE);
    assert(pitch_spelling_create(letter, accidental, octave, &pool[used]) == PITCH_SPELLING_OK);
    return pool[used++];
}

/* P(C, 0, 4) is C natural in octave 4; P(B, -1, 3) is B-flat 3. */
#define P(letter, accidental, octave) note(PITCH_LETTER_##letter, (accidental), (octave))

/* Every test exposition is on an eighth-note grid: a bar is eight slots. */
#define TEST_BAR 8

/* Slot macros: ATK(p) is a new note, TIE(p) a tie to the slot before it,
   REST() a rest. HALF(p) is a note of four slots, QUARTER(p) two, EIGHTH(p)
   one, WHOLE(p) eight. R16() is sixteen rest slots. */
#define ATK(pitch) {STANDARD_COUNTERPOINT_SLOT_ATTACK, (pitch)}
#define TIE(pitch) {STANDARD_COUNTERPOINT_SLOT_TIE, (pitch)}
#define REST() {STANDARD_COUNTERPOINT_SLOT_REST, NULL}

#define T2(pitch) TIE(pitch), TIE(pitch)
#define T3(pitch) TIE(pitch), TIE(pitch), TIE(pitch)
#define T7(pitch) T3(pitch), T3(pitch), TIE(pitch)

#define REST4() REST(), REST(), REST(), REST()
#define R8() REST4(), REST4()
#define R16() R8(), R8()
#define EIGHTH(pitch) ATK(pitch)
#define QUARTER(pitch) ATK(pitch), TIE(pitch)
#define HALF(pitch) ATK(pitch), T3(pitch)
#define WHOLE(pitch) ATK(pitch), T7(pitch)

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

/* A slot line over a fixed array of slots. */
#define LINE_OF(array) {(array), COUNT_OF(array)}

/* An exposition over the slot lines in `voice_array` and the statements in
   `statement_array`, a bar of TEST_BAR, no tonic, not invertible, default
   limits. */
#define EXPOSITION_OF(name, voice_array, statement_array)                                                  \
    FugueExposition name = {(voice_array), COUNT_OF(voice_array), (statement_array),                      \
                            COUNT_OF(statement_array), TEST_BAR, NULL, false, NULL}

#endif
