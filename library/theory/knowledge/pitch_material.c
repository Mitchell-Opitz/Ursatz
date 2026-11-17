#include "theory/knowledge/pitch_material.h"

#define SEMITONES_PER_OCTAVE 12

int pitch_material_step(const PitchMaterial *material, int degree) {
    if (material == NULL || degree < 1 || (size_t)degree > material->step_count) {
        return -1;
    }
    return material->steps[degree - 1];
}

bool pitch_material_contains(const PitchMaterial *material, int semitones_above_center) {
    if (material == NULL) {
        return false;
    }
    int reduced = semitones_above_center % SEMITONES_PER_OCTAVE;
    if (reduced < 0) {
        reduced += SEMITONES_PER_OCTAVE;
    }
    for (size_t i = 0; i < material->step_count; i++) {
        if (material->steps[i] == reduced) {
            return true;
        }
    }
    return false;
}
