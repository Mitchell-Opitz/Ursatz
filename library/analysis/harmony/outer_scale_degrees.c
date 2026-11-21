#include "analysis/harmony/outer_scale_degrees.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/core/provenance_reference.h"
#include "model/core/value.h"
#include "model/pitch/midi_pitch.h"
#include "theory/tonality/tonal_key.h"
#include "theory/tonality/common_practice_materials.h"

/* [degree - 1][alteration + 1]: flat, natural, sharp. */
static const char *const DEGREE_LABELS[7][3] = {{"b1", "1", "#1"}, {"b2", "2", "#2"}, {"b3", "3", "#3"},
                                                {"b4", "4", "#4"}, {"b5", "5", "#5"}, {"b6", "6", "#6"},
                                                {"b7", "7", "#7"}};

bool common_practice_scale_degree_of(int pitch_class, int tonic_pitch_class, CommonPracticeKeyMode mode,
                                     const CommonPracticeMaterials *materials, int *degree, int *alteration) {
    if (materials == NULL || degree == NULL || alteration == NULL || pitch_class < 0 || pitch_class > 11 || tonic_pitch_class < 0 ||
        tonic_pitch_class > 11 || (mode != COMMON_PRACTICE_KEY_MAJOR && mode != COMMON_PRACTICE_KEY_MINOR)) {
        return false;
    }
    const PitchMaterial *material = common_practice_material(materials, mode);
    return standard_tonality_material_degree_of_semitone(pitch_class - tonic_pitch_class, material, degree, alteration);
}

const char *common_practice_scale_degree_label(int pitch_class, int tonic_pitch_class, CommonPracticeKeyMode mode,
                                               const CommonPracticeMaterials *materials) {
    int degree = 0;
    int alteration = 0;
    if (!common_practice_scale_degree_of(pitch_class, tonic_pitch_class, mode, materials, &degree, &alteration)) {
        return NULL;
    }
    return DEGREE_LABELS[degree - 1][alteration + 1];
}


