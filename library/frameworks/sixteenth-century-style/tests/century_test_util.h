#ifndef URSATZ_SIXTEENTH_CENTURY_STYLE_TESTS_CENTURY_TEST_UTIL_H
#define URSATZ_SIXTEENTH_CENTURY_STYLE_TESTS_CENTURY_TEST_UTIL_H

#include <assert.h>
#include <stddef.h>

#include "century_framework.h"
#include "century_score.h"
#include "pitch/pitch_spelling.h"
#include "standard-counterpoint/counterpoint_slots.h"

/* Test helpers shared by the sixteenth-century-style tests. Notes live in a
   fixed pool for the life of the test process and are never freed. */

#define CENTURY_TEST_POOL_SIZE 4096

static inline const PitchSpelling *note(PitchLetter letter, int accidental, int octave) {
    static PitchSpelling *pool[CENTURY_TEST_POOL_SIZE];
    static int used = 0;
    assert(used < CENTURY_TEST_POOL_SIZE);
    assert(pitch_spelling_create(letter, accidental, octave, &pool[used]) == PITCH_SPELLING_OK);
    return pool[used++];
}

/* P(C, 0, 4) is C natural in octave 4; P(B, -1, 3) is B-flat 3. */
#define P(letter, accidental, octave) note(PITCH_LETTER_##letter, (accidental), (octave))

/* The tactus is four slots in every test score: slots on an eighth-note grid,
   so a tactus is a half note, and two make a bar of eight slots. */
#define TEST_TACTUS 4

/* Slot macros: ATK(p) is a new note, TIE(p) a tie to the slot before it,
   REST() a rest. HALF(p) is a note of four slots (one tactus), QUARTER(p) two,
   EIGHTH(p) one, WHOLE(p) eight. */
#define ATK(pitch) {STANDARD_COUNTERPOINT_SLOT_ATTACK, (pitch)}
#define TIE(pitch) {STANDARD_COUNTERPOINT_SLOT_TIE, (pitch)}
#define REST() {STANDARD_COUNTERPOINT_SLOT_REST, NULL}

#define T2(pitch) TIE(pitch), TIE(pitch)
#define T3(pitch) TIE(pitch), TIE(pitch), TIE(pitch)
#define T7(pitch) T3(pitch), T3(pitch), TIE(pitch)

#define REST4() REST(), REST(), REST(), REST()
#define EIGHTH(pitch) ATK(pitch)
#define QUARTER(pitch) ATK(pitch), TIE(pitch)
#define HALF(pitch) ATK(pitch), T3(pitch)
#define WHOLE(pitch) ATK(pitch), T7(pitch)

/* A voice of half notes: fills `buffer` (room for 4 * n slots) with a half
   note for each pitch and returns the slot line over it. */
static inline StandardCounterpointSlotLine halves(StandardCounterpointSlot *buffer, const PitchSpelling *const *pitches,
                                                  size_t n) {
    for (size_t i = 0; i < n; i++) {
        buffer[4 * i].kind = STANDARD_COUNTERPOINT_SLOT_ATTACK;
        buffer[4 * i].pitch = pitches[i];
        for (size_t j = 1; j < 4; j++) {
            buffer[4 * i + j].kind = STANDARD_COUNTERPOINT_SLOT_TIE;
            buffer[4 * i + j].pitch = pitches[i];
        }
    }
    StandardCounterpointSlotLine line = {buffer, 4 * n};
    return line;
}

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

/* A score of the voices in `voice_array` (slot lines), a tactus of
   TEST_TACTUS, no tonic. */
#define SCORE_OF(name, voice_array) \
    SixteenthCenturyScore name = {(voice_array), COUNT_OF(voice_array), TEST_TACTUS, NULL, false}

/* A one-voice score of half notes named `name` over the pitch array `array`,
   with the given tonic (or NULL) and phrygian flag. Declares `name##_slots`
   and `name##_voice` beside it. */
#define MELODY(name, array, tonic_pitch, phrygian_flag)                                         \
    StandardCounterpointSlot name##_slots[4 * COUNT_OF(array)];                                  \
    StandardCounterpointSlotLine name##_voice = halves(name##_slots, (array), COUNT_OF(array)); \
    SixteenthCenturyScore name = {&name##_voice, 1, TEST_TACTUS, (tonic_pitch), (phrygian_flag)}

#endif
