#include "pitch/midi_pitch.h"

#include <stddef.h>

#define KEY_SIGNATURE_MAX_ACCIDENTALS 7

static const PitchLetter SHARP_LETTER[12] = {PITCH_LETTER_C, PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_D,
                                             PITCH_LETTER_E, PITCH_LETTER_F, PITCH_LETTER_F, PITCH_LETTER_G,
                                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_A, PITCH_LETTER_B};
static const int SHARP_ACCIDENTAL[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
static const PitchLetter FLAT_LETTER[12] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_D, PITCH_LETTER_E,
                                            PITCH_LETTER_E, PITCH_LETTER_F, PITCH_LETTER_G, PITCH_LETTER_G,
                                            PITCH_LETTER_A, PITCH_LETTER_A, PITCH_LETTER_B, PITCH_LETTER_B};
static const int FLAT_ACCIDENTAL[12] = {0, -1, 0, -1, 0, 0, -1, 0, -1, 0, -1, 0};

static const char *const TONIC_NAMES[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

int pitch_class_of_midi(int midi) {
    return ((midi % 12) + 12) % 12;
}

bool pitch_spelling_of_midi(int midi, int key_fifths, PitchSpelling **out) {
    if (out == NULL) {
        return false;
    }
    int pitch_class = pitch_class_of_midi(midi);
    int octave = (midi - pitch_class) / 12 - 1;
    bool flats = key_fifths < 0;
    PitchLetter letter = flats ? FLAT_LETTER[pitch_class] : SHARP_LETTER[pitch_class];
    int accidental = flats ? FLAT_ACCIDENTAL[pitch_class] : SHARP_ACCIDENTAL[pitch_class];
    return pitch_spelling_create(letter, accidental, octave, out) == PITCH_SPELLING_OK;
}

bool key_signature_from_fifths(int fifths, KeySignature **out) {
    if (out == NULL || fifths < -KEY_SIGNATURE_MAX_ACCIDENTALS || fifths > KEY_SIGNATURE_MAX_ACCIDENTALS) {
        return false;
    }
    static const PitchLetter SHARP_ORDER[KEY_SIGNATURE_MAX_ACCIDENTALS] = {
        PITCH_LETTER_F, PITCH_LETTER_C, PITCH_LETTER_G, PITCH_LETTER_D,
        PITCH_LETTER_A, PITCH_LETTER_E, PITCH_LETTER_B};
    int size = fifths < 0 ? -fifths : fifths;
    KeySignatureEntry entries[KEY_SIGNATURE_MAX_ACCIDENTALS];
    for (int i = 0; i < size; i++) {
        entries[i].letter = fifths > 0 ? SHARP_ORDER[i] : SHARP_ORDER[KEY_SIGNATURE_MAX_ACCIDENTALS - 1 - i];
        entries[i].accidental = fifths > 0 ? 1 : -1;
    }
    return key_signature_create(entries, (size_t)size, out) == KEY_SIGNATURE_OK;
}

const char *key_tonic_name(int tonic_pitch_class) {
    return TONIC_NAMES[pitch_class_of_midi(tonic_pitch_class)];
}
