#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_TONAL_KEY_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_TONAL_KEY_H

#include <stdbool.h>

#include "minor_scale_rules.h"
#include "pitch/key_signature.h"
#include "pitch/pitch_spelling.h"
#include "roman_numeral_analysis_rules.h"

/*
 * Degrees, spellings, and signatures of a key, for standard-tonality-v1.
 *
 * A key is a tonic (letter and accidental) and a mode. The four modes are
 * the major scale and the three minor forms; a bare "minor" is never
 * implied, the caller names the form. Melodic minor is the ascending
 * form (the descending form is the natural minor).
 *
 * Degrees are 1 to 7, counted by letter from the tonic. An alteration is
 * the distance in semitones from the mode's own scale degree: 0 for the
 * note in the scale, +1 for a note a semitone above it, -1 below it.
 */

typedef enum {
    STANDARD_TONALITY_MODE_MAJOR = 0,
    STANDARD_TONALITY_MODE_NATURAL_MINOR,
    STANDARD_TONALITY_MODE_HARMONIC_MINOR,
    STANDARD_TONALITY_MODE_MELODIC_MINOR
} StandardTonalityMode;

/* Semitones above the tonic of `degree` (1 to 7) in the mode: 0 for degree 1. -1 for an invalid
 * mode or degree. */
int standard_tonality_degree_semitone(StandardTonalityMode mode, int degree);

/*
 * The degree (1 to 7, by letter distance from the tonic) and alteration of
 * a note against the mode's scale. False (outputs untouched) for NULL
 * outputs, an invalid letter, or an invalid mode.
 */
bool standard_tonality_degree_of(PitchLetter tonic_letter, int tonic_accidental,
                                 StandardTonalityMode mode, PitchLetter note_letter,
                                 int note_accidental, int *degree, int *alteration);

/*
 * The note at `degree` (1 to 7) raised by `alteration` semitones. False
 * (outputs untouched) for NULL outputs, an invalid letter, mode, or
 * degree, or a spelling that would need more than a double accidental.
 */
bool standard_tonality_spell_degree(PitchLetter tonic_letter, int tonic_accidental,
                                    StandardTonalityMode mode, int degree, int alteration,
                                    PitchLetter *letter, int *accidental);

/*
 * The degree (1 to 7) of the note in the mode's scale on that tonic, 0 when the note is not in the
 * scale (letter and accidental must match) or the tonic has no such scale (a degree would need more
 * than a double accidental).
 */
int standard_tonality_scale_degree_in_mode(PitchLetter tonic_letter, int tonic_accidental,
                                           StandardTonalityMode mode, PitchLetter note_letter,
                                           int note_accidental);

/*
 * The signature of a key as sharps (positive) or flats (negative). The
 * tonic's place on the line of fifths, less 3 for a minor key. Harmonic and
 * melodic minor use the natural-minor signature. Imaginary keys (beyond
 * +-7) have a count too. False for a NULL output or an invalid letter.
 */
bool standard_tonality_key_fifths(PitchLetter tonic_letter, int tonic_accidental, bool minor,
                                  int *fifths);

/* The signature as a KeySignature (sharps F C G D A E B, flats reversed). False for imaginary keys
 * (beyond +-7), invalid input, or allocation failure. */
bool standard_tonality_key_signature_of(PitchLetter tonic_letter, int tonic_accidental, bool minor,
                                        KeySignature **out);

/* The tonic of the key with that signature count. False for NULL outputs. */
bool standard_tonality_key_of_fifths(int fifths, bool minor, PitchLetter *letter, int *accidental);

/* The tonic of the key on `pitch_class` (0..11) spelled with the fewer accidentals in its signature, sharps on a
 * tie (F# major, D# minor). A minor key is judged by its relative major. False (outputs untouched) for a
 * pitch class outside 0..11 or NULL outputs. */
bool standard_tonality_key_tonic_of_pitch_class(int pitch_class, bool minor, PitchLetter *letter,
                                                int *accidental);

/* The label of the chromatic scale degree `semitones_above_tonic` (any integer, reduced mod 12): "1" "b2" "2"
 * "b3" "3" "4" "#4" "5" "b6" "6" "b7" "7". */
const char *standard_tonality_chromatic_degree_label(int semitones_above_tonic);

/* The relative key's tonic: a minor third down for a major key, up for a minor key. */
bool standard_tonality_relative_key(PitchLetter tonic_letter, int tonic_accidental, bool minor,
                                    PitchLetter *letter, int *accidental);

/* The tonic `steps` places around the circle of fifths (positive clockwise, a fifth up). False for
 * NULL outputs or a tonic that would need more than a double accidental. */
bool standard_tonality_circle_step(PitchLetter letter, int accidental, int steps,
                                   PitchLetter *out_letter, int *out_accidental);

/* The mode of a minor form (the descending melodic form is the natural minor). False for an invalid
 * form or NULL out. */
bool standard_tonality_mode_of_minor_form(StandardTonalityMinorForm form,
                                          StandardTonalityMode *out);

/* The mode of a Roman numeral key mode: major, or the natural minor. False for an invalid value or
 * NULL out. */
bool standard_tonality_mode_of_roman_key_mode(StandardTonalityRomanKeyMode mode,
                                              StandardTonalityMode *out);

#endif
