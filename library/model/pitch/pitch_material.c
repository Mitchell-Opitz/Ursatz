#include "model/pitch/pitch_material.h"

#include <string.h>

#define SEMITONES_PER_OCTAVE 12
#define DIATONIC_STEPS 7
#define THIRD_DEGREE 3

static int reduced(int semitones) {
    int value = semitones % SEMITONES_PER_OCTAVE;
    return value < 0 ? value + SEMITONES_PER_OCTAVE : value;
}

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
    int value = reduced(semitones_above_center);
    for (size_t i = 0; i < material->step_count; i++) {
        if (material->steps[i] == value) {
            return true;
        }
    }
    return false;
}

unsigned pitch_material_mask(const PitchMaterial *material, int center) {
    if (material == NULL) {
        return 0;
    }
    unsigned mask = 0;
    int base = reduced(center);
    for (size_t i = 0; i < material->step_count; i++) {
        mask |= 1u << ((base + material->steps[i]) % SEMITONES_PER_OCTAVE);
    }
    return mask;
}

int pitch_material_third(const PitchMaterial *material) {
    return pitch_material_step(material, THIRD_DEGREE);
}

bool pitch_material_is_diatonic(const PitchMaterial *material) {
    return material != NULL && material->family != NULL && strcmp(material->family, "diatonic") == 0 &&
           material->step_count == DIATONIC_STEPS;
}
