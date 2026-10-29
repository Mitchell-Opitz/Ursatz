#include "tonal_key.h"

#include <stddef.h>

#include "pitch/pitch_arithmetic.h"

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
    return true;
}

bool standard_tonality_degree_of(PitchLetter tonic_letter, int tonic_accidental,
                                 StandardTonalityMode mode, PitchLetter note_letter,
                                 int note_accidental, int *degree, int *alteration) {
    if (degree == NULL || alteration == NULL || !letter_valid(tonic_letter) ||
        !letter_valid(note_letter) || !mode_valid(mode)) {
        return false;
    }
    int found = floor_mod(pitch_letter_index(note_letter) - pitch_letter_index(tonic_letter),
                          KEY_DEGREE_COUNT) +
                1;
    int natural_distance =
        floor_mod(pitch_letter_semitone(note_letter) - pitch_letter_semitone(tonic_letter),
                  PITCH_SEMITONES_PER_OCTAVE);
    *degree = found;
    *alteration =
        natural_distance + note_accidental - tonic_accidental - MODE_SEMITONES[mode][found - 1];
    return true;
}

bool standard_tonality_spell_degree(PitchLetter tonic_letter, int tonic_accidental,
                                    StandardTonalityMode mode, int degree, int alteration,
                                    PitchLetter *letter, int *accidental) {
    if (letter == NULL || accidental == NULL || !letter_valid(tonic_letter) || !mode_valid(mode) ||
        degree < 1 || degree > KEY_DEGREE_COUNT) {
        return false;
    }
    PitchLetter spelled = pitch_letter_offset(tonic_letter, degree - 1);
    int natural_distance =
        floor_mod(pitch_letter_semitone(spelled) - pitch_letter_semitone(tonic_letter),
                  PITCH_SEMITONES_PER_OCTAVE);
    int offset =
        tonic_accidental + MODE_SEMITONES[mode][degree - 1] + alteration - natural_distance;
    if (offset < -KEY_MAX_ACCIDENTAL_OFFSET || offset > KEY_MAX_ACCIDENTAL_OFFSET) {
        return false;
    }
    *letter = spelled;
    *accidental = offset;
    return true;
}

int standard_tonality_scale_degree_in_mode(PitchLetter tonic_letter, int tonic_accidental,
                                           StandardTonalityMode mode, PitchLetter note_letter,
                                           int note_accidental) {
    for (int degree = 1; degree <= KEY_DEGREE_COUNT; degree++) {
        PitchLetter letter;
        int accidental;
        if (!standard_tonality_spell_degree(tonic_letter, tonic_accidental, mode, degree, 0,
                                            &letter, &accidental)) {
            return 0;
        }
    }
    int degree;
    int alteration;
    if (!standard_tonality_degree_of(tonic_letter, tonic_accidental, mode, note_letter,
                                     note_accidental, &degree, &alteration)) {
        return 0;
    }
    return alteration == 0 ? degree : 0;
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
    /* Sharps come in the order F C G D A E B; flats in the reverse. */
    static const PitchLetter SHARP_ORDER[KEY_MAX_ACCIDENTALS] = {
        PITCH_LETTER_F, PITCH_LETTER_C, PITCH_LETTER_G, PITCH_LETTER_D,
        PITCH_LETTER_A, PITCH_LETTER_E, PITCH_LETTER_B};
    int size = fifths < 0 ? -fifths : fifths;
    KeySignatureEntry entries[KEY_MAX_ACCIDENTALS];
    for (int i = 0; i < size; i++) {
        entries[i].letter = fifths > 0 ? SHARP_ORDER[i] : SHARP_ORDER[KEY_MAX_ACCIDENTALS - 1 - i];
        entries[i].accidental = fifths > 0 ? 1 : -1;
    }
    return key_signature_create(entries, (size_t)size, out) == KEY_SIGNATURE_OK;
}

bool standard_tonality_key_of_fifths(int fifths, bool minor, PitchLetter *letter, int *accidental) {
    if (letter == NULL || accidental == NULL) {
        return false;
    }
    pitch_spelling_from_fifths(fifths + (minor ? KEY_MINOR_FIFTHS_OFFSET : 0), letter, accidental);
    return true;
}

bool standard_tonality_relative_key(PitchLetter tonic_letter, int tonic_accidental, bool minor,
                                    PitchLetter *letter, int *accidental) {
    int fifths;
    if (!standard_tonality_key_fifths(tonic_letter, tonic_accidental, minor, &fifths)) {
        return false;
    }
    return standard_tonality_key_of_fifths(fifths, !minor, letter, accidental);
}

bool standard_tonality_circle_step(PitchLetter letter, int accidental, int steps,
                                   PitchLetter *out_letter, int *out_accidental) {
    if (out_letter == NULL || out_accidental == NULL || !letter_valid(letter)) {
        return false;
    }
    PitchLetter stepped;
    int stepped_accidental;
    pitch_spelling_from_fifths(pitch_spelling_fifths(letter, accidental) + steps, &stepped,
                               &stepped_accidental);
    if (stepped_accidental < -KEY_MAX_ACCIDENTAL_OFFSET ||
        stepped_accidental > KEY_MAX_ACCIDENTAL_OFFSET) {
        return false;
    }
    *out_letter = stepped;
    *out_accidental = stepped_accidental;
    return true;
}

bool standard_tonality_mode_of_minor_form(StandardTonalityMinorForm form,
                                          StandardTonalityMode *out) {
    if (out == NULL) {
        return false;
    }
    switch (form) {
    case STANDARD_TONALITY_MINOR_NATURAL:
    case STANDARD_TONALITY_MINOR_MELODIC_DESCENDING:
        *out = STANDARD_TONALITY_MODE_NATURAL_MINOR;
        return true;
    case STANDARD_TONALITY_MINOR_HARMONIC:
        *out = STANDARD_TONALITY_MODE_HARMONIC_MINOR;
        return true;
    case STANDARD_TONALITY_MINOR_MELODIC_ASCENDING:
        *out = STANDARD_TONALITY_MODE_MELODIC_MINOR;
        return true;
    default:
        return false;
    }
}

bool standard_tonality_mode_of_roman_key_mode(StandardTonalityRomanKeyMode mode,
                                              StandardTonalityMode *out) {
    if (out == NULL) {
        return false;
    }
    switch (mode) {
    case STANDARD_TONALITY_ROMAN_KEY_MAJOR:
        *out = STANDARD_TONALITY_MODE_MAJOR;
        return true;
    case STANDARD_TONALITY_ROMAN_KEY_MINOR:
        *out = STANDARD_TONALITY_MODE_NATURAL_MINOR;
        return true;
    default:
        return false;
    }
}

const char *standard_tonality_chromatic_degree_label(int semitones_above_tonic) {
    static const char *const LABELS[12] = {"1", "b2", "2", "b3", "3", "4", "#4", "5", "b6", "6", "b7", "7"};
    return LABELS[((semitones_above_tonic % 12) + 12) % 12];
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
