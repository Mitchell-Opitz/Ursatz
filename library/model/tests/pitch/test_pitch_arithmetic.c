#include "model/pitch/pitch_arithmetic.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const PitchLetter LETTERS[7] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                                       PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
static const int LETTER_SEMITONES[7] = {0, 2, 4, 5, 7, 9, 11};

static PitchSpelling *make(PitchLetter letter, int accidental, int octave) {
    PitchSpelling *spelling = NULL;
    assert(pitch_spelling_create(letter, accidental, octave, &spelling) == PITCH_SPELLING_OK);
    return spelling;
}

static void test_letter_semitone_and_index(void) {
    for (int i = 0; i < 7; i++) {
        assert(pitch_letter_semitone(LETTERS[i]) == LETTER_SEMITONES[i]);
        assert(pitch_letter_index(LETTERS[i]) == i);
    }
}

static void test_letter_offset_wraps_both_ways(void) {
    assert(pitch_letter_offset(PITCH_LETTER_C, 0) == PITCH_LETTER_C);
    assert(pitch_letter_offset(PITCH_LETTER_C, 1) == PITCH_LETTER_D);
    assert(pitch_letter_offset(PITCH_LETTER_B, 1) == PITCH_LETTER_C);
    assert(pitch_letter_offset(PITCH_LETTER_C, -1) == PITCH_LETTER_B);
    assert(pitch_letter_offset(PITCH_LETTER_G, 7) == PITCH_LETTER_G);
    assert(pitch_letter_offset(PITCH_LETTER_G, -15) == PITCH_LETTER_F);
    for (int i = 0; i < 7; i++) {
        for (int steps = -20; steps <= 20; steps++) {
            PitchLetter up = pitch_letter_offset(LETTERS[i], steps);
            assert(pitch_letter_index(up) == (((i + steps) % 7) + 7) % 7);
            assert(pitch_letter_offset(up, -steps) == LETTERS[i]);
        }
    }
}

static void test_every_spelling(void) {
    int count = 0;
    for (int i = 0; i < 7; i++) {
        for (int accidental = -2; accidental <= 2; accidental++) {
            PitchSpelling *spelling = make(LETTERS[i], accidental, 4);
            int semitone = LETTER_SEMITONES[i] + accidental;
            assert(pitch_class_of_letter(LETTERS[i], accidental) == ((semitone % 12) + 12) % 12);
            pitch_spelling_destroy(spelling);
            count++;
        }
    }
    assert(count == 35);
}

static void test_fifths_values(void) {
    assert(pitch_spelling_fifths(PITCH_LETTER_C, 0) == 0);
    assert(pitch_spelling_fifths(PITCH_LETTER_G, 0) == 1);
    assert(pitch_spelling_fifths(PITCH_LETTER_F, 0) == -1);
    assert(pitch_spelling_fifths(PITCH_LETTER_F, 1) == 6);
    assert(pitch_spelling_fifths(PITCH_LETTER_B, -1) == -2);
    assert(pitch_spelling_fifths(PITCH_LETTER_B, 0) == 5);
}

static void test_fifths_round_trip(void) {
    for (int i = 0; i < 7; i++) {
        for (int accidental = -2; accidental <= 2; accidental++) {
            PitchLetter letter;
            int back;
            pitch_spelling_from_fifths(pitch_spelling_fifths(LETTERS[i], accidental), &letter, &back);
            assert(letter == LETTERS[i] && back == accidental);
        }
    }
    for (int fifths = -40; fifths <= 40; fifths++) {
        PitchLetter letter;
        int accidental;
        pitch_spelling_from_fifths(fifths, &letter, &accidental);
        assert(pitch_spelling_fifths(letter, accidental) == fifths);
    }
}

static void test_name_every_spelling(void) {
    static const char *const SUFFIXES[5] = {"bb", "b", "", "#", "x"};
    static const char LETTER_CHARS[7] = {'C', 'D', 'E', 'F', 'G', 'A', 'B'};
    for (int i = 0; i < 7; i++) {
        for (int accidental = -2; accidental <= 2; accidental++) {
            char buffer[8];
            char expected[8];
            assert(pitch_spelling_name(LETTERS[i], accidental, buffer, sizeof(buffer)));
            snprintf(expected, sizeof(expected), "%c%s", LETTER_CHARS[i], SUFFIXES[accidental + 2]);
            assert(strcmp(buffer, expected) == 0);

            PitchLetter letter = PITCH_LETTER_A;
            int parsed = 99;
            assert(pitch_spelling_parse_name(buffer, &letter, &parsed, NULL));
            assert(letter == LETTERS[i] && parsed == accidental);
        }
    }
}

static void test_name_errors(void) {
    char buffer[8] = "keep";
    assert(!pitch_spelling_name(PITCH_LETTER_C, 3, buffer, sizeof(buffer)));
    assert(!pitch_spelling_name(PITCH_LETTER_C, -3, buffer, sizeof(buffer)));
    assert(!pitch_spelling_name((PitchLetter)42, 0, buffer, sizeof(buffer)));
    assert(!pitch_spelling_name(PITCH_LETTER_E, -2, buffer, 3));
    assert(!pitch_spelling_name(PITCH_LETTER_C, 0, NULL, 8));
    assert(strcmp(buffer, "keep") == 0);
    assert(pitch_spelling_name(PITCH_LETTER_E, -2, buffer, 4));
    assert(strcmp(buffer, "Ebb") == 0);
}

static void test_parse_accepts(void) {
    PitchLetter letter;
    int accidental;
    int octave = -99;

    assert(pitch_spelling_parse_name("F#4", &letter, &accidental, &octave));
    assert(letter == PITCH_LETTER_F && accidental == 1 && octave == 4);
    assert(pitch_spelling_parse_name("Bb-1", &letter, &accidental, &octave));
    assert(letter == PITCH_LETTER_B && accidental == -1 && octave == -1);
    assert(pitch_spelling_parse_name("Cx10", &letter, &accidental, &octave));
    assert(letter == PITCH_LETTER_C && accidental == 2 && octave == 10);
    assert(pitch_spelling_parse_name("G##", &letter, &accidental, &octave));
    assert(letter == PITCH_LETTER_G && accidental == 2 && octave == 10);
    assert(pitch_spelling_parse_name("Abb2", &letter, &accidental, &octave));
    assert(letter == PITCH_LETTER_A && accidental == -2 && octave == 2);
    assert(pitch_spelling_parse_name("Bbb", &letter, &accidental, NULL));
    assert(letter == PITCH_LETTER_B && accidental == -2);
    assert(pitch_spelling_parse_name("B", &letter, &accidental, &octave));
    assert(letter == PITCH_LETTER_B && accidental == 0 && octave == 2);
}

static void test_parse_rejects(void) {
    static const char *const BAD[] = {"",   "H",   "c",     "C#x", "C#b", "Cbbb", "C###", "C4x", "C 4",
                                      "C#4 ", "C-",  "C#-", "C4.5", "Cb4b", "#C",   "4",    "Cxx"};
    for (size_t i = 0; i < sizeof(BAD) / sizeof(BAD[0]); i++) {
        PitchLetter letter = PITCH_LETTER_D;
        int accidental = 7;
        int octave = 7;
        assert(!pitch_spelling_parse_name(BAD[i], &letter, &accidental, &octave));
        assert(letter == PITCH_LETTER_D && accidental == 7 && octave == 7);
    }
    PitchLetter letter;
    int accidental;
    assert(!pitch_spelling_parse_name("C4", &letter, &accidental, NULL));
    assert(!pitch_spelling_parse_name(NULL, &letter, &accidental, NULL));
    assert(!pitch_spelling_parse_name("C", NULL, &accidental, NULL));
    assert(!pitch_spelling_parse_name("C", &letter, NULL, NULL));
    assert(!pitch_spelling_parse_name("C12345678901", &letter, &accidental, &(int){0}));
}

static void test_pitch_class_names(void) {
    static const char *const SHARPS[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char *const FLATS[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
    for (int pc = 0; pc < 12; pc++) {
        assert(strcmp(pitch_class_name(pc, false), SHARPS[pc]) == 0);
        assert(strcmp(pitch_class_name(pc, true), FLATS[pc]) == 0);
    }
    assert(pitch_class_name(-1, false) == NULL);
    assert(pitch_class_name(12, true) == NULL);
}

static void test_letter_from_char(void) {
    static const char CHARS[7] = {'C', 'D', 'E', 'F', 'G', 'A', 'B'};
    for (int i = 0; i < 7; i++) {
        PitchLetter letter = PITCH_LETTER_A;
        assert(pitch_letter_from_char(CHARS[i], &letter));
        assert(pitch_letter_index(letter) == i);
    }
    PitchLetter untouched = PITCH_LETTER_D;
    assert(!pitch_letter_from_char('H', &untouched));
    assert(!pitch_letter_from_char('c', &untouched));
    assert(!pitch_letter_from_char('@', &untouched));
    assert(untouched == PITCH_LETTER_D);
    assert(!pitch_letter_from_char('A', NULL));
}

static void test_step_number_and_generic_size(void) {
    PitchSpelling *c4 = NULL;
    PitchSpelling *g4 = NULL;
    PitchSpelling *c5 = NULL;
    PitchSpelling *b3_flat = NULL;
    assert(pitch_spelling_create(PITCH_LETTER_C, 0, 4, &c4) == PITCH_SPELLING_OK);
    assert(pitch_spelling_create(PITCH_LETTER_G, 1, 4, &g4) == PITCH_SPELLING_OK);
    assert(pitch_spelling_create(PITCH_LETTER_C, -1, 5, &c5) == PITCH_SPELLING_OK);
    assert(pitch_spelling_create(PITCH_LETTER_B, -1, 3, &b3_flat) == PITCH_SPELLING_OK);
    assert(pitch_spelling_step_number(c4) == 28);
    assert(pitch_spelling_step_number(g4) == 32);
    assert(pitch_spelling_step_number(c5) == 35);
    assert(pitch_spelling_step_number(b3_flat) == 27);
    assert(pitch_spelling_step_number(NULL) == 0);
    assert(pitch_spelling_generic_size(c4, c4) == 1);
    assert(pitch_spelling_generic_size(c4, g4) == 5);
    assert(pitch_spelling_generic_size(g4, c4) == 5);
    assert(pitch_spelling_generic_size(c4, c5) == 8);
    assert(pitch_spelling_generic_size(b3_flat, c4) == 2);
    assert(pitch_spelling_generic_size(NULL, c4) == 1);
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(g4);
    pitch_spelling_destroy(c5);
    pitch_spelling_destroy(b3_flat);
}

static void test_direction_and_same_name(void) {
    PitchSpelling *c4 = NULL;
    PitchSpelling *d4 = NULL;
    PitchSpelling *b3_sharp = NULL;
    PitchSpelling *c5 = NULL;
    PitchSpelling *c_sharp4 = NULL;
    PitchSpelling *d_flat4 = NULL;
    assert(pitch_spelling_create(PITCH_LETTER_C, 0, 4, &c4) == PITCH_SPELLING_OK);
    assert(pitch_spelling_create(PITCH_LETTER_D, 0, 4, &d4) == PITCH_SPELLING_OK);
    assert(pitch_spelling_create(PITCH_LETTER_B, 1, 3, &b3_sharp) == PITCH_SPELLING_OK);
    assert(pitch_spelling_create(PITCH_LETTER_C, 0, 5, &c5) == PITCH_SPELLING_OK);
    assert(pitch_spelling_create(PITCH_LETTER_C, 1, 4, &c_sharp4) == PITCH_SPELLING_OK);
    assert(pitch_spelling_create(PITCH_LETTER_D, -1, 4, &d_flat4) == PITCH_SPELLING_OK);
    assert(pitch_spelling_direction(c4, d4) == 1);
    assert(pitch_spelling_direction(d4, c4) == -1);
    assert(pitch_spelling_direction(c4, c4) == 0);
    assert(pitch_spelling_direction(b3_sharp, c4) == 1); /* sounds alike: the step number decides */
    assert(pitch_spelling_direction(c4, b3_sharp) == -1);
    assert(pitch_spelling_same_name(c4, c5));
    assert(!pitch_spelling_same_name(c4, d4));
    assert(!pitch_spelling_same_name(c_sharp4, d_flat4)); /* enharmonic spellings differ */
    assert(!pitch_spelling_same_name(c4, c_sharp4));
    assert(!pitch_spelling_same_name(NULL, c4));
    assert(!pitch_spelling_same_name(c4, NULL));
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(d4);
    pitch_spelling_destroy(b3_sharp);
    pitch_spelling_destroy(c5);
    pitch_spelling_destroy(c_sharp4);
    pitch_spelling_destroy(d_flat4);
}

int main(void) {
    test_letter_semitone_and_index();
    test_letter_offset_wraps_both_ways();
    test_every_spelling();
    test_fifths_values();
    test_fifths_round_trip();
    test_name_every_spelling();
    test_name_errors();
    test_parse_accepts();
    test_parse_rejects();
    test_pitch_class_names();
    test_letter_from_char();
    test_step_number_and_generic_size();
    test_direction_and_same_name();
    printf("test_pitch_arithmetic: ok\n");
    return 0;
}
