#include <assert.h>
#include <stdio.h>

#include "tonal_key.h"

/*
 * Key tonic spelling: all 24 keys, the tonic with the fewer accidentals in its signature, sharps on a tie
 * (F# major, D# minor), minor judged by its relative major.
 */

typedef struct {
    PitchLetter letter;
    int accidental;
} Tonic;

#define T(l, a) {PITCH_LETTER_##l, (a)}

static void test_all_keys(void) {
    static const Tonic MAJOR[12] = {T(C, 0), T(D, -1), T(D, 0), T(E, -1), T(E, 0), T(F, 0),
                                    T(F, 1), T(G, 0),  T(A, -1), T(A, 0), T(B, -1), T(B, 0)};
    static const Tonic MINOR[12] = {T(C, 0), T(C, 1), T(D, 0), T(D, 1), T(E, 0), T(F, 0),
                                    T(F, 1), T(G, 0), T(G, 1), T(A, 0), T(B, -1), T(B, 0)};
    for (int minor = 0; minor < 2; minor++) {
        for (int pc = 0; pc < 12; pc++) {
            const Tonic *expected = minor ? &MINOR[pc] : &MAJOR[pc];
            PitchLetter letter = PITCH_LETTER_A;
            int accidental = 9;
            assert(standard_tonality_key_tonic_of_pitch_class(pc, minor != 0, &letter, &accidental));
            assert(letter == expected->letter && accidental == expected->accidental);
            /* The chosen spelling is a real key with a signature of at most 7 accidentals, and the shortest. */
            int fifths = 0;
            assert(standard_tonality_key_fifths(letter, accidental, minor != 0, &fifths));
            assert(fifths >= -7 && fifths <= 7);
        }
    }
}

static void test_ties_take_sharps(void) {
    PitchLetter letter;
    int accidental;
    assert(standard_tonality_key_tonic_of_pitch_class(6, false, &letter, &accidental));
    assert(letter == PITCH_LETTER_F && accidental == 1);
    assert(standard_tonality_key_tonic_of_pitch_class(3, true, &letter, &accidental));
    assert(letter == PITCH_LETTER_D && accidental == 1);
}

static void test_invalid_arguments(void) {
    PitchLetter letter = PITCH_LETTER_G;
    int accidental = 5;
    assert(!standard_tonality_key_tonic_of_pitch_class(-1, false, &letter, &accidental));
    assert(!standard_tonality_key_tonic_of_pitch_class(12, true, &letter, &accidental));
    assert(!standard_tonality_key_tonic_of_pitch_class(0, false, NULL, &accidental));
    assert(!standard_tonality_key_tonic_of_pitch_class(0, false, &letter, NULL));
    assert(letter == PITCH_LETTER_G && accidental == 5);
}

int main(void) {
    test_all_keys();
    test_ties_take_sharps();
    test_invalid_arguments();
    printf("All key tonic spelling tests passed.\n");
    return 0;
}
