#include "analysis/notes/analysis_key.h"

#include <stddef.h>
#include <string.h>

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
    bool same_material = a->material == b->material ||
                         (a->material != NULL && b->material != NULL && strcmp(a->material->id, b->material->id) == 0);
    return a->tonic_letter == b->tonic_letter && a->tonic_accidental == b->tonic_accidental && same_material;
}

bool analysis_key_from_fifths(int fifths, const PitchMaterial *material, AnalysisKey *out) {
    if (out == NULL || fifths < -7 || fifths > 7) {
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
    out->material = material;
    return true;
}
