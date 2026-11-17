#ifndef URSATZ_COMPOSER_UTIL_H
#define URSATZ_COMPOSER_UTIL_H

#include <stdbool.h>
#include <stddef.h>

#include <analysis/notes/analysis_key.h>
#include <theory/harmony/harmonic_function.h>
#include <theory/tonality/common_practice_materials.h>

/* Small helpers the sources share. Private: not installed, not part of the API. */

/* The absolute value of an int. */
int util_absolute(int value);

/* How many bits are set. */
unsigned util_bits_set(unsigned bits);

/* -1, 0 or 1 by the sign of the value. */
int util_sign(int value);

/* The pitch class, 0 to 11, of a MIDI pitch or any integer (PITCH_SEMITONES_PER_OCTAVE). */
int util_pitch_class(int pitch);

/* A MIDI pitch as "C#4" ("Db4" when prefer_flats): the library's pitch class name and the octave of middle C = 4. */
void util_pitch_name(int midi, bool prefer_flats, char *out, size_t size);

/*
 * A key as "Db major" or "C# minor": the tonic spelled with the fewest accidentals in the material's key signature, sharps on a
 * tie, then the material's key word (the third of the material decides the signature, as in standard-tonality). The pitch class is
 * taken mod 12. A NULL material gives "?" for the tonic and the word.
 */
void util_key_name(int tonic_pitch_class, const PitchMaterial *material, char *out, size_t size);

/* Whether the key's signature has flats (the key is spelled with flats): the tonic's pitch class is taken mod 12. */
bool util_key_prefers_flats(int tonic_pitch_class, const PitchMaterial *material);

/*
 * The scale of a key's material on a tonic, read by standard-tonality: a 12-bit mask over pitch classes, and the degree (1 to 7) a
 * pitch is read as with its alteration from that degree (0 for a note of the scale). The pitch and the tonic are taken mod 12.
 * util_scale_mask_melodic adds, for a minor material, its sixth and seventh raised a semitone (the natural scale and the melodic
 * one as counterpoint writes a minor key); a major material gives its own mask.
 */
unsigned util_scale_mask(int tonic_pitch_class, const PitchMaterial *material);
unsigned util_scale_mask_melodic(int tonic_pitch_class, const PitchMaterial *material);
bool util_scale_degree(int tonic_pitch_class, const PitchMaterial *material, int pitch, int *degree, int *alteration);

/*
 * The material's scale with its seventh degree raised to the leading tone (the semitone below the octave): the harmonic minor of a
 * natural minor, the major scale itself for a major. A minor key's scale over a chord that holds the leading tone.
 */
unsigned util_scale_mask_raised_seventh(int tonic_pitch_class, const PitchMaterial *material);

/* The semitones above the tonic of a degree (1 to 7) of the material; -1 for a NULL material or a degree it does not have. */
int util_scale_semitone(const PitchMaterial *material, int degree);

/* The semitones from the tonic to the third of the material: 3 or 4. A material that is neither (or NULL) reads as major, as the zero key does. */
int composer_material_third(const PitchMaterial *material);

/* Whether the material reads as a minor key: its third is a minor third (common_practice_key_mode_of). False for NULL. */
bool composer_material_is_minor(const PitchMaterial *material);

/* Whether the material has a major third or a minor third (common_practice_key_mode_of); false for NULL. */
bool composer_material_valid(const PitchMaterial *material);

/*
 * Whether the Composer composes in the material: the major or the natural minor of the materials, and no other (the harmonic and
 * the melodic minor are forms of the minor key, the modes and the rest are not keys it writes). The one check every key text passes.
 */
bool composer_material_supported(const CommonPracticeMaterials *materials, const PitchMaterial *material);

/*
 * Whether the key reads as a minor key: its material has a minor third (common_practice_key_mode_of). False for a key with no
 * material (the zero key reads as major, as a request left at its defaults holds) or a material that is neither major nor
 * minor; the Composer writes keys that have one (P58 B13). Every read of a key's mode goes through this.
 */
bool composer_key_is_minor(const AnalysisKey *key);

/*
 * The builtin major (or natural minor) material, common_practice_materials_builtin's, for code that holds a mode and no materials.
 * TRANSITIONAL: goes with the mode types and the builtin materials (P58 B13).
 */
const PitchMaterial *composer_builtin_material(bool minor);

/*
 * Makes the key a major or a minor key, keeping its tonic spelling: its material becomes the major or the natural minor of
 * materials and its mode follows. The one way the Composer writes a mode (P58 B13).
 */
void composer_key_set_minor(AnalysisKey *key, const CommonPracticeMaterials *materials, bool minor);

/* Makes the key of the material (the tonic spelling stays); its mode follows. The material is the major or the natural minor of the call's materials. */
void composer_key_set_material(AnalysisKey *key, const PitchMaterial *material);

/*
 * Whether the key has a material the Composer writes: a major third or a minor third (common_practice_key_mode_of). The zero key
 * (C, no material) is valid and reads as C major, which a request left at its defaults holds.
 */
bool composer_key_valid(const AnalysisKey *key);

/*
 * The key's mode as the code still written on KeyStructureMode reads it: KEY_STRUCTURE_MODE_MINOR when composer_key_is_minor,
 * else KEY_STRUCTURE_MODE_MAJOR. TRANSITIONAL: goes with the mode types (P58 B13).
 */
KeyStructureMode composer_key_mode(const AnalysisKey *key);

/* The name of a chord's harmonic function, "unknown" for a chord with none. */
const char *util_function_name(bool has_function, HarmonicFunction function);

/* A malloc'd copy of the text ("" for NULL), or NULL when out of memory. The caller frees it. */
char *util_copy_string(const char *text);

/*
 * Reads the whole file at path into a NUL-terminated string in *text (the caller frees it); *size, when not NULL, is
 * its length without the terminator. False, with *text NULL, when the file cannot be read.
 */
bool util_read_file(const char *path, char **text, size_t *size);

#endif
