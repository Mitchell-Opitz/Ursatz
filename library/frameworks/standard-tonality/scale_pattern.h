#ifndef URSATZ_FRAMEWORKS_STANDARD_TONALITY_SCALE_PATTERN_H
#define URSATZ_FRAMEWORKS_STANDARD_TONALITY_SCALE_PATTERN_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"

/*
 * Spelled scale pattern helper for standard-tonality-v1: checks and
 * builds a scale from a list of step sizes in 12-TET semitones (half
 * step 1, whole step 2, three half steps 3). Major and minor scales
 * differ only in their step list, so they share this code.
 *
 * A scale of n steps is n + 1 notes: the scale notes and the tonic
 * repeated an octave up.
 *
 * ASSUMPTION: a spelled scale uses each letter once in turn (C D E F G A
 * B C). The step sizes alone cannot tell E-sharp from F; the letters
 * can. This holds for the harmonic minor's three-half-step gap too: the
 * gap is a raised seventh a letter above, not a skipped letter.
 */

/* True when notes (count == step_count + 1) ascend by exactly steps and
   use each letter once in turn from the first note's letter. False for
   NULL, a NULL note, or the wrong count. */
bool standard_tonality_is_scale_pattern(const int *steps, size_t step_count, const PitchSpelling *const *notes,
                                        size_t count);

/*
 * Builds the scale of step_count + 1 notes on `tonic` into out (the
 * caller provides room for step_count + 1 pointers and destroys each
 * with pitch_spelling_destroy). Returns false, leaving out untouched,
 * when an argument is NULL, on allocation failure, or when the scale
 * would need more than a double sharp or double flat.
 */
bool standard_tonality_build_scale_pattern(const int *steps, size_t step_count, const PitchSpelling *tonic,
                                           PitchSpelling **out);

#endif
