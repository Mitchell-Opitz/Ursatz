#include "theory/tonality/tonal_key.h"

#include <stddef.h>

#include "model/pitch/pitch_arithmetic.h"
#include "model/pitch/midi_pitch.h"
#include "theory/tonality/common_practice_materials.h"

#define KEY_MAX_ACCIDENTALS 7
#define KEY_DEGREE_COUNT 7
#define KEY_MAX_ACCIDENTAL_OFFSET 2

static bool mode_valid(StandardTonalityMode mode) {
    return mode >= STANDARD_TONALITY_MODE_MAJOR && mode <= STANDARD_TONALITY_MODE_MELODIC_MINOR;
}

static bool letter_valid(PitchLetter letter) {
    return letter >= PITCH_LETTER_A && letter <= PITCH_LETTER_G;
}

static int floor_mod(int value, int modulus) {
    int result = value % modulus;
    return result < 0 ? result + modulus : result;
}

/* The material of a mode (the generated rows, until P58 B13), or NULL for an invalid mode. */
static const PitchMaterial *mode_material(StandardTonalityMode mode) {
    const CommonPracticeMaterials *materials = common_practice_materials_builtin();
    switch (mode) {
        case STANDARD_TONALITY_MODE_MAJOR:
            return materials->major;
        case STANDARD_TONALITY_MODE_NATURAL_MINOR:
            return materials->natural_minor;
        case STANDARD_TONALITY_MODE_HARMONIC_MINOR:
            return materials->harmonic_minor;
        case STANDARD_TONALITY_MODE_MELODIC_MINOR:
            return materials->melodic_minor;
        default:
            return NULL;
    }
}

static const PitchMaterial *key_material(bool minor) {
    const CommonPracticeMaterials *materials = common_practice_materials_builtin();
    return minor ? materials->natural_minor : materials->major;
}

/* The letters in the order of the major scale from C, and their natural semitones. */
static const PitchLetter LETTERS_FROM_C[KEY_DEGREE_COUNT] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                                                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};

int standard_tonality_natural_degree_semitone(int degree) {
    if (degree < 1 || degree > KEY_DEGREE_COUNT) {
        return -1;
    }
    return pitch_letter_semitone(LETTERS_FROM_C[degree - 1]);
}

int standard_tonality_material_degree_semitone(const PitchMaterial *material, int degree) {
    return pitch_material_step(material, degree);
}

int standard_tonality_degree_semitone(StandardTonalityMode mode, int degree) {
    return standard_tonality_material_degree_semitone(mode_material(mode), degree);
}

unsigned standard_tonality_material_pitch_classes(int center, const PitchMaterial *material) {
    return pitch_material_mask(material, center);
}

unsigned standard_tonality_material_pitch_classes_with_leading_tone(int center, const PitchMaterial *material) {
    unsigned mask = pitch_material_mask(material, center);
    if (mask == 0) {
        return 0;
    }
    return mask | (1u << ((floor_mod(center, PITCH_SEMITONES_PER_OCTAVE) + 11) % PITCH_SEMITONES_PER_OCTAVE));
}

unsigned standard_tonality_mode_pitch_classes(int tonic_pitch_class, StandardTonalityMode mode) {
    if (!mode_valid(mode) || tonic_pitch_class < 0 || tonic_pitch_class >= PITCH_SEMITONES_PER_OCTAVE) {
        return 0;
    }
    return standard_tonality_material_pitch_classes(tonic_pitch_class, mode_material(mode));
}

unsigned standard_tonality_mode_pitch_classes_with_leading_tone(int tonic_pitch_class, StandardTonalityMode mode) {
    if (!mode_valid(mode) || tonic_pitch_class < 0 || tonic_pitch_class >= PITCH_SEMITONES_PER_OCTAVE) {
        return 0;
    }
    return standard_tonality_material_pitch_classes_with_leading_tone(tonic_pitch_class, mode_material(mode));
}

/* The degree of `steps` at or below `inside` (0 to 11) and the semitones above it. */
static void locate(const int *steps, size_t count, int inside, int *degree, int *alteration) {
    size_t found = 0;
    for (size_t d = 0; d < count; d++) {
        if (steps[d] <= inside) {
            found = d;
        }
    }
    *degree = (int)found + 1;
    *alteration = inside - steps[found];
}

/* A chromatic semitone with no context is read as the next degree lowered where the two spellings are
 * a semitone from the neighbors on both sides (R-285): semitone 1 in every material, and where the third is major and the sixth and
 * seventh are 9 and 11 (the major scale) also 3, 8 and 10. The sharp readings that stay are the raised fourth, and the natural
 * minor's #3, #6 and #7. */
static bool semitone_reads_lowered(const PitchMaterial *material, int inside) {
    if (inside == 1) {
        return true;
    }
    return pitch_material_third(material) == 4 && pitch_material_step(material, 6) == 9 &&
           pitch_material_step(material, 7) == 11 && (inside == 3 || inside == 8 || inside == 10);
}

bool standard_tonality_material_degree_at_or_below_semitone(int semitones_above_center, const PitchMaterial *material, int *degree,
                                                            int *alteration) {
    if (degree == NULL || alteration == NULL || material == NULL || material->step_count == 0) {
        return false;
    }
    locate(material->steps, material->step_count, floor_mod(semitones_above_center, PITCH_SEMITONES_PER_OCTAVE), degree, alteration);
    return true;
}

bool standard_tonality_material_degree_of_semitone(int semitones_above_center, const PitchMaterial *material, int *degree,
                                                   int *alteration) {
    if (!standard_tonality_material_degree_at_or_below_semitone(semitones_above_center, material, degree, alteration)) {
        return false;
    }
    if (semitone_reads_lowered(material, floor_mod(semitones_above_center, PITCH_SEMITONES_PER_OCTAVE))) {
        *degree += 1;
        *alteration = -1;
    }
    return true;
}

bool standard_tonality_degree_at_or_below_semitone(int semitones_above_tonic, StandardTonalityMode mode, int *degree,
                                                   int *alteration) {
    return standard_tonality_material_degree_at_or_below_semitone(semitones_above_tonic, mode_material(mode), degree, alteration);
}

bool standard_tonality_degree_of_semitone(int semitones_above_tonic, StandardTonalityMode mode, int *degree,
                                          int *alteration) {
    return standard_tonality_material_degree_of_semitone(semitones_above_tonic, mode_material(mode), degree, alteration);
}

bool standard_tonality_natural_degree_of_semitone(int semitones_above_tonic, int *degree, int *alteration) {
    if (degree == NULL || alteration == NULL) {
        return false;
    }
    int steps[KEY_DEGREE_COUNT];
    for (int d = 0; d < KEY_DEGREE_COUNT; d++) {
        steps[d] = pitch_letter_semitone(LETTERS_FROM_C[d]);
    }
    int inside = floor_mod(semitones_above_tonic, PITCH_SEMITONES_PER_OCTAVE);
    locate(steps, KEY_DEGREE_COUNT, inside, degree, alteration);
    if (inside == 1 || inside == 3 || inside == 8 || inside == 10) {
        *degree += 1;
        *alteration = -1;
    }
    return true;
}

/* The material whose own steps give the signature: itself when diatonic, else its variant_of when that is. */
static const PitchMaterial *signature_material(const PitchMaterial *material) {
    if (pitch_material_is_diatonic(material)) {
        return material;
    }
    if (material != NULL && pitch_material_is_diatonic(material->variant_of)) {
        return material->variant_of;
    }
    return NULL;
}

static int letter_index(PitchLetter letter) {
    for (int i = 0; i < KEY_DEGREE_COUNT; i++) {
        if (LETTERS_FROM_C[i] == letter) {
            return i;
        }
    }
    return 0;
}

/* The sum of the accidentals of the seven steps of a diatonic material spelled up from the tonic's letter, one letter a degree. */
static int signature_sum(PitchLetter tonic_letter, int tonic_accidental, const PitchMaterial *diatonic) {
    int tonic = letter_index(tonic_letter);
    int tonic_semitone = pitch_letter_semitone(tonic_letter) + tonic_accidental;
    int sum = 0;
    for (int d = 0; d < KEY_DEGREE_COUNT; d++) {
        int index = tonic + d;
        int natural = pitch_letter_semitone(LETTERS_FROM_C[index % KEY_DEGREE_COUNT]) +
                      PITCH_SEMITONES_PER_OCTAVE * (index / KEY_DEGREE_COUNT);
        sum += tonic_semitone + diatonic->steps[d] - natural;
    }
    return sum;
}

bool standard_tonality_key_fifths_of(PitchLetter tonic_letter, int tonic_accidental, const PitchMaterial *material, int *fifths) {
    const PitchMaterial *diatonic = signature_material(material);
    if (fifths == NULL || !letter_valid(tonic_letter) || diatonic == NULL) {
        return false;
    }
    *fifths = signature_sum(tonic_letter, tonic_accidental, diatonic);
    return true;
}

bool standard_tonality_key_fifths(PitchLetter tonic_letter, int tonic_accidental, bool minor,
                                  int *fifths) {
    return standard_tonality_key_fifths_of(tonic_letter, tonic_accidental, key_material(minor), fifths);
}

bool standard_tonality_key_signature_of_material(PitchLetter tonic_letter, int tonic_accidental, const PitchMaterial *material,
                                                 KeySignature **out) {
    int fifths;
    if (out == NULL ||
        !standard_tonality_key_fifths_of(tonic_letter, tonic_accidental, material, &fifths) ||
        fifths < -KEY_MAX_ACCIDENTALS || fifths > KEY_MAX_ACCIDENTALS) {
        return false;
    }
    return key_signature_from_fifths(fifths, out);
}

bool standard_tonality_key_signature_of(PitchLetter tonic_letter, int tonic_accidental, bool minor,
                                        KeySignature **out) {
    return standard_tonality_key_signature_of_material(tonic_letter, tonic_accidental, key_material(minor), out);
}

bool standard_tonality_key_of_fifths_material(int fifths, const PitchMaterial *material, PitchLetter *letter, int *accidental) {
    const PitchMaterial *diatonic = signature_material(material);
    if (letter == NULL || accidental == NULL || diatonic == NULL) {
        return false;
    }
    /* The signature of the material on C is its offset from the tonic's place on the line of fifths. */
    pitch_spelling_from_fifths(fifths - signature_sum(PITCH_LETTER_C, 0, diatonic), letter, accidental);
    return true;
}

bool standard_tonality_key_of_fifths(int fifths, bool minor, PitchLetter *letter, int *accidental) {
    return standard_tonality_key_of_fifths_material(fifths, key_material(minor), letter, accidental);
}

const char *standard_tonality_chromatic_degree_label(int semitones_above_tonic) {
    static const char *const LABELS[12] = {"1", "b2", "2", "b3", "3", "4", "#4", "5", "b6", "6", "b7", "7"};
    return LABELS[pitch_class_of_midi(semitones_above_tonic)];
}

/* Major tonics by signature size 0 to 7, sharps then flats (circle of fifths). */
static const int SHARP_MAJOR_TONICS[8] = {0, 7, 2, 9, 4, 11, 6, 1};
static const int FLAT_MAJOR_TONICS[8] = {0, 5, 10, 3, 8, 1, 6, 11};

static int signature_size(const int tonics[8], int pitch_class) {
    for (int size = 0; size < 8; size++) {
        if (tonics[size] == pitch_class) {
            return size;
        }
    }
    return 99;
}

/* The semitones to add to a tonic of the material to reach the tonic of the major scale with the same notes, or -1 for none. */
static int relative_major_offset(const PitchMaterial *diatonic, const PitchMaterial *major, int tonic) {
    unsigned notes = pitch_material_mask(diatonic, tonic);
    for (int offset = 0; offset < PITCH_SEMITONES_PER_OCTAVE; offset++) {
        if (pitch_material_mask(major, tonic + offset) == notes) {
            return offset;
        }
    }
    return -1;
}

bool standard_tonality_key_tonic_of_pitch_class_material(int pitch_class, const PitchMaterial *material, PitchLetter *letter,
                                                         int *accidental) {
    const PitchMaterial *diatonic = signature_material(material);
    if (pitch_class < 0 || pitch_class > 11 || letter == NULL || accidental == NULL || diatonic == NULL) {
        return false;
    }
    int offset = relative_major_offset(diatonic, common_practice_materials_builtin()->major, pitch_class);
    if (offset < 0) {
        return false;
    }
    int major_pitch_class = (pitch_class + offset) % PITCH_SEMITONES_PER_OCTAVE;
    int sharps = signature_size(SHARP_MAJOR_TONICS, major_pitch_class);
    int flats = signature_size(FLAT_MAJOR_TONICS, major_pitch_class);
    return standard_tonality_key_of_fifths_material(flats < sharps ? -flats : sharps, material, letter, accidental);
}

bool standard_tonality_key_tonic_of_pitch_class(int pitch_class, bool minor, PitchLetter *letter,
                                                int *accidental) {
    return standard_tonality_key_tonic_of_pitch_class_material(pitch_class, key_material(minor), letter, accidental);
}
