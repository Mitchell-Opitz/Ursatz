#ifndef URSATZ_COMPOSER_UTIL_H
#define URSATZ_COMPOSER_UTIL_H

#include <stdbool.h>
#include <stddef.h>

#include <theory/tonality/spelled_key.h>
#include <theory/harmony/harmonic_function.h>
#include <theory/tonality/common_practice_materials.h>
#include <theory/tonality/modal_chords.h>

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
 * natural minor, the major scale itself for a major. A minor key's scale over a chord that holds the leading tone. The other
 * modes keep their own seventh: the plain scale (R-725).
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
 * Whether the Composer composes in the material: the major, the natural minor or one of the church modes (a modal key,
 * pitch_material_is_modal_key), and no other (the harmonic and the melodic minor are forms of the minor key, the synthetic scales are
 * references). The one check every key text passes.
 */
bool composer_material_supported(const CommonPracticeMaterials *materials, const PitchMaterial *material);

/*
 * Whether two materials are the same mode of a key: the same material, or both major or natural minor read by their third (a D natural
 * minor motive is not a D dorian one). NULL reads as major, as the zero key does. composer_key_same_mode is the same for keys.
 */
bool composer_material_same_mode(const PitchMaterial *a, const PitchMaterial *b);
bool composer_key_same_mode(const SpelledKey *a, const SpelledKey *b);

/* Whether a counting by mode (the predominants a mode has, the cells a phrase needs) reads the material as minor: a minor third, or any modal key (a run of one). */
bool composer_material_counts_as_minor(const PitchMaterial *material);

/* The word after the tonic of the key's name: "major", "minor", or the church mode ("dorian"). */
const char *composer_key_word(const SpelledKey *key);

/* The pitch class of the default tonic of a material: that of the major degree from which the major steps rotate into it (dorian D, natural minor A), C for the rest. */
int composer_material_default_tonic(const PitchMaterial *material);

/*
 * Whether a key of the material raises its seventh degree to a leading tone where a chord holds it: the natural minor only
 * (pitch_material_is_natural_minor). The modes keep their own seventh, whatever the third they have.
 */
bool composer_material_raises_seventh(const PitchMaterial *material);

/*
 * Whether the key reads as a minor key: its material has a minor third (common_practice_key_mode_of). False for a key with no
 * material (the zero key reads as major, as a request left at its defaults holds) or a material that is neither major nor
 * minor; the Composer writes keys that have one. Every read of a key's mode goes through this.
 */
bool composer_key_is_minor(const SpelledKey *key);

/* Makes the key of the material (the tonic spelling stays). The material is the major or the natural minor of the call's materials. */
void composer_key_set_material(SpelledKey *key, const PitchMaterial *material);

/* The name of a chord's harmonic function, "unknown" for a chord with none. */
const char *util_function_name(bool has_function, HarmonicFunction function);

/* A malloc'd copy of the text ("" for NULL), or NULL when out of memory. The caller frees it. */
char *util_copy_string(const char *text);

/* The scale degree a pitch is spelled as in the key; the altered degrees share the degree of the natural one. */
int util_pitch_degree(const SpelledKey *key, int pitch);

#endif
