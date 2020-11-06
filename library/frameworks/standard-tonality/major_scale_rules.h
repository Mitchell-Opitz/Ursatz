#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_MAJOR_SCALE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_MAJOR_SCALE_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Major scale rules for standard-tonality-v1:
 *
 *   STEP_PATTERN    a major scale is an ordered collection of whole (W)
 *                   and half (H) steps, ascending W-W-H-W-W-W-H
 *   NAMED_BY_TONIC  a major scale starts and ends on notes of the same
 *                   letter name one octave apart, and that note names
 *                   the scale, accidental included (B-flat major is not
 *                   B major)
 *
 * A scale here is an ordered list of PitchSpelling, eight long: the
 * seven scale notes and the tonic repeated an octave up. Steps are
 * measured in 12-TET semitones (tonal_pitch.h): half step 1, whole
 * step 2.
 *
 * ASSUMPTION: a spelled scale uses each letter once in turn (C D E F G A
 * B C), which is what makes F-flat major a scale of F-flat G-flat A-flat
 * B-double-flat ... rather than one spelled with sharps. The pattern
 * rule alone cannot tell the two apart; the check below requires the
 * letters.
 */

#define STANDARD_TONALITY_MAJOR_SCALE_STEP_COUNT 7
#define STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT 8

typedef enum {
    STANDARD_TONALITY_MAJOR_SCALE_RULE_STEP_PATTERN = 0,
    STANDARD_TONALITY_MAJOR_SCALE_RULE_NAMED_BY_TONIC,
    STANDARD_TONALITY_MAJOR_SCALE_RULE_COUNT
} StandardTonalityMajorScaleRule;

const Rule *standard_tonality_major_scale_rule(StandardTonalityMajorScaleRule which);
const Constraint *standard_tonality_major_scale_constraint(StandardTonalityMajorScaleRule which);

/* STEP_PATTERN: semitones in the index-th step: 2 (whole) or 1 (half),
   W W H W W W H. index must be < STANDARD_TONALITY_MAJOR_SCALE_STEP_COUNT. */
int standard_tonality_major_scale_step(size_t index);

/* STEP_PATTERN: true when notes (count must be 8) ascend by the major
   step pattern and use each letter once in turn, starting from the
   first note's letter. False for NULL, a NULL note, or the wrong count. */
bool standard_tonality_is_major_scale(const PitchSpelling *const *notes, size_t count);

/* NAMED_BY_TONIC: true when `last` has the same letter and accidental as
   `first` and is one octave higher. False for NULL. */
bool standard_tonality_scale_closes_on_tonic(const PitchSpelling *first, const PitchSpelling *last);

/* NAMED_BY_TONIC: true when two tonics name the same scale -- same letter
   and same accidental, whatever the octave. B-flat and B differ. False
   for NULL. */
bool standard_tonality_same_scale_name(const PitchSpelling *a, const PitchSpelling *b);

/*
 * Builds the major scale on `tonic` into out[0..7] (caller-provided
 * array of 8 pointers; the caller destroys each with
 * pitch_spelling_destroy). Returns false, leaving out untouched, when
 * tonic is NULL, on allocation failure, or when the scale would need
 * more than a double sharp or double flat.
 */
bool standard_tonality_build_major_scale(const PitchSpelling *tonic, PitchSpelling *out[STANDARD_TONALITY_MAJOR_SCALE_NOTE_COUNT]);

#endif
