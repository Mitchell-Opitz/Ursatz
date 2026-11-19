#include "theory/tonality/key_vocabulary.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/pitch/midi_pitch.h"
#include "model/pitch/pitch_arithmetic.h"
#include "theory/tonality/common_practice_materials.h"

static const char *mode_suffix(CommonPracticeKeyMode mode) {
    switch (mode) {
        case COMMON_PRACTICE_KEY_MAJOR:
            return "major";
        case COMMON_PRACTICE_KEY_MINOR:
            return "minor";
        default:
            return NULL;
    }
}

/* The pitch class of a tonic name: a letter A-G then any number of '#' or 'b' accidentals. -1 if it is not
   one. The accidental loop stays here on purpose (R-582): a name
   with three or more accidentals is read as written, which no owner function does. */
static int tonic_pitch_class_of(const char *name) {
    PitchLetter letter;
    if (!pitch_letter_from_char(name[0], &letter)) {
        return -1;
    }
    int pitch_class = pitch_letter_semitone(letter);
    for (const char *c = name + 1; *c != '\0'; c++) {
        if (*c == '#') {
            pitch_class++;
        } else if (*c == 'b') {
            pitch_class--;
        } else {
            return -1;
        }
    }
    return pitch_class_of_midi(pitch_class);
}

bool common_practice_build_key(const PitchMaterial *material, const char *tonic_name, KeyStructure *out) {
    if (tonic_name == NULL || tonic_name[0] == '\0' || out == NULL) {
        return false;
    }
    CommonPracticeKeyMode mode;
    if (!common_practice_key_mode_of(material, &mode) || mode_suffix(mode) == NULL) {
        return false;
    }
    int tonic_pitch_class = tonic_pitch_class_of(tonic_name);
    if (tonic_pitch_class < 0) {
        return false;
    }
    out->tonic_pitch_class = tonic_pitch_class;
    out->material = material;
    return true;
}
