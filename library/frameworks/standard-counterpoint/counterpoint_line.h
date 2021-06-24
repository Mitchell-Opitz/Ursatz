#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_LINE_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_LINE_H

#include <stdbool.h>
#include <stddef.h>

#include "counterpoint_interval.h"
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
 * report a part that has gone below the other (crossing).
 */

typedef struct {
    const StandardCounterpointLine *upper;
    const StandardCounterpointLine *lower;
} StandardCounterpointPair;

/* True when the line is non-NULL, has at least one note, and every note is
   non-NULL. */
bool standard_counterpoint_line_valid(const StandardCounterpointLine *line);

/* True when both lines are valid and have the same number of notes. */
bool standard_counterpoint_pair_valid(const StandardCounterpointPair *pair);

/* The interval between notes `index` and `index + 1` of a valid line. False
   (*out untouched) for an invalid line or an index with no next note. */
bool standard_counterpoint_line_interval_at(const StandardCounterpointLine *line, size_t index,
                                            StandardCounterpointInterval *out);

/* The direction from note `index` to note `index + 1`: 1 up, -1 down, 0
   for a repeated pitch. 0 for an invalid line or an index with no next
   note. */
int standard_counterpoint_line_direction_at(const StandardCounterpointLine *line, size_t index);

/* The position of the first note at the highest pitch of a valid line (its
   climax), and of the first note at the lowest pitch. 0 for an invalid
   line. */
size_t standard_counterpoint_line_highest_index(const StandardCounterpointLine *line);
size_t standard_counterpoint_line_lowest_index(const StandardCounterpointLine *line);

#endif
