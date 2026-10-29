#include "semantics/analysis_key.h"

#include <stddef.h>

#include "pitch/pitch_arithmetic.h"

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
    return true;
}
