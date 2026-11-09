#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_REPEATS_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_REPEATS_H

/*
 * Owner of written-out repeats and melodic returns over measures (standard_form_find_repeats) (docs/analytic-job-owners.md,
 * Analytic job owners). Nothing here calls the chain; a later branch calls it from the chain's key work (R-400).
 *
 * THE INPUT is the notes of a piece, each with its span and its semitone, borrowed; the caller chooses which notes (the whole
 * texture finds repeats, the melody alone is not asked for). A note belongs to the measure of its start. A measure's CONTENT is
 * its notes as (onset minus the measure start, semitone, duration), sorted by onset, then semitone, then duration; its SOPRANO is
 * the highest semitone among the notes starting at each onset, in onset order. Times are compared exactly, as Rationals. A measure
 * with no notes matches nothing.
 *
 * A REPEAT is a maximal run of at least min_measures measures whose content equals that of an earlier run, measure for measure,
 * (a, b), (a + 1, b + 1) ..., that is not the continuation of a run at (a - 1, b - 1), with b - a at least the run length (no
 * overlap). Scanning earlier measures from the first and later ones after each, a later measure is in at most one repeat: the
 * first found keeps it.
 *
 * A RETURN is a maximal run of at least min_measures measure pairs whose sopranos have equal onsets and differ by one constant
 * semitone count over the whole run (0 and multiples of 12 included), that is not the continuation of a run at (a - 1, b - 1),
 * with b - a at least the run length, and whose pairs are not all exact content repeats. A return is dropped when implied: its
 * later span lies inside a REPEAT's later span and the same return exists from that repeat's source, or its earlier span lies
 * inside a REPEAT's later span and the same return exists from the repeat's earlier span ("the same" is read pair by pair over
 * the shifted measures: the same sopranos, the same semitone count).
 *
 * OUTPUT. Items ordered by later_first, then earlier_first, repeats and returns together. Measure numbers are the MeterMap's.
 * transposition is the later soprano minus the earlier, in semitones; 0 for a repeat.
 */

#include <stddef.h>

#include "analysis_input.h"
#include "time/meter_map.h"
#include "time/time_span.h"

typedef struct {
    const TimeSpan *span; /* borrowed */
    long semitone;
} StandardFormRepeatNote;

typedef enum { STANDARD_FORM_REPEAT = 0, STANDARD_FORM_RETURN } StandardFormRepeatKind;

typedef struct {
    StandardFormRepeatKind kind;
    int earlier_first;
    int earlier_last;
    int later_first;
    int later_last;
    int transposition; /* semitones, later minus earlier; 0 for a repeat */
} StandardFormRepeat;

typedef struct {
    StandardFormRepeat *items; /* owned; NULL when none */
    size_t count;
} StandardFormRepeats;

typedef struct {
    size_t min_measures; /* the shortest run found, at least 1; default 4 */
} StandardFormRepeatConfig;

/* Finds the repeats and returns. `config` NULL for the default. NULL_ARGUMENT for a NULL notes with count > 0, a NULL map or a NULL
   out; BAD_INPUT for a note without a span or a min_measures of 0; METER for a time the map cannot place. On OK, *out is owned by the
   caller (standard_form_repeats_destroy); otherwise untouched. No notes: no items. */
StandardFormAnalysisStatus standard_form_find_repeats(const StandardFormRepeatNote *notes, size_t count, const MeterMap *map,
                                                      const StandardFormRepeatConfig *config, StandardFormRepeats *out);
void standard_form_repeats_destroy(StandardFormRepeats *repeats);

#endif
