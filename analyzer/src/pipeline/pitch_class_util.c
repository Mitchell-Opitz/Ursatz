#include "pipeline/pitch_class_util.h"

#include <stdio.h>
#include <stdlib.h>

#include "theory/tonality/tonal_key.h"

int pitch_class_of(long midi_note_number) {
    long remainder = midi_note_number % PITCH_SEMITONES_PER_OCTAVE;
    if (remainder < 0) {
        remainder += PITCH_SEMITONES_PER_OCTAVE;
    }
    return (int)remainder;
}

bool tonic_prefers_flats(int tonic_pitch_class, const PitchMaterial *material) {
    PitchLetter letter;
    int accidental;
    int fifths;
    return material != NULL &&
           standard_tonality_key_tonic_of_pitch_class_material(tonic_pitch_class, material, &letter, &accidental) &&
           standard_tonality_key_fifths_of(letter, accidental, material, &fifths) && fifths < 0;
}

char *key_label_create(int tonic_pitch_class, const PitchMaterial *material) {
    if (material == NULL || material->key_word == NULL) {
        return NULL;
    }
    const char *name = pitch_class_name(tonic_pitch_class, tonic_prefers_flats(tonic_pitch_class, material));
    const char *mode = material->key_word;
    size_t size = (size_t)snprintf(NULL, 0, "%s %s", name, mode) + 1;
    char *label = malloc(size);
    if (label) {
        snprintf(label, size, "%s %s", name, mode);
    }
    return label;
}

int key_signature_major_tonic(int sharps_flats) {
    PitchLetter letter;
    int accidental;
    if (sharps_flats < -7 || sharps_flats > 7 ||
        !standard_tonality_key_of_fifths(sharps_flats, false, &letter, &accidental)) {
        return -1;
    }
    return pitch_class_of_letter(letter, accidental);
}
