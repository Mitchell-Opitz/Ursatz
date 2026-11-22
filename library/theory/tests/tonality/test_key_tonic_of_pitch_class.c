#include <assert.h>
#include <stdio.h>

#include "theory/tonality/tonal_key.h"
#include "ursatz_test_knowledge.h"

/*
 * Key tonic spelling: all 24 keys, the tonic with the fewer accidentals in its signature, sharps on a tie
 * (F# major, D# minor), minor judged by its relative major.
 */

typedef struct {
    PitchLetter letter;
    int accidental;
} Tonic;

/* The key of a major or a natural minor material, as `minor` says. */
static const PitchMaterial *key_material(int minor) {
    return minor ? test_materials()->natural_minor : test_materials()->major;
}

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
            assert(standard_tonality_key_tonic_of_pitch_class_material(pc, key_material(minor), &letter, &accidental));
            assert(letter == expected->letter && accidental == expected->accidental);
            /* The chosen spelling is a real key with a signature of at most 7 accidentals, and the shortest. */
            int fifths = 0;
            assert(standard_tonality_key_fifths_of(letter, accidental, key_material(minor), &fifths));
            assert(fifths >= -7 && fifths <= 7);
        }
    }
}

static void test_ties_take_sharps(void) {
    PitchLetter letter;
    int accidental;
    assert(standard_tonality_key_tonic_of_pitch_class_material(6, key_material(0), &letter, &accidental));
    assert(letter == PITCH_LETTER_F && accidental == 1);
    assert(standard_tonality_key_tonic_of_pitch_class_material(3, key_material(1), &letter, &accidental));
    assert(letter == PITCH_LETTER_D && accidental == 1);
}

static void test_invalid_arguments(void) {
    PitchLetter letter = PITCH_LETTER_G;
    int accidental = 5;
    assert(!standard_tonality_key_tonic_of_pitch_class_material(-1, key_material(0), &letter, &accidental));
    assert(!standard_tonality_key_tonic_of_pitch_class_material(12, key_material(1), &letter, &accidental));
    assert(!standard_tonality_key_tonic_of_pitch_class_material(0, key_material(0), NULL, &accidental));
    assert(!standard_tonality_key_tonic_of_pitch_class_material(0, key_material(0), &letter, NULL));
    assert(letter == PITCH_LETTER_G && accidental == 5);
}

int main(void) {
    test_all_keys();
    test_ties_take_sharps();
    test_invalid_arguments();
    printf("All key tonic spelling tests passed.\n");
    return 0;
}
