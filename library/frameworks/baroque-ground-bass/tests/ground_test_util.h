#ifndef URSATZ_BAROQUE_GROUND_BASS_TESTS_GROUND_TEST_UTIL_H
#define URSATZ_BAROQUE_GROUND_BASS_TESTS_GROUND_TEST_UTIL_H

#include <assert.h>
#include <stddef.h>

#include "ground_framework.h"
#include "ground_piece.h"
#include "pitch/pitch_spelling.h"
#include "standard-counterpoint/counterpoint_slots.h"

/* Test helpers shared by the baroque-ground-bass tests. Notes live in a fixed
   pool for the life of the test process and are never freed. */

#define GROUND_TEST_POOL_SIZE 4096

static inline const PitchSpelling *note(PitchLetter letter, int accidental, int octave) {
    static PitchSpelling *pool[GROUND_TEST_POOL_SIZE];
    static int used = 0;
    assert(used < GROUND_TEST_POOL_SIZE);
    assert(pitch_spelling_create(letter, accidental, octave, &pool[used]) == PITCH_SPELLING_OK);
    return pool[used++];
}

/* P(C, 0, 4) is C natural in octave 4; P(B, -1, 3) is B-flat 3. */
#define P(letter, accidental, octave) note(PITCH_LETTER_##letter, (accidental), (octave))

#define COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))

/* The grid is eighth notes in 3/4: a bar is six slots, a beat two. */
#define TEST_BAR 6
#define TEST_BEATS 3
#define TEST_ITERATION 24

/* Sets `length` slots from `start` to one note: an attack and its ties. */
static inline void put(StandardCounterpointSlot *slots, size_t start, size_t length, const PitchSpelling *pitch) {
    for (size_t j = 0; j < length; j++) {
        slots[start + j].kind = j == 0 ? STANDARD_COUNTERPOINT_SLOT_ATTACK : STANDARD_COUNTERPOINT_SLOT_TIE;
        slots[start + j].pitch = pitch;
    }
}

/*
 * A ground bass in G minor shaped like a trio sonata: two upper voices over a
 * bass, four iterations of a four-bar ground (a descending tetrachord, G F
 * E-flat D, a dotted half note each), 96 slots in all. In every iteration one
 * upper voice leads with a three-note motive in quarter notes and the other
 * follows a bar (six slots, three beats) later; the voices swap the lead from
 * one iteration to the next. The motive is different in each iteration, and in
 * the third a bar of eighth notes brings in a shorter note value. Voice 0 and 1
 * are the upper voices and voice 2 is the bass. The piece breaks no rule.
 * Set up in place with ground_fixture.
 */
#define FIXTURE_SLOTS 96
#define FIXTURE_STATEMENTS 8

typedef struct {
    StandardCounterpointSlot slots[3][FIXTURE_SLOTS];
    StandardCounterpointSlotLine voices[3];
    GroundStatement statements[FIXTURE_STATEMENTS];
    GroundBassPiece piece;
} GroundFixture;

static inline void ground_fixture(GroundFixture *f) {
    const PitchSpelling *bass[4] = {P(G, 0, 2), P(F, 0, 2), P(E, -1, 2), P(D, 0, 2)};
    const PitchSpelling *motive[4][3] = {{P(G, 0, 4), P(A, 0, 4), P(B, -1, 4)},
                                         {P(D, 0, 5), P(C, 0, 5), P(B, -1, 4)},
                                         {P(B, -1, 4), P(C, 0, 5), P(D, 0, 5)},
                                         {P(G, 0, 4), P(F, 0, 4), P(E, -1, 4)}};
    const PitchSpelling *filler[4] = {P(C, 0, 5), P(B, -1, 4), P(G, 0, 4), P(A, 0, 4)};
    const PitchSpelling *eighths[2] = {P(G, 0, 4), P(A, 0, 4)};

    for (size_t k = 0; k < 4; k++) {
        size_t base = TEST_ITERATION * k;
        size_t lead = k % 2;
        size_t follow = 1 - lead;
        for (size_t b = 0; b < 4; b++) {
            put(f->slots[2], base + TEST_BAR * b, TEST_BAR, bass[b]);
        }
        for (size_t i = 0; i < 3; i++) {
            put(f->slots[lead], base + 2 * i, 2, motive[k][i]);
            put(f->slots[follow], base + TEST_BAR + 2 * i, 2, motive[k][i]);
        }
        put(f->slots[follow], base, TEST_BAR, filler[k]);
        put(f->slots[follow], base + 2 * TEST_BAR, TEST_BAR, filler[k]);
        put(f->slots[follow], base + 3 * TEST_BAR, TEST_BAR, filler[k]);
        for (size_t b = 1; b < 4; b++) {
            if (k == 2 && b == 1) {
                for (size_t e = 0; e < TEST_BAR; e++) {
                    put(f->slots[lead], base + TEST_BAR * b + e, 1, eighths[e % 2]);
                }
            } else {
                put(f->slots[lead], base + TEST_BAR * b, TEST_BAR, filler[k]);
            }
        }
        f->statements[2 * k] = (GroundStatement){lead, base, IMITATIVE_ROLE_DUX, 3};
        f->statements[2 * k + 1] = (GroundStatement){follow, base + TEST_BAR, IMITATIVE_ROLE_COMES, 3};
    }
    for (size_t v = 0; v < 3; v++) {
        f->voices[v].slots = f->slots[v];
        f->voices[v].count = FIXTURE_SLOTS;
    }
    f->piece = (GroundBassPiece){f->voices, 3, 2, TEST_ITERATION, TEST_BAR, TEST_BEATS, OSTINATO_TYPE_GROUND_BASS,
                                 f->statements, FIXTURE_STATEMENTS, NULL};
}

#endif
