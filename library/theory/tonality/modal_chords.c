#include "theory/tonality/modal_chords.h"

#define MODAL_STEPS 7
#define SEMITONES_PER_OCTAVE 12
#define THIRD_MINOR 3
#define THIRD_MAJOR 4
#define FIFTH_DIMINISHED 6
#define FIFTH_PERFECT 7
#define FIFTH_AUGMENTED 8

static int above(const PitchMaterial *material, int from_index, int to_index) {
    return (material->steps[to_index % MODAL_STEPS] - material->steps[from_index] + SEMITONES_PER_OCTAVE) % SEMITONES_PER_OCTAVE;
}

bool pitch_material_triad_on(const PitchMaterial *material, int degree, int *root_semitones, int intervals[3],
                             StandardTonalityTriadQuality *quality) {
    if (material == NULL || material->step_count != MODAL_STEPS || degree < 1 || degree > MODAL_STEPS) {
        return false;
    }
    int index = degree - 1;
    int third = above(material, index, index + 2);
    int fifth = above(material, index, index + 4);
    StandardTonalityTriadQuality found;
    if (third == THIRD_MAJOR && fifth == FIFTH_PERFECT) {
        found = STANDARD_TONALITY_TRIAD_MAJOR;
    } else if (third == THIRD_MINOR && fifth == FIFTH_PERFECT) {
        found = STANDARD_TONALITY_TRIAD_MINOR;
    } else if (third == THIRD_MINOR && fifth == FIFTH_DIMINISHED) {
        found = STANDARD_TONALITY_TRIAD_DIMINISHED;
    } else if (third == THIRD_MAJOR && fifth == FIFTH_AUGMENTED) {
        found = STANDARD_TONALITY_TRIAD_AUGMENTED;
    } else {
        return false;
    }
    if (root_semitones != NULL) {
        *root_semitones = material->steps[index];
    }
    if (intervals != NULL) {
        intervals[0] = 0;
        intervals[1] = third;
        intervals[2] = fifth;
    }
    if (quality != NULL) {
        *quality = found;
    }
    return true;
}

bool pitch_material_is_modal_key(const CommonPracticeMaterials *materials, const PitchMaterial *material) {
    if (materials == NULL || material == NULL || material->step_count != MODAL_STEPS || material->cadence == NULL) {
        return false;
    }
    return material != materials->major && material != materials->natural_minor;
}

bool pitch_material_avoids(const PitchMaterial *material, int degree) {
    if (material == NULL) {
        return false;
    }
    for (size_t i = 0; i < material->avoided_count; i++) {
        if (material->avoided_degrees[i] == degree) {
            return true;
        }
    }
    return false;
}

bool modal_function_of(const PitchMaterial *material, const VocabularyCadence *cadence, int degree, ModalFunction *out) {
    StandardTonalityTriadQuality quality;
    if (out == NULL || !pitch_material_triad_on(material, degree, NULL, NULL, &quality)) {
        return false;
    }
    if (degree == 1) {
        *out = MODAL_FUNCTION_TONIC;
        return true;
    }
    if (cadence != NULL) {
        for (size_t i = 0; i < cadence->approach_count; i++) {
            if (cadence->approach_degrees[i] == degree) {
                *out = MODAL_FUNCTION_DOMINANT;
                return true;
            }
        }
    }
    if ((quality != STANDARD_TONALITY_TRIAD_MAJOR && quality != STANDARD_TONALITY_TRIAD_MINOR) || pitch_material_avoids(material, degree)) {
        return false;
    }
    *out = MODAL_FUNCTION_PREDOMINANT;
    return true;
}
