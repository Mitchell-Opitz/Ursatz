#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_TONAL_KEY_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_TONAL_KEY_H

#include <stdbool.h>

#include "model/pitch/key_signature.h"
#include "model/pitch/pitch_material.h"
#include "model/pitch/pitch_spelling.h"
#include "theory/tonality/roman_numeral_analysis_rules.h"

/*
 * Degrees, spellings, and signatures of a key, for standard-tonality-v1.
 *
 * A key is a tonic (letter and accidental) and a pitch material (the steps of a knowledge entry: the major scale, a minor form, a
 * mode). A bare "minor" is never implied, the caller names the material.
 *
 * Degrees are 1 to 7, counted by letter from the tonic. An alteration is
 * the distance in semitones from the material's own scale degree: 0 for the
 * note in the scale, +1 for a note a semitone above it, -1 below it.
 */

/*
 * Readings from a pitch material (the steps of a knowledge entry). A material has the steps it names (degrees 1 to step_count); a
 * key of it is a tonic and the material.
 */

/* The label of the chromatic scale degree `semitones_above_tonic` (any integer, reduced mod 12): "1" "b2" "2"
 * "b3" "3" "4" "#4" "5" "b6" "6" "b7" "7". */
const char *standard_tonality_chromatic_degree_label(int semitones_above_tonic);

/* Semitones above the center of `degree` of the material, or -1 for a NULL material or a degree it does not have. */
int standard_tonality_material_degree_semitone(const PitchMaterial *material, int degree);

/* The material on `center` (any integer, reduced to 0 to 11) as a 12-bit mask over pitch classes; 0 for NULL (pitch_material_mask). */
unsigned standard_tonality_material_pitch_classes(int center, const PitchMaterial *material);

/* The next pitch above (direction > 0) or below (direction < 0) `pitch` whose pitch class is in `mask` (bit n set for pitch class n,
   as the two masks below make). The pitch itself for a mask with no pitch class, or for a direction of 0. A caller whose mask is
   relative to a tonic passes the pitch above the tonic and adds the tonic back. */
int standard_tonality_mask_step(unsigned mask, int pitch, int direction);

/* The same mask plus the leading tone, pitch class center + 11. 0 for NULL. */
unsigned standard_tonality_material_pitch_classes_with_leading_tone(int center, const PitchMaterial *material);

/*
 * The degree (1 to 7) at or below a chromatic semitone above the center in the material, and the semitones above that degree: 0 for a
 * note of the scale, else the note read as the next lower degree raised (+1; +2 where the material has a gap of a minor third, as the
 * harmonic minor's sixth to seventh). Any whole number of semitones is taken mod 12. False (outputs untouched) for NULL arguments or a
 * material with no steps.
 */
bool standard_tonality_material_degree_at_or_below_semitone(int semitones_above_center, const PitchMaterial *material, int *degree,
                                                            int *alteration);

/*
 * The degree (1 to 7) a chromatic semitone above the center is read as in the material, and its alteration from that degree: 0 for a
 * note of the scale. With no context a chromatic note is read as the next lower degree raised, except the lowered readings (-1 on
 * the next degree up): semitone 1 always, and semitones 3, 8 and 10 where the material's third is major and its sixth and seventh
 * are 9 and 11 (the major scale's rule, R-285). False (outputs untouched) for NULL arguments or a material with no steps.
 */
bool standard_tonality_material_degree_of_semitone(int semitones_above_center, const PitchMaterial *material, int *degree,
                                                   int *alteration);

/*
 * The signature of a key of the material on a tonic: the sum of the accidentals of its scale spelled up from the tonic's letter
 * (the tonic's place on the line of fifths, less 3 for natural minor; D dorian 0, Bb mixolydian -3). A diatonic material (family
 * "diatonic", 7 steps) has its own; any other uses the signature of its variant_of (harmonic and melodic minor: the natural
 * minor's). False for a NULL output, an invalid letter, or a material that is neither diatonic nor a variant of a diatonic one.
 */
bool standard_tonality_key_fifths_of(PitchLetter tonic_letter, int tonic_accidental, const PitchMaterial *material, int *fifths);

/* The signature of a key of the material on a tonic as a KeySignature (sharps F C G D A E B, flats reversed). False for imaginary keys
 * (beyond +-7), invalid input, a material with no signature, or allocation failure. */
bool standard_tonality_key_signature_of_material(PitchLetter tonic_letter, int tonic_accidental, const PitchMaterial *material,
                                                 KeySignature **out);

/* The tonic of the key of the material with that signature count. False for NULL outputs or a material with no signature. */
bool standard_tonality_key_of_fifths_material(int fifths, const PitchMaterial *material, PitchLetter *letter, int *accidental);

/*
 * The tonic of the key of the material on `pitch_class` (0..11) spelled with the fewer accidentals in its signature, sharps on a tie
 * (F# major, D# minor), judged by the major scale of the same notes. False (outputs untouched) for a pitch class outside 0..11, NULL
 * outputs, or a material with no signature.
 */
bool standard_tonality_key_tonic_of_pitch_class_material(int pitch_class, const PitchMaterial *material, PitchLetter *letter,
                                                         int *accidental);

/* The semitones above the tonic of `degree` (1 to 7) of the major scale, spelled by letter (C D E F G A B): 0 2 4 5 7 9 11. -1 for
 * another degree. For the sites that read a degree against the major scale with no key. */
int standard_tonality_natural_degree_semitone(int degree);

/* standard_tonality_material_degree_of_semitone against the major scale. False (outputs untouched) for NULL outputs. */
bool standard_tonality_natural_degree_of_semitone(int semitones_above_tonic, int *degree, int *alteration);

#endif
