#include "theory/tonality/tonal_key.h"

#include <assert.h>
#include <stdio.h>

#include "model/pitch/pitch_arithmetic.h"

#define KEY_COUNT 15

static const StandardTonalityMode MODES[4] = {STANDARD_TONALITY_MODE_MAJOR, STANDARD_TONALITY_MODE_NATURAL_MINOR,
                                              STANDARD_TONALITY_MODE_HARMONIC_MINOR,
                                              STANDARD_TONALITY_MODE_MELODIC_MINOR};

/* Tonic of the key with `fifths` sharps (flats negative), from -7 to 7. */
static void tonic_of(int fifths, bool minor, PitchLetter *letter, int *accidental) {
    assert(standard_tonality_key_of_fifths(fifths, minor, letter, accidental));
}

static void test_degree_semitones(void) {
    static const int EXPECTED[4][7] = {{0, 2, 4, 5, 7, 9, 11},
                                       {0, 2, 3, 5, 7, 8, 10},
                                       {0, 2, 3, 5, 7, 8, 11},
                                       {0, 2, 3, 5, 7, 9, 11}};
    for (int m = 0; m < 4; m++) {
        for (int degree = 1; degree <= 7; degree++) {
            assert(standard_tonality_degree_semitone(MODES[m], degree) == EXPECTED[m][degree - 1]);
        }
        assert(standard_tonality_degree_semitone(MODES[m], 0) == -1);
        assert(standard_tonality_degree_semitone(MODES[m], 8) == -1);
    }
    assert(standard_tonality_degree_semitone((StandardTonalityMode)9, 1) == -1);
}

static void test_key_signatures_for_all_thirty_keys(void) {
    static const PitchLetter SHARPS[7] = {PITCH_LETTER_F, PITCH_LETTER_C, PITCH_LETTER_G, PITCH_LETTER_D,
                                          PITCH_LETTER_A, PITCH_LETTER_E, PITCH_LETTER_B};
    int keys = 0;
    for (int minor = 0; minor <= 1; minor++) {
        for (int fifths = -7; fifths <= 7; fifths++) {
            PitchLetter letter;
            int accidental;
            tonic_of(fifths, minor, &letter, &accidental);
            int counted;
            assert(standard_tonality_key_fifths(letter, accidental, minor, &counted));
            assert(counted == fifths);

            KeySignature *signature = NULL;
            assert(standard_tonality_key_signature_of(letter, accidental, minor, &signature));
            size_t size = (size_t)(fifths < 0 ? -fifths : fifths);
            assert(key_signature_entry_count(signature) == size);
            for (size_t i = 0; i < size; i++) {
                if (fifths > 0) {
                    assert(key_signature_letter_at(signature, i) == SHARPS[i]);
                    assert(key_signature_accidental_at(signature, i) == 1);
                } else {
                    assert(key_signature_letter_at(signature, i) == SHARPS[6 - i]);
                    assert(key_signature_accidental_at(signature, i) == -1);
                }
            }
            key_signature_destroy(signature);
            keys++;
        }
    }
    assert(keys == 30);
}

static void test_known_signatures(void) {
    int fifths;
    assert(standard_tonality_key_fifths(PITCH_LETTER_C, 0, false, &fifths) && fifths == 0);
    assert(standard_tonality_key_fifths(PITCH_LETTER_A, 0, true, &fifths) && fifths == 0);
    assert(standard_tonality_key_fifths(PITCH_LETTER_G, 0, false, &fifths) && fifths == 1);
    assert(standard_tonality_key_fifths(PITCH_LETTER_E, 0, true, &fifths) && fifths == 1);
    assert(standard_tonality_key_fifths(PITCH_LETTER_F, 0, true, &fifths) && fifths == -4);
    assert(standard_tonality_key_fifths(PITCH_LETTER_C, -1, false, &fifths) && fifths == -7);
    assert(standard_tonality_key_fifths(PITCH_LETTER_A, 1, true, &fifths) && fifths == 7);
    assert(!standard_tonality_key_fifths(PITCH_LETTER_C, 0, false, NULL));
    assert(!standard_tonality_key_fifths((PitchLetter)42, 0, false, &fifths));
    /* Imaginary keys have a count but no signature. */
    assert(standard_tonality_key_fifths(PITCH_LETTER_G, 1, false, &fifths) && fifths == 8);
    KeySignature *signature = NULL;
    assert(!standard_tonality_key_signature_of(PITCH_LETTER_G, 1, false, &signature));
    assert(!standard_tonality_key_signature_of(PITCH_LETTER_D, -1, true, &signature));
    assert(signature == NULL);
}

static void test_key_of_fifths(void) {
    PitchLetter letter;
    int accidental;
    assert(standard_tonality_key_of_fifths(0, false, &letter, &accidental));
    assert(letter == PITCH_LETTER_C && accidental == 0);
    assert(standard_tonality_key_of_fifths(0, true, &letter, &accidental));
    assert(letter == PITCH_LETTER_A && accidental == 0);
    assert(standard_tonality_key_of_fifths(-3, false, &letter, &accidental));
    assert(letter == PITCH_LETTER_E && accidental == -1);
    assert(standard_tonality_key_of_fifths(6, true, &letter, &accidental));
    assert(letter == PITCH_LETTER_D && accidental == 1);
    assert(!standard_tonality_key_of_fifths(0, false, NULL, &accidental));
    for (int minor = 0; minor <= 1; minor++) {
        for (int fifths = -12; fifths <= 12; fifths++) {
            int back;
            tonic_of(fifths, minor, &letter, &accidental);
            assert(standard_tonality_key_fifths(letter, accidental, minor, &back) && back == fifths);
        }
    }
}

static void test_enharmonic_keys(void) {
    static const int PAIRS[3][2] = {{5, -7}, {6, -6}, {7, -5}};
    for (int minor = 0; minor <= 1; minor++) {
        for (int p = 0; p < 3; p++) {
            PitchLetter sharp_letter;
            int sharp_accidental;
            PitchLetter flat_letter;
            int flat_accidental;
            tonic_of(PAIRS[p][0], minor, &sharp_letter, &sharp_accidental);
            tonic_of(PAIRS[p][1], minor, &flat_letter, &flat_accidental);
            assert(pitch_class_of_letter(sharp_letter, sharp_accidental) ==
                   pitch_class_of_letter(flat_letter, flat_accidental));
            assert(sharp_letter != flat_letter);
            int sharp_fifths;
            int flat_fifths;
            assert(standard_tonality_key_fifths(sharp_letter, sharp_accidental, minor, &sharp_fifths));
            assert(standard_tonality_key_fifths(flat_letter, flat_accidental, minor, &flat_fifths));
            assert(sharp_fifths - flat_fifths == 12);
        }
    }
}

int main(void) {
    test_degree_semitones();
    test_key_signatures_for_all_thirty_keys();
    test_known_signatures();
    test_key_of_fifths();
    test_enharmonic_keys();
    printf("test_tonal_key: ok\n");
    return 0;
}
