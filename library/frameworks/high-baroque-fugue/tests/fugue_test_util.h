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

/*
 * A four-voice exposition of 64 slots on the tonic C (soprano, alto, tenor,
 * bass from the highest voice), entered in the order alto, soprano, tenor,
 * bass: a subject C D E C, a real answer G A B G a fifth up, and so on, each
 * statement four half notes in two bars. Each entry is followed by a
 * countersubject in the voice that has just stated: CS1 (eight quarters) in
 * the alto against the answer, and CS2 in the soprano and then the tenor, an
 * octave and a half apart in register. It breaks no rule. Declares `name`,
 * and `name##_voices`, `name##_statements` (seven statements in an array of
 * eight, the eighth a spare redundant subject in the alto), `name##_alto`
 * and the pitches `name##_d4` and `name##_f4` for tests that change a note.
 */
#define FOUR_VOICE_FUGUE(name)                                                                              \
    const PitchSpelling *name##_c5 = P(C, 0, 5);                                                            \
    const PitchSpelling *name##_g4 = P(G, 0, 4);                                                            \
    const PitchSpelling *name##_a4 = P(A, 0, 4);                                                            \
    const PitchSpelling *name##_b4 = P(B, 0, 4);                                                            \
    const PitchSpelling *name##_f4 = P(F, 0, 4);                                                            \
    const PitchSpelling *name##_e4 = P(E, 0, 4);                                                            \
    const PitchSpelling *name##_d4 = P(D, 0, 4);                                                            \
    const PitchSpelling *name##_c4 = P(C, 0, 4);                                                            \
    const PitchSpelling *name##_c3 = P(C, 0, 3);                                                            \
    const PitchSpelling *name##_d3 = P(D, 0, 3);                                                            \
    const PitchSpelling *name##_e3 = P(E, 0, 3);                                                            \
    const PitchSpelling *name##_a2 = P(A, 0, 2);                                                            \
    const PitchSpelling *name##_b2 = P(B, 0, 2);                                                            \
    const PitchSpelling *name##_g2 = P(G, 0, 2);                                                            \
    const PitchSpelling *name##_f2 = P(F, 0, 2);                                                            \
    const PitchSpelling *name##_e2 = P(E, 0, 2);                                                            \
    StandardCounterpointSlot name##_soprano[] = {                                                           \
        R16(), HALF(name##_g4), HALF(name##_a4), HALF(name##_b4), HALF(name##_g4), QUARTER(name##_g4),      \
        QUARTER(name##_a4), QUARTER(name##_g4), QUARTER(name##_f4), QUARTER(name##_e4), QUARTER(name##_f4), \
        QUARTER(name##_g4), QUARTER(name##_a4), HALF(name##_c5), HALF(name##_c5), HALF(name##_c5),          \
        HALF(name##_c5)};                                                                                   \
    StandardCounterpointSlot name##_alto[] = {                                                              \
        HALF(name##_c4), HALF(name##_d4), HALF(name##_e4), HALF(name##_c4), QUARTER(name##_e4),             \
        QUARTER(name##_f4), QUARTER(name##_f4), QUARTER(name##_g4), QUARTER(name##_g4), QUARTER(name##_a4), \
        QUARTER(name##_e4), QUARTER(name##_e4), HALF(name##_c4), HALF(name##_c4), HALF(name##_c4),          \
        HALF(name##_c4), HALF(name##_c4), HALF(name##_c4), HALF(name##_c4), HALF(name##_c4)};               \
    StandardCounterpointSlot name##_tenor[] = {                                                             \
        R16(), R16(), HALF(name##_c3), HALF(name##_d3), HALF(name##_e3), HALF(name##_c3),                   \
        QUARTER(name##_g2), QUARTER(name##_a2), QUARTER(name##_g2), QUARTER(name##_f2),                     \
        QUARTER(name##_e2), QUARTER(name##_f2), QUARTER(name##_g2), QUARTER(name##_a2)};                    \
    StandardCounterpointSlot name##_bass[] = {R16(), R16(), R16(), HALF(name##_g2), HALF(name##_a2),        \
                                              HALF(name##_b2), HALF(name##_g2)};                            \
    StandardCounterpointSlotLine name##_voices[] = {LINE_OF(name##_soprano), LINE_OF(name##_alto),          \
                                                    LINE_OF(name##_tenor), LINE_OF(name##_bass)};           \
    FugueStatement name##_statements[] = {                                                                  \
        {1, 0, FUGUE_ROLE_SUBJECT, 4, 0},        {0, 16, FUGUE_ROLE_ANSWER, 4, 0},                          \
        {1, 16, FUGUE_ROLE_COUNTERSUBJECT, 8, 0}, {2, 32, FUGUE_ROLE_SUBJECT, 4, 0},                        \
        {0, 32, FUGUE_ROLE_COUNTERSUBJECT, 8, 1}, {3, 48, FUGUE_ROLE_ANSWER, 4, 0},                         \
        {2, 48, FUGUE_ROLE_COUNTERSUBJECT, 8, 1}, {1, 48, FUGUE_ROLE_SUBJECT, 4, 0}};                       \
    FugueExposition name = {name##_voices, 4, name##_statements, 7, TEST_BAR, name##_c4, false, NULL}

/*
 * A subject and an answer in two voices, for the answer rules: the subject
 * (n half notes) in the upper voice from the first slot, and the answer in the
 * lower voice after it. `answer_lengths` gives the answer's note lengths in
 * slots, or NULL for half notes. Set up in place with answer_fixture.
 */
typedef struct {
    StandardCounterpointSlot upper[256];
    StandardCounterpointSlot lower[256];
    StandardCounterpointSlotLine voices[2];
    FugueStatement statements[2];
    FugueExposition exposition;
} AnswerFixture;

static inline void answer_fixture(AnswerFixture *f, const PitchSpelling *const *subject,
                                  const PitchSpelling *const *answer, size_t n, const size_t *answer_lengths,
                                  const PitchSpelling *tonic) {
    size_t subject_total = 4 * n;
    size_t answer_total = 0;
    for (size_t i = 0; i < n; i++) {
        answer_total += answer_lengths != NULL ? answer_lengths[i] : 4;
    }
    size_t total = subject_total + answer_total;
    assert(total <= 256);

    for (size_t s = 0; s < total; s++) {
        f->upper[s].kind = STANDARD_COUNTERPOINT_SLOT_REST;
        f->upper[s].pitch = NULL;
        f->lower[s] = f->upper[s];
    }
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < 4; j++) {
            f->upper[4 * i + j].kind = j == 0 ? STANDARD_COUNTERPOINT_SLOT_ATTACK : STANDARD_COUNTERPOINT_SLOT_TIE;
            f->upper[4 * i + j].pitch = subject[i];
        }
    }
    size_t s = subject_total;
    for (size_t i = 0; i < n; i++) {
        size_t length = answer_lengths != NULL ? answer_lengths[i] : 4;
        for (size_t j = 0; j < length; j++) {
            f->lower[s].kind = j == 0 ? STANDARD_COUNTERPOINT_SLOT_ATTACK : STANDARD_COUNTERPOINT_SLOT_TIE;
            f->lower[s].pitch = answer[i];
            s++;
        }
    }
    f->voices[0].slots = f->upper;
    f->voices[0].count = total;
    f->voices[1].slots = f->lower;
    f->voices[1].count = total;
    f->statements[0] = (FugueStatement){0, 0, FUGUE_ROLE_SUBJECT, n, 0};
    f->statements[1] = (FugueStatement){1, subject_total, FUGUE_ROLE_ANSWER, n, 0};
    f->exposition = (FugueExposition){f->voices, 2, f->statements, 2, TEST_BAR, tonic, false, NULL};
}

#endif
