#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_LINE_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_LINE_H

#include <stddef.h>

#include "pitch/pitch_spelling.h"

/*
 * A melodic line in strict counterpoint: an ordered run of sounding
 * pitches, one per note. Rhythm is not modeled: in the species each note
 * of one part is measured against the note of the other part sounding
 * with it, so two lines are compared position by position. The spellings
 * are borrowed, not owned; every entry must be non-NULL for a rule to
 * give an answer.
 */

typedef struct {
    const PitchSpelling *const *notes;
    size_t count;
} StandardCounterpointLine;

/*
 * A two-part texture: two lines of equal length, the note at position i
 * of the upper line sounding with the note at position i of the lower
 * line. "Upper" and "lower" are the parts' nominal roles; the rules
 * report a part that has gone below the other (overlapping).
 */

typedef struct {
    const StandardCounterpointLine *upper;
    const StandardCounterpointLine *lower;
} StandardCounterpointPair;

#endif
