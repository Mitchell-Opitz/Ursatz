#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_PENTATONIC_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_PENTATONIC_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Pentatonic collection rules for standard-tonality-v1. The major
 * pentatonic collection has five notes:
 *
 *   INTERVALS     the interval sequence is major 2nd, major 2nd, minor
 *                 3rd, major 2nd, minor 3rd (2 2 3 2 3 semitones)
 *   DEGREES       related to the major scale on the same tonic, it is
 *                 degrees 1 2 3 5 6, with 4 and 7 omitted
 *   SOLFEGE       do re mi sol la, then do again
 *   SPELLING      spell it by taking the solfege pattern from the major
 *                 scale on the same tonic: B-flat major pentatonic is
 *                 B-flat C D F G B-flat
 *   BLACK_KEYS    it maps exactly onto the black keys of the piano when
 *                 it begins on F-sharp (or G-flat); it can begin on any
 *                 note
 *   MINOR_ROTATION
 *                 the minor pentatonic collection is the same five notes
 *                 begun on la: do me fa sol te, the natural minor scale
 *                 without degrees 2 and 6 (steps 3 2 2 3 2)
 *
 * The minor pentatonic is not defined by the source's text; it follows
 * the standard definition (the major collection read from la) and the
 * source's minor pentatonic example.
 *
 * A collection is a list of PitchSpelling, six long: the five notes and
 * the tonic an octave up. Unlike a scale, it skips letters (B-flat C D F
 * G), so the letter advances are part of the pattern.
 */

#define STANDARD_TONALITY_PENTATONIC_STEP_COUNT 5
#define STANDARD_TONALITY_PENTATONIC_NOTE_COUNT 6

typedef enum {
    STANDARD_TONALITY_PENTATONIC_MAJOR = 0,
    STANDARD_TONALITY_PENTATONIC_MINOR,
    STANDARD_TONALITY_PENTATONIC_FORM_COUNT
} StandardTonalityPentatonicForm;

typedef enum {
    STANDARD_TONALITY_PENTATONIC_RULE_INTERVALS = 0,
    STANDARD_TONALITY_PENTATONIC_RULE_DEGREES,
    STANDARD_TONALITY_PENTATONIC_RULE_SOLFEGE,
    STANDARD_TONALITY_PENTATONIC_RULE_SPELLING,
    STANDARD_TONALITY_PENTATONIC_RULE_BLACK_KEYS,
    STANDARD_TONALITY_PENTATONIC_RULE_MINOR_ROTATION,
    STANDARD_TONALITY_PENTATONIC_RULE_COUNT
} StandardTonalityPentatonicRule;

const Rule *standard_tonality_pentatonic_rule(StandardTonalityPentatonicRule which);
const Constraint *standard_tonality_pentatonic_constraint(StandardTonalityPentatonicRule which);

/* INTERVALS / MINOR_ROTATION: the index-th step in semitones: major
   2 2 3 2 3, minor 3 2 2 3 2. index must be <
   STANDARD_TONALITY_PENTATONIC_STEP_COUNT, form valid. */
int standard_tonality_pentatonic_step(StandardTonalityPentatonicForm form, size_t index);

/* DEGREES: the index-th note's degree in the matching scale (the major
   scale for the major collection: 1 2 3 5 6; natural minor for the minor
   collection: 1 3 4 5 7). index must be <
   STANDARD_TONALITY_PENTATONIC_STEP_COUNT, form valid. */
int standard_tonality_pentatonic_degree(StandardTonalityPentatonicForm form, size_t index);

/* SOLFEGE: the syllable of the index-th note, 0..5 (5 is the tonic
   again): do re mi sol la do, or do me fa sol te do. NULL for an
   invalid form or index. */
const char *standard_tonality_pentatonic_solfege(StandardTonalityPentatonicForm form, size_t index);

/* INTERVALS / SPELLING: true when notes (count must be 6) are a
   pentatonic collection of the form: its steps in semitones and the
   letter advances of the matching scale's degrees. False for NULL, a
   NULL note, the wrong count, or an invalid form. */
bool standard_tonality_is_pentatonic(StandardTonalityPentatonicForm form, const PitchSpelling *const *notes,
                                     size_t count);

/*
 * SPELLING: builds the collection of the form on `tonic` into out[0..5]
 * by taking the matching scale's degrees (the caller provides 6
 * pointers and destroys each with pitch_spelling_destroy). False,
 * leaving out untouched, for NULL, an invalid form, allocation failure,
 * or a scale needing more than a double accidental.
 */
bool standard_tonality_build_pentatonic(StandardTonalityPentatonicForm form, const PitchSpelling *tonic,
                                        PitchSpelling *out[STANDARD_TONALITY_PENTATONIC_NOTE_COUNT]);

/* BLACK_KEYS: true when every note is a black key of the piano (12-TET
   pitch classes 1 3 6 8 10). False for NULL, count 0, or a NULL note. */
bool standard_tonality_all_black_keys(const PitchSpelling *const *notes, size_t count);

#endif
