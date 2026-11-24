#include "util/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <model/pitch/midi_pitch.h>
#include <model/pitch/pitch_arithmetic.h>
#include <theory/tonality/modal_chords.h>
#include <theory/tonality/common_practice_materials.h>
#include <theory/tonality/tonal_key.h>

int util_absolute(int value) {
    return value < 0 ? -value : value;
}

unsigned util_bits_set(unsigned bits) {
    unsigned count = 0;
    for (; bits; bits >>= 1) {
        count += bits & 1u;
    }
    return count;
}

int util_sign(int value) {
    return value > 0 ? 1 : (value < 0 ? -1 : 0);
}

int util_pitch_class(int pitch) {
    return pitch_class_of_midi(pitch);
}

void util_pitch_name(int midi, bool prefer_flats, char *out, size_t size) {
    pitch_name_of_midi(midi, prefer_flats, out, size);
}

bool composer_material_is_minor(const PitchMaterial *material) {
    CommonPracticeKeyMode mode;
    return common_practice_key_mode_of(material, &mode) && mode == COMMON_PRACTICE_KEY_MINOR;
}

#define MAJOR_THIRD_SEMITONES 4
#define MAJOR_SEVENTH_SEMITONES 11

int composer_material_third(const PitchMaterial *material) {
    return composer_material_valid(material) ? pitch_material_third(material) : MAJOR_THIRD_SEMITONES;
}

bool composer_material_valid(const PitchMaterial *material) {
    CommonPracticeKeyMode mode;
    return common_practice_key_mode_of(material, &mode);
}

bool composer_material_supported(const CommonPracticeMaterials *materials, const PitchMaterial *material) {
    return materials != NULL && material != NULL &&
           (material == materials->major || material == materials->natural_minor || pitch_material_is_modal_key(materials, material));
}

bool composer_material_same_mode(const PitchMaterial *a, const PitchMaterial *b) {
    if (a == b) {
        return true;
    }
    if (pitch_material_is_modal_key(NULL, a) || pitch_material_is_modal_key(NULL, b)) {
        return false;
    }
    return composer_material_is_minor(a) == composer_material_is_minor(b);
}

bool composer_key_same_mode(const SpelledKey *a, const SpelledKey *b) {
    return composer_material_same_mode(a != NULL ? a->material : NULL, b != NULL ? b->material : NULL);
}

bool composer_material_counts_as_minor(const PitchMaterial *material) {
    return composer_material_is_minor(material) || pitch_material_is_modal_key(NULL, material);
}

unsigned composer_key_color_mask(const SpelledKey *key) {
    const PitchMaterial *material = key != NULL ? key->material : NULL;
    unsigned mask = 0;
    for (size_t i = 0; material != NULL && i < material->characteristic_count; i++) {
        int step = pitch_material_step(material, material->characteristic_degrees[i]);
        if (step >= 0) {
            mask |= 1u << ((spelled_key_tonic_pitch_class(key) + step) % 12);
        }
    }
    return mask;
}

const char *composer_key_word(const SpelledKey *key) {
    const PitchMaterial *material = key != NULL ? key->material : NULL;
    if (material != NULL && pitch_material_is_modal_key(NULL, material)) {
        return material->key_word != NULL ? material->key_word : material->name;
    }
    return composer_key_is_minor(key) ? "minor" : "major";
}

int composer_material_default_tonic(const PitchMaterial *material) {
    int rotation = pitch_material_major_rotation(material);
    if (rotation > 0) {
        return standard_tonality_natural_degree_semitone(rotation);
    }
    return composer_material_is_minor(material) ? 9 : 0;
}

bool composer_material_raises_seventh(const PitchMaterial *material) {
    return pitch_material_is_natural_minor(material);
}

bool composer_key_is_minor(const SpelledKey *key) {
    return key && composer_material_is_minor(key->material);
}

void composer_key_set_material(SpelledKey *key, const PitchMaterial *material) {
    key->material = material;
}

unsigned util_scale_mask(int tonic_pitch_class, const PitchMaterial *material) {
    return standard_tonality_material_pitch_classes(util_pitch_class(tonic_pitch_class), material);
}

/* The scale with the raised sixth and seventh beside the natural ones, as counterpoint writes a minor key. */
unsigned util_scale_mask_melodic(int tonic_pitch_class, const PitchMaterial *material) {
    if (!composer_material_raises_seventh(material)) {
        return util_scale_mask(tonic_pitch_class, material);
    }
    unsigned mask = util_scale_mask_raised_seventh(tonic_pitch_class, material) | util_scale_mask(tonic_pitch_class, material);
    int sixth = pitch_material_step(material, 6);
    if (sixth >= 0) {
        mask |= 1u << util_pitch_class(tonic_pitch_class + sixth + 1);
    }
    return mask;
}

unsigned util_scale_mask_raised_seventh(int tonic_pitch_class, const PitchMaterial *material) {
    unsigned mask = util_scale_mask(tonic_pitch_class, material);
    if (!composer_material_raises_seventh(material)) {
        return mask;
    }
    int seventh = pitch_material_step(material, 7);
    if (seventh >= 0) {
        mask &= ~(1u << util_pitch_class(tonic_pitch_class + seventh));
        mask |= 1u << util_pitch_class(tonic_pitch_class + MAJOR_SEVENTH_SEMITONES);
    }
    return mask;
}

bool util_scale_degree(int tonic_pitch_class, const PitchMaterial *material, int pitch, int *degree, int *alteration) {
    return standard_tonality_material_degree_of_semitone(util_pitch_class(pitch - tonic_pitch_class), material, degree, alteration);
}

int util_scale_semitone(const PitchMaterial *material, int degree) {
    return standard_tonality_material_degree_semitone(material, degree);
}

void util_key_name(int tonic_pitch_class, const PitchMaterial *material, char *out, size_t size) {
    PitchLetter letter;
    int accidental;
    char tonic[8];
    if (!standard_tonality_key_tonic_of_pitch_class_material(util_pitch_class(tonic_pitch_class), material, &letter, &accidental) ||
        !pitch_spelling_name(letter, accidental, tonic, sizeof(tonic))) {
        snprintf(tonic, sizeof(tonic), "?");
    }
    const char *word = material == NULL ? NULL : (material->key_word != NULL ? material->key_word : material->name);
    snprintf(out, size, "%s %s", tonic, word != NULL ? word : "?");
}

bool util_key_prefers_flats(int tonic_pitch_class, const PitchMaterial *material) {
    PitchLetter letter;
    int accidental;
    int fifths = 0;
    return standard_tonality_key_tonic_of_pitch_class_material(util_pitch_class(tonic_pitch_class), material, &letter, &accidental) &&
           standard_tonality_key_fifths_of(letter, accidental, material, &fifths) && fifths < 0;
}

char *util_copy_string(const char *text) {
    size_t size = text ? strlen(text) + 1 : 1;
    char *out = malloc(size);
    if (out) {
        memcpy(out, text ? text : "", size);
    }
    return out;
}

const char *util_function_name(bool has_function, HarmonicFunction function) {
    const char *name = has_function ? harmonic_function_name(function) : NULL;
    return name ? name : "unknown";
}

/* The scale degree a pitch is spelled as in the key; the altered degrees share the degree of the natural one. */
int util_pitch_degree(const SpelledKey *key, int pitch) {
    int degree = 1;
    int alteration;
    util_scale_degree(spelled_key_tonic_pitch_class(key), key->material, pitch, &degree, &alteration);
    return degree;
}
