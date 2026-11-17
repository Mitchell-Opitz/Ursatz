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
 * A key as "Db major" or "C# minor": the tonic spelled with the fewest accidentals in the key signature, sharps on a
 * tie, through the standard-tonality framework. The pitch class is taken mod 12.
 */
void util_key_name(int tonic_pitch_class, bool minor, char *out, size_t size);

/* Whether the key's signature has flats (the key is spelled with flats): the tonic's pitch class is taken mod 12. */
bool util_key_prefers_flats(int tonic_pitch_class, bool minor);

/*
 * The major scale (or the natural minor) on a tonic, read by standard-tonality: a 12-bit mask over pitch classes, and
 * the degree (1 to 7) a pitch is read as with its alteration from that degree (0 for a note of the scale). The pitch
 * and the tonic are taken mod 12.
 */
unsigned util_scale_mask(int tonic_pitch_class, bool minor);
unsigned util_scale_mask_melodic(int tonic_pitch_class, bool minor);
bool util_scale_degree(int tonic_pitch_class, bool minor, int pitch, int *degree, int *alteration);

/* The semitones above the tonic of a degree (1 to 7) in the major scale or the natural minor. */
int util_scale_semitone(bool minor, int degree);

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
