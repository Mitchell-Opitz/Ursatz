#include "tonal_key.h"

#include <assert.h>
#include <stdio.h>

#include "pitch/pitch_arithmetic.h"

#define KEY_COUNT 15

static const StandardTonalityMode MODES[4] = {STANDARD_TONALITY_MODE_MAJOR, STANDARD_TONALITY_MODE_NATURAL_MINOR,
                                              STANDARD_TONALITY_MODE_HARMONIC_MINOR,
                                              STANDARD_TONALITY_MODE_MELODIC_MINOR};

static bool mode_is_minor(StandardTonalityMode mode) {
    return mode != STANDARD_TONALITY_MODE_MAJOR;
}

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

static void test_spell_and_degree_in_every_key_and_mode(void) {
    for (int m = 0; m < 4; m++) {
        bool minor = mode_is_minor(MODES[m]);
        for (int fifths = -7; fifths <= 7; fifths++) {
            PitchLetter tonic_letter;
            int tonic_accidental;
            tonic_of(fifths, minor, &tonic_letter, &tonic_accidental);
            for (int degree = 1; degree <= 7; degree++) {
                PitchLetter letter;
                int accidental;
                assert(standard_tonality_spell_degree(tonic_letter, tonic_accidental, MODES[m], degree, 0, &letter,
                                                      &accidental));
                assert(letter == pitch_letter_offset(tonic_letter, degree - 1));
                int pc = pitch_class_of_letter(letter, accidental);
                int expected = (pitch_class_of_letter(tonic_letter, tonic_accidental) +
                                standard_tonality_degree_semitone(MODES[m], degree)) % 12;
                assert(pc == expected);

                int found;
                int alteration;
                assert(standard_tonality_degree_of(tonic_letter, tonic_accidental, MODES[m], letter, accidental,
                                                   &found, &alteration));
                assert(found == degree && alteration == 0);
                assert(standard_tonality_scale_degree_in_mode(tonic_letter, tonic_accidental, MODES[m], letter,
                                                              accidental) == degree);

                /* Raising by one semitone: the same letter, one more accidental. */
                PitchLetter raised_letter;
                int raised_accidental;
                if (standard_tonality_spell_degree(tonic_letter, tonic_accidental, MODES[m], degree, 1,
                                                   &raised_letter, &raised_accidental)) {
                    assert(raised_letter == letter && raised_accidental == accidental + 1);
                    assert(standard_tonality_degree_of(tonic_letter, tonic_accidental, MODES[m], raised_letter,
                                                       raised_accidental, &found, &alteration));
                    assert(found == degree && alteration == 1);
                    assert(standard_tonality_scale_degree_in_mode(tonic_letter, tonic_accidental, MODES[m],
                                                                  raised_letter, raised_accidental) == 0);
                }
            }
        }
    }
}

static void test_known_spellings(void) {
    PitchLetter letter;
    int accidental;
    /* C major: F# is the raised fourth. */
    assert(standard_tonality_degree_of(PITCH_LETTER_C, 0, STANDARD_TONALITY_MODE_MAJOR, PITCH_LETTER_F, 1, &(int){0},
                                       &accidental));
    assert(accidental == 1);
    /* A minor: the seventh is G natural, G# in harmonic and melodic. */
    assert(standard_tonality_spell_degree(PITCH_LETTER_A, 0, STANDARD_TONALITY_MODE_NATURAL_MINOR, 7, 0, &letter,
                                          &accidental));
    assert(letter == PITCH_LETTER_G && accidental == 0);
    assert(standard_tonality_spell_degree(PITCH_LETTER_A, 0, STANDARD_TONALITY_MODE_HARMONIC_MINOR, 7, 0, &letter,
                                          &accidental));
    assert(letter == PITCH_LETTER_G && accidental == 1);
    assert(standard_tonality_spell_degree(PITCH_LETTER_A, 0, STANDARD_TONALITY_MODE_HARMONIC_MINOR, 6, 0, &letter,
                                          &accidental));
    assert(letter == PITCH_LETTER_F && accidental == 0);
    assert(standard_tonality_spell_degree(PITCH_LETTER_A, 0, STANDARD_TONALITY_MODE_MELODIC_MINOR, 6, 0, &letter,
                                          &accidental));
    assert(letter == PITCH_LETTER_F && accidental == 1);
    assert(standard_tonality_spell_degree(PITCH_LETTER_A, 0, STANDARD_TONALITY_MODE_MELODIC_MINOR, 7, 0, &letter,
                                          &accidental));
    assert(letter == PITCH_LETTER_G && accidental == 1);
    /* C minor harmonic: B natural. G# harmonic minor: F double sharp. */
    assert(standard_tonality_spell_degree(PITCH_LETTER_C, 0, STANDARD_TONALITY_MODE_HARMONIC_MINOR, 7, 0, &letter,
                                          &accidental));
    assert(letter == PITCH_LETTER_B && accidental == 0);
    assert(standard_tonality_spell_degree(PITCH_LETTER_G, 1, STANDARD_TONALITY_MODE_HARMONIC_MINOR, 7, 0, &letter,
                                          &accidental));
    assert(letter == PITCH_LETTER_F && accidental == 2);
    /* An alteration by a chromatic step. */
    int degree;
    int alteration;
    assert(standard_tonality_degree_of(PITCH_LETTER_B, -1, STANDARD_TONALITY_MODE_MAJOR, PITCH_LETTER_G, -1, &degree,
                                       &alteration));
    assert(degree == 6 && alteration == -1 + 0);
    assert(standard_tonality_degree_of(PITCH_LETTER_B, 0, STANDARD_TONALITY_MODE_MAJOR, PITCH_LETTER_C, 0, &degree,
                                       &alteration));
    assert(degree == 2 && alteration == -1);
}

static void test_spell_rejects(void) {
    PitchLetter letter = PITCH_LETTER_D;
    int accidental = 5;
    assert(!standard_tonality_spell_degree(PITCH_LETTER_C, 0, STANDARD_TONALITY_MODE_MAJOR, 0, 0, &letter,
                                           &accidental));
    assert(!standard_tonality_spell_degree(PITCH_LETTER_C, 0, STANDARD_TONALITY_MODE_MAJOR, 8, 0, &letter,
                                           &accidental));
    assert(!standard_tonality_spell_degree(PITCH_LETTER_C, 0, (StandardTonalityMode)7, 1, 0, &letter, &accidental));
    assert(!standard_tonality_spell_degree((PitchLetter)42, 0, STANDARD_TONALITY_MODE_MAJOR, 1, 0, &letter,
                                           &accidental));
    assert(!standard_tonality_spell_degree(PITCH_LETTER_G, 2, STANDARD_TONALITY_MODE_MAJOR, 7, 0, &letter,
                                           &accidental));
    assert(!standard_tonality_spell_degree(PITCH_LETTER_C, 0, STANDARD_TONALITY_MODE_MAJOR, 1, 0, NULL, &accidental));
    assert(letter == PITCH_LETTER_D && accidental == 5);
    int degree;
    assert(!standard_tonality_degree_of(PITCH_LETTER_C, 0, STANDARD_TONALITY_MODE_MAJOR, PITCH_LETTER_C, 0, &degree,
                                        NULL));
    assert(!standard_tonality_degree_of(PITCH_LETTER_C, 0, (StandardTonalityMode)7, PITCH_LETTER_C, 0, &degree,
                                        &accidental));
    /* G double sharp has no major scale: its seventh would need three sharps. */
    assert(standard_tonality_scale_degree_in_mode(PITCH_LETTER_G, 2, STANDARD_TONALITY_MODE_MAJOR, PITCH_LETTER_G, 2) ==
           0);
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

static void test_relative_keys(void) {
    PitchLetter letter;
    int accidental;
    assert(standard_tonality_relative_key(PITCH_LETTER_C, 0, false, &letter, &accidental));
    assert(letter == PITCH_LETTER_A && accidental == 0);
    assert(standard_tonality_relative_key(PITCH_LETTER_A, 0, true, &letter, &accidental));
    assert(letter == PITCH_LETTER_C && accidental == 0);
    assert(standard_tonality_relative_key(PITCH_LETTER_E, -1, false, &letter, &accidental));
    assert(letter == PITCH_LETTER_C && accidental == 0);
    assert(standard_tonality_relative_key(PITCH_LETTER_C, 1, true, &letter, &accidental));
    assert(letter == PITCH_LETTER_E && accidental == 0);
    for (int fifths = -7; fifths <= 7; fifths++) {
        PitchLetter major_letter;
        int major_accidental;
        PitchLetter minor_letter;
        int minor_accidental;
        tonic_of(fifths, false, &major_letter, &major_accidental);
        assert(standard_tonality_relative_key(major_letter, major_accidental, false, &minor_letter, &minor_accidental));
        int minor_fifths;
        assert(standard_tonality_key_fifths(minor_letter, minor_accidental, true, &minor_fifths));
        assert(minor_fifths == fifths);
        PitchLetter back_letter;
        int back_accidental;
        assert(standard_tonality_relative_key(minor_letter, minor_accidental, true, &back_letter, &back_accidental));
        assert(back_letter == major_letter && back_accidental == major_accidental);
    }
}

static void test_circle_steps(void) {
    PitchLetter letter;
    int accidental;
    assert(standard_tonality_circle_step(PITCH_LETTER_C, 0, 1, &letter, &accidental));
    assert(letter == PITCH_LETTER_G && accidental == 0);
    assert(standard_tonality_circle_step(PITCH_LETTER_C, 0, -1, &letter, &accidental));
    assert(letter == PITCH_LETTER_F && accidental == 0);
    assert(standard_tonality_circle_step(PITCH_LETTER_C, 0, 6, &letter, &accidental));
    assert(letter == PITCH_LETTER_F && accidental == 1);
    assert(standard_tonality_circle_step(PITCH_LETTER_C, 0, -6, &letter, &accidental));
    assert(letter == PITCH_LETTER_G && accidental == -1);
    assert(standard_tonality_circle_step(PITCH_LETTER_B, 0, 0, &letter, &accidental));
    assert(letter == PITCH_LETTER_B && accidental == 0);
    /* Twelve steps return to the same pitch class, spelled with one more sharp per seven steps. */
    assert(standard_tonality_circle_step(PITCH_LETTER_C, 0, 12, &letter, &accidental));
    assert(letter == PITCH_LETTER_B && accidental == 1);
    assert(!standard_tonality_circle_step(PITCH_LETTER_C, 0, 40, &letter, &accidental));
    assert(!standard_tonality_circle_step(PITCH_LETTER_C, 0, 1, NULL, &accidental));
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

static void test_mode_mappings(void) {
    StandardTonalityMode mode;
    assert(standard_tonality_mode_of_minor_form(STANDARD_TONALITY_MINOR_NATURAL, &mode) &&
           mode == STANDARD_TONALITY_MODE_NATURAL_MINOR);
    assert(standard_tonality_mode_of_minor_form(STANDARD_TONALITY_MINOR_HARMONIC, &mode) &&
           mode == STANDARD_TONALITY_MODE_HARMONIC_MINOR);
    assert(standard_tonality_mode_of_minor_form(STANDARD_TONALITY_MINOR_MELODIC_ASCENDING, &mode) &&
           mode == STANDARD_TONALITY_MODE_MELODIC_MINOR);
    assert(standard_tonality_mode_of_minor_form(STANDARD_TONALITY_MINOR_MELODIC_DESCENDING, &mode) &&
           mode == STANDARD_TONALITY_MODE_NATURAL_MINOR);
    assert(!standard_tonality_mode_of_minor_form(STANDARD_TONALITY_MINOR_FORM_COUNT, &mode));
    assert(!standard_tonality_mode_of_minor_form(STANDARD_TONALITY_MINOR_NATURAL, NULL));
    assert(standard_tonality_mode_of_roman_key_mode(STANDARD_TONALITY_ROMAN_KEY_MAJOR, &mode) &&
           mode == STANDARD_TONALITY_MODE_MAJOR);
    assert(standard_tonality_mode_of_roman_key_mode(STANDARD_TONALITY_ROMAN_KEY_MINOR, &mode) &&
           mode == STANDARD_TONALITY_MODE_NATURAL_MINOR);
    assert(!standard_tonality_mode_of_roman_key_mode((StandardTonalityRomanKeyMode)9, &mode));
}

int main(void) {
    test_degree_semitones();
    test_spell_and_degree_in_every_key_and_mode();
    test_known_spellings();
    test_spell_rejects();
    test_key_signatures_for_all_thirty_keys();
    test_known_signatures();
    test_key_of_fifths();
    test_relative_keys();
    test_circle_steps();
    test_enharmonic_keys();
    test_mode_mappings();
    printf("test_tonal_key: ok\n");
    return 0;
}
