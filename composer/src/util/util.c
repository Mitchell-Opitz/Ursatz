#include "util/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <model/pitch/midi_pitch.h>
#include <model/pitch/pitch_arithmetic.h>
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
    snprintf(out, size, "%s%d", pitch_class_name(util_pitch_class(midi), prefer_flats),
             midi / PITCH_SEMITONES_PER_OCTAVE - 1);
}

bool composer_key_is_minor(const AnalysisKey *key) {
    CommonPracticeKeyMode mode;
    return key && common_practice_key_mode_of(key->material, &mode) && mode == COMMON_PRACTICE_KEY_MINOR;
}

bool composer_key_valid(const AnalysisKey *key) {
    CommonPracticeKeyMode mode;
    return key && (key->material == NULL || common_practice_key_mode_of(key->material, &mode));
}

KeyStructureMode composer_key_mode(const AnalysisKey *key) {
    return composer_key_is_minor(key) ? KEY_STRUCTURE_MODE_MINOR : KEY_STRUCTURE_MODE_MAJOR;
}

const PitchMaterial *composer_builtin_material(bool minor) {
    return common_practice_material(common_practice_materials_builtin(),
                                    minor ? COMMON_PRACTICE_KEY_MINOR : COMMON_PRACTICE_KEY_MAJOR);
}

void composer_key_set_minor(AnalysisKey *key, const CommonPracticeMaterials *materials, bool minor) {
    key->material = common_practice_material(materials, minor ? COMMON_PRACTICE_KEY_MINOR : COMMON_PRACTICE_KEY_MAJOR);
    key->mode = minor ? KEY_STRUCTURE_MODE_MINOR : KEY_STRUCTURE_MODE_MAJOR;
}

static StandardTonalityMode scale_mode(bool minor) {
    return minor ? STANDARD_TONALITY_MODE_NATURAL_MINOR : STANDARD_TONALITY_MODE_MAJOR;
}

unsigned util_scale_mask(int tonic_pitch_class, bool minor) {
    return standard_tonality_mode_pitch_classes(util_pitch_class(tonic_pitch_class), scale_mode(minor));
}

/* The scale with the raised sixth and seventh beside the natural ones, as counterpoint writes a minor key. */
unsigned util_scale_mask_melodic(int tonic_pitch_class, bool minor) {
    unsigned mask = util_scale_mask(tonic_pitch_class, minor);
    if (minor) {
        mask |= standard_tonality_mode_pitch_classes(util_pitch_class(tonic_pitch_class),
                                                     STANDARD_TONALITY_MODE_MELODIC_MINOR);
    }
    return mask;
}

bool util_scale_degree(int tonic_pitch_class, bool minor, int pitch, int *degree, int *alteration) {
    return standard_tonality_degree_of_semitone(util_pitch_class(pitch - tonic_pitch_class), scale_mode(minor), degree,
                                                alteration);
}

int util_scale_semitone(bool minor, int degree) {
    return standard_tonality_degree_semitone(scale_mode(minor), degree);
}

void util_key_name(int tonic_pitch_class, bool minor, char *out, size_t size) {
    PitchLetter letter;
    int accidental;
    char tonic[8];
    if (!standard_tonality_key_tonic_of_pitch_class(util_pitch_class(tonic_pitch_class), minor, &letter, &accidental) ||
        !pitch_spelling_name(letter, accidental, tonic, sizeof(tonic))) {
        snprintf(tonic, sizeof(tonic), "?");
    }
    snprintf(out, size, "%s %s", tonic, minor ? "minor" : "major");
}

bool util_key_prefers_flats(int tonic_pitch_class, bool minor) {
    PitchLetter letter;
    int accidental;
    int fifths = 0;
    return standard_tonality_key_tonic_of_pitch_class(util_pitch_class(tonic_pitch_class), minor, &letter, &accidental) &&
           standard_tonality_key_fifths(letter, accidental, minor, &fifths) && fifths < 0;
}

char *util_copy_string(const char *text) {
    size_t size = text ? strlen(text) + 1 : 1;
    char *out = malloc(size);
    if (out) {
        memcpy(out, text ? text : "", size);
    }
    return out;
}

bool util_read_file(const char *path, char **text, size_t *size) {
    *text = NULL;
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    long length = -1;
    if (fseek(file, 0, SEEK_END) == 0) {
        length = ftell(file);
        if (length < 0 || fseek(file, 0, SEEK_SET) != 0) {
            length = -1;
        }
    }
    char *buffer = length >= 0 ? malloc((size_t)length + 1) : NULL;
    if (buffer && fread(buffer, 1, (size_t)length, file) == (size_t)length) {
        buffer[length] = '\0';
        *text = buffer;
        if (size) {
            *size = (size_t)length;
        }
    } else {
        free(buffer);
    }
    fclose(file);
    return *text != NULL;
}

const char *util_function_name(bool has_function, HarmonicFunction function) {
    const char *name = has_function ? harmonic_function_name(function) : NULL;
    return name ? name : "unknown";
}
