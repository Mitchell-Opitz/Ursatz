#ifndef URSATZ_MODEL_PITCH_PITCH_MATERIAL_H
#define URSATZ_MODEL_PITCH_PITCH_MATERIAL_H

#include <stdbool.h>
#include <stddef.h>

/*
 * K1 pitch material (target architecture 4.4 and 5.2), unordered: a collection of pitch classes above a center, as the knowledge entry
 * gives it. `steps` are the semitones above the center, ascending from 0. A material with a different descending form also has
 * `descending_steps` (also ascending from 0); descending_step_count is 0 when the entry gives none. `key_word` is the word after the
 * tonic in a key name ("C minor"; the entry's name when it gives none). `family` is NULL when the entry gives none; `has_brightness`
 * says `brightness_rank` is set (only a diatonic family has one). A color note is a scale degree and the alteration against major
 * (degree 3, alteration -1 for "b3"; the 7 and the #4 likewise). `variant_of` and `rotation_of` point at the material the entry
 * relates to, or are NULL. The primary and avoided chords are the degrees of the entry's chord numerals ("ii" is degree 2; the case
 * and a trailing "o" are read from the stacked triad, not kept), the characteristic degrees the steps that give the mode its color
 * (2 to 7), `cadence` the id of the cadence-kind entry the mode closes with (NULL for none); each count is 0 when the entry gives none.
 * A snapshot owns it; the strings and the pointed-to materials live as long as the snapshot.
 */
typedef struct PitchMaterial PitchMaterial;

struct PitchMaterial {
    const char *id;
    const char *name;
    const char *key_word;
    const char *family;
    bool has_brightness;
    int brightness_rank;
    size_t color_note_count;
    struct {
        int degree;
        int alteration;
    } color_notes[12];
    size_t step_count;
    int steps[12];
    size_t descending_step_count;
    int descending_steps[12];
    const PitchMaterial *variant_of;
    const PitchMaterial *rotation_of;
    size_t primary_count;
    int primary_degrees[7];
    size_t avoided_count;
    int avoided_degrees[7];
    size_t characteristic_count;
    int characteristic_degrees[7];
    const char *cadence;
};

/* The semitones above the center of degree `degree` (1 to step_count), or -1 for another degree or a NULL material. */
int pitch_material_step(const PitchMaterial *material, int degree);

/* True when `semitones_above_center` (any integer, reduced to 0 to 11) is a step of the material. */
bool pitch_material_contains(const PitchMaterial *material, int semitones_above_center);

/* The pitch classes of the material on `center` (any integer, reduced to 0 to 11) as 12 bits: bit p is pitch class p. 0 for NULL. */
unsigned pitch_material_mask(const PitchMaterial *material, int center);

/* The semitones of degree 3 above the center, or -1 for a NULL material or fewer than 3 steps. */
int pitch_material_third(const PitchMaterial *material);

/* True for a material of family "diatonic" with 7 steps. */
bool pitch_material_is_diatonic(const PitchMaterial *material);

/*
 * The degree (1 to 7) of the major scale from which the major steps rotate into this material's steps: dorian 2, phrygian 3, lydian 4,
 * mixolydian 5, natural minor 6, locrian 7, major 1. 0 for a material that is no rotation of the major scale (and for NULL). Computed
 * from the steps, so it holds for the natural minor, which names no rotation.
 */
int pitch_material_major_rotation(const PitchMaterial *material);

/* True for the natural minor: a diatonic material of brightness rank 3 (the aeolian). False for NULL and for the other modes. */
bool pitch_material_is_natural_minor(const PitchMaterial *material);

#endif
