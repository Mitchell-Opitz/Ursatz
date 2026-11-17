#include "analysis/notes/analysis_key.h"

#include <stddef.h>

#include "model/pitch/pitch_arithmetic.h"
#include "theory/tonality/tonal_key.h"

int analysis_key_tonic_pitch_class(const AnalysisKey *key) {
    if (key == NULL) {
        return -1;
    }
    return pitch_class_of_letter(key->tonic_letter, key->tonic_accidental);
}

bool analysis_key_equals(const AnalysisKey *a, const AnalysisKey *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return a->tonic_letter == b->tonic_letter && a->tonic_accidental == b->tonic_accidental && a->mode == b->mode;
}

bool analysis_key_from_fifths_material(int fifths, const PitchMaterial *material, AnalysisKey *out) {
    KeyStructureMode mode;
    if (out == NULL || fifths < -7 || fifths > 7 || !key_structure_mode_of(material, &mode)) {
        return false;
    }
    PitchLetter letter;
    int accidental;
    if (!standard_tonality_key_of_fifths_material(fifths, material, &letter, &accidental)) {
        return false;
    }
    out->id = NULL;
    out->tonic_letter = letter;
    out->tonic_accidental = accidental;
    out->mode = mode;
    out->material = material;
    return true;
}

bool analysis_key_from_fifths(int fifths, KeyStructureMode mode, AnalysisKey *out) {
    if (out == NULL || fifths < -7 || fifths > 7) {
        return false;
    }
    if (mode != KEY_STRUCTURE_MODE_MAJOR && mode != KEY_STRUCTURE_MODE_MINOR) {
        return false;
    }
    PitchLetter letter;
    int accidental;
    pitch_spelling_from_fifths(mode == KEY_STRUCTURE_MODE_MINOR ? fifths + 3 : fifths, &letter, &accidental);
    out->id = NULL;
    out->tonic_letter = letter;
    out->tonic_accidental = accidental;
    out->mode = mode;
    out->material = NULL;
    return true;
}
