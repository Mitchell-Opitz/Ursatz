#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_HARMONIC_SPANS_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_HARMONIC_SPANS_H

/*
 * Owner of harmonic span grouping and of the bass each segment is read over (docs/analytic-job-owners.md).
 *
 * A harmonic span is the stretch of music one chord governs. Segmentation cuts at every change of sounding
 * notes, which is far finer than the rate the music changes harmony; this module reads the bass of each
 * segment (R-370) and groups segments into spans (R-369). It is pure: it reads segments and the metric level
 * the caller gives each one, and includes no other framework (the caller reads the levels with standard-meter).
 *
 * THE BASS (R-370). The bass of a segment is read at the segment's start, from the members sounding there
 * (attacked there or held into it):
 *   - the lowest sounding pitch;
 *   - unless that is 12 or more semitones above the most recent bass of the same measure, or nothing sounds:
 *     then the most recent bass persists (a bass that has stopped keeps governing while the upper parts move);
 *   - the first segment of a measure takes its lowest sounding pitch, or the previous measure's bass when
 *     nothing sounds.
 * The bass is -1 only when nothing has sounded yet.
 */

#include <stdbool.h>
#include <stddef.h>

#include "non_chord_tones.h"

/* The metric level of a segment's START. */
typedef enum {
    HARMONIC_SPAN_LEVEL_BAR = 0, /* a measure start */
    HARMONIC_SPAN_LEVEL_HALF_BAR, /* the middle of a 4/4, 2/2, 6/8 or 12/8 measure, and beat 3 of 4/4 */
    HARMONIC_SPAN_LEVEL_BEAT,     /* another beat */
    HARMONIC_SPAN_LEVEL_INNER     /* inside a beat */
} HarmonicSpanLevel;

/* One segment of the piece, in time order. segment is a view (not owned): its members are every note sounding
   in it, attacked or held. start and end are whole notes (end > start); measure numbers the measure the segment
   lies in, equal for every segment of one measure. */
typedef struct {
    const CommonPracticeSegment *segment;
    double start;
    double end;
    int measure;
    HarmonicSpanLevel level;
} HarmonicSpanCell;

/*
 * The bass of each cell at its start by the rule above, in out_bass[0..count) (semitone number, C4 = 60, or -1).
 * A member sounds at the cell start when it is held or its onset is not after the start. False, out_bass
 * untouched, for a NULL cells or out_bass with a non-zero count, or a cell without a segment; count 0 is true.
 */
bool harmonic_span_basses(const HarmonicSpanCell *cells, size_t count, int *out_bass);

#endif
