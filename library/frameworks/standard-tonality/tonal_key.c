#include "tonal_key.h"

#include <stddef.h>

#include "model/pitch/pitch_arithmetic.h"
#include "model/pitch/midi_pitch.h"

#define KEY_MAX_ACCIDENTALS 7
#define KEY_MINOR_FIFTHS_OFFSET 3
#define KEY_DEGREE_COUNT 7
#define KEY_MAX_ACCIDENTAL_OFFSET 2

static const int MODE_SEMITONES[4][KEY_DEGREE_COUNT] = {
    {0, 2, 4, 5, 7, 9, 11},
    {0, 2, 3, 5, 7, 8, 10},
    {0, 2, 3, 5, 7, 8, 11},
    {0, 2, 3, 5, 7, 9, 11},
};

static bool mode_valid(StandardTonalityMode mode) {
    return mode >= STANDARD_TONALITY_MODE_MAJOR && mode <= STANDARD_TONALITY_MODE_MELODIC_MINOR;
}

static bool letter_valid(PitchLetter letter) {
    return letter >= PITCH_LETTER_A && letter <= PITCH_LETTER_G;
}

static int floor_mod(int value, int modulus) {
    int result = value % modulus;
    return result < 0 ? result + modulus : result;
}

int standard_tonality_degree_semitone(StandardTonalityMode mode, int degree) {
    if (!mode_valid(mode) || degree < 1 || degree > KEY_DEGREE_COUNT) {
        return -1;
    }
    return MODE_SEMITONES[mode][degree - 1];
}

unsigned standard_tonality_mode_pitch_classes(int tonic_pitch_class, StandardTonalityMode mode) {
    if (!mode_valid(mode) || tonic_pitch_class < 0 || tonic_pitch_class >= PITCH_SEMITONES_PER_OCTAVE) {
        return 0;
    }
    unsigned mask = 0;
    for (int d = 0; d < KEY_DEGREE_COUNT; d++) {
        mask |= 1u << ((tonic_pitch_class + MODE_SEMITONES[mode][d]) % PITCH_SEMITONES_PER_OCTAVE);
    }
    return mask;
}

/* A chromatic semitone with no context is read as the next degree lowered where the two spellings are
 * a semitone from the neighbors on both sides (R-285): semitone 1 in every mode, and in major also 3, 8
 * and 10. The sharp readings that stay are the raised fourth, and the natural minor's #3, #6 and #7. */
static bool semitone_reads_lowered(StandardTonalityMode mode, int inside) {
    if (inside == 1) {
        return true;
    }
    return mode == STANDARD_TONALITY_MODE_MAJOR && (inside == 3 || inside == 8 || inside == 10);
}

bool standard_tonality_degree_of_semitone(int semitones_above_tonic, StandardTonalityMode mode, int *degree,
                                          int *alteration) {
    if (degree == NULL || alteration == NULL || !mode_valid(mode)) {
        return false;
    }
    int inside = floor_mod(semitones_above_tonic, PITCH_SEMITONES_PER_OCTAVE);
    int found = 0;
    for (int d = 0; d < KEY_DEGREE_COUNT; d++) {
        if (MODE_SEMITONES[mode][d] <= inside) {
            found = d;
        }
    }
    *degree = found + 1;
    *alteration = inside - MODE_SEMITONES[mode][found];
    if (semitone_reads_lowered(mode, inside)) {
        *degree = found + 2;
        *alteration = -1;
    }
    return true;
}

bool standard_tonality_key_fifths(PitchLetter tonic_letter, int tonic_accidental, bool minor,
                                  int *fifths) {
    if (fifths == NULL || !letter_valid(tonic_letter)) {
        return false;
    }
    *fifths = pitch_spelling_fifths(tonic_letter, tonic_accidental) -
              (minor ? KEY_MINOR_FIFTHS_OFFSET : 0);
    return true;
}

bool standard_tonality_key_signature_of(PitchLetter tonic_letter, int tonic_accidental, bool minor,
                                        KeySignature **out) {
    int fifths;
    if (out == NULL ||
        !standard_tonality_key_fifths(tonic_letter, tonic_accidental, minor, &fifths) ||
        fifths < -KEY_MAX_ACCIDENTALS || fifths > KEY_MAX_ACCIDENTALS) {
        return false;
    }
    return key_signature_from_fifths(fifths, out);
}

bool standard_tonality_key_of_fifths(int fifths, bool minor, PitchLetter *letter, int *accidental) {
    if (letter == NULL || accidental == NULL) {
        return false;
    }
    pitch_spelling_from_fifths(fifths + (minor ? KEY_MINOR_FIFTHS_OFFSET : 0), letter, accidental);
    return true;
}

const char *standard_tonality_chromatic_degree_label(int semitones_above_tonic) {
    static const char *const LABELS[12] = {"1", "b2", "2", "b3", "3", "4", "#4", "5", "b6", "6", "b7", "7"};
    return LABELS[pitch_class_of_midi(semitones_above_tonic)];
}

/* Major tonics by signature size 0 to 7, sharps then flats (circle of fifths). */
static const int SHARP_MAJOR_TONICS[8] = {0, 7, 2, 9, 4, 11, 6, 1};
static const int FLAT_MAJOR_TONICS[8] = {0, 5, 10, 3, 8, 1, 6, 11};

static int signature_size(const int tonics[8], int pitch_class) {
    for (int size = 0; size < 8; size++) {
        if (tonics[size] == pitch_class) {
            return size;
        }
    }
    return 99;
}

bool standard_tonality_key_tonic_of_pitch_class(int pitch_class, bool minor, PitchLetter *letter,
                                                int *accidental) {
    if (pitch_class < 0 || pitch_class > 11 || letter == NULL || accidental == NULL) {
        return false;
    }
    int major_pitch_class = minor ? (pitch_class + 3) % 12 : pitch_class;
    int sharps = signature_size(SHARP_MAJOR_TONICS, major_pitch_class);
    int flats = signature_size(FLAT_MAJOR_TONICS, major_pitch_class);
    return standard_tonality_key_of_fifths(flats < sharps ? -flats : sharps, minor, letter, accidental);
}
