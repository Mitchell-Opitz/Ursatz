#include "model/pitch/midi_pitch.h"

#include <stddef.h>

#include "model/pitch/pitch_arithmetic.h"

#define KEY_SIGNATURE_MAX_ACCIDENTALS 7

/* The windows of the line of fifths the names come from (pitch_arithmetic.h): sharps start at -1, flats at -6, a major tonic at -5
   and a minor tonic at -3. */
#define SHARPS_LOWEST (-1)
#define FLATS_LOWEST (-6)
#define MAJOR_TONIC_LOWEST (-5)
#define MINOR_TONIC_LOWEST (-3)

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
    PitchLetter letter;
    int accidental;
    if (!pitch_class_spelling(pitch_class, flats ? FLATS_LOWEST : SHARPS_LOWEST, &letter, &accidental)) {
        return false;
    }
    return pitch_spelling_create(letter, accidental, octave, out) == PITCH_SPELLING_OK;
}

int pitch_spelling_midi(const PitchSpelling *spelling) {
    if (spelling == NULL) {
        return 0;
    }
    return 12 * (pitch_spelling_octave(spelling) + 1) + pitch_letter_semitone(pitch_spelling_letter(spelling)) +
           pitch_spelling_accidental(spelling);
}

bool key_signature_from_fifths(int fifths, KeySignature **out) {
    if (out == NULL || fifths < -KEY_SIGNATURE_MAX_ACCIDENTALS || fifths > KEY_SIGNATURE_MAX_ACCIDENTALS) {
        return false;
    }
    int size = fifths < 0 ? -fifths : fifths;
    KeySignatureEntry entries[KEY_SIGNATURE_MAX_ACCIDENTALS];
    for (int i = 0; i < size; i++) {
        /* the sharps come in the order F C G D A E B along the line of fifths, the flats in the reverse order */
        int accidental_fifths = fifths > 0 ? -1 + i : KEY_SIGNATURE_MAX_ACCIDENTALS - 2 - i;
        int natural;
        pitch_spelling_from_fifths(accidental_fifths, &entries[i].letter, &natural);
        entries[i].accidental = fifths > 0 ? 1 : -1;
    }
    return key_signature_create(entries, (size_t)size, out) == KEY_SIGNATURE_OK;
}

const char *key_tonic_name(int tonic_pitch_class) {
    return pitch_class_window_name(pitch_class_of_midi(tonic_pitch_class), MAJOR_TONIC_LOWEST);
}

const char *key_tonic_name_minor(int tonic_pitch_class) {
    return pitch_class_window_name(pitch_class_of_midi(tonic_pitch_class), MINOR_TONIC_LOWEST);
}
