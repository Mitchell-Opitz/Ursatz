#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_PHRASE_GRID_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_PHRASE_GRID_H

/*
 * Owner of the phrase grid (standard_form_find_phrase_grid) (docs/analytic-job-owners.md, Analytic job owners).
 */

#include <stdbool.h>
#include <stddef.h>

#include "analysis_input.h"
#include "analysis_repeats.h"

/*
 * The phrase grid (R-389): a piece is first cut into units of 4, 8 or 16 measures, the phrases in Michelle's sense, and only
 * then confirmed by the cadences at their ends. The grid is a tiling of the measures from the first to the last in which a
 * melody note starts; it is chosen by dynamic programming from the evidence at each unit's end. Nothing here calls the chain;
 * the grid is an input of standard_form_segment_phrases_on_grid (analysis_phrase.h) and standard_form_identify_themes_on_grid
 * (analysis_theme.h).
 *
 * THE INPUT is a finished StandardFormInput. Its notes are the MELODY, one note per attack (the caller builds it; the chain
 * takes the highest sounding note at each attack). Its cadences are the CANDIDATES: the found cadences, both readings of a
 * two-reading cadence, and the half readings of standard-harmony (harmony_analysis_half_readings). The grid reads each
 * cadence's arrival span, its type and nothing else (not strength_rank, not post_cadential).
 *
 * THE EVIDENCE of a measure m, from the first measure in which a note starts to the last (constants are measured, not
 * fitted to a piece; a constant is never changed to make a test pass):
 *
 *   cadence(m)  the largest of 1.0 for a PERFECT_AUTHENTIC cadence, and 0.8 for an IMPERFECT_AUTHENTIC,
 *               AUTHENTIC_UNSPECIFIED or HALF cadence, whose arrival starts in m, or is held into m (its arrival span starts in
 *               an earlier measure and reaches m; an arrival ending exactly on a barline does not reach the next measure).
 *               Every other type gives nothing
 *   long(m)     0.3 when the last melody note starting in m lasts at least half of the measure
 *   rest(m)     0.3 when no melody note sounds in the last half beat of m (the beat of the MeterMap at m)
 *   return(m)   0.3 when the melody notes starting in m + 1 and m + 2, at least 6 of them, repeat an earlier pair of measures:
 *               for some measure b from the first measure to m - 5, the notes starting in b and b + 1 are as many, their
 *               onsets (relative to the start of b, and of m + 1) are equal within 1e-6 whole notes, and so are the semitone
 *               steps between neighbouring notes
 *   evidence(m) the sum of the four
 *
 * THE UNITS. A unit [f, e] has the length L = e - max(f, first counted measure) + 1 (the pickup, measure 0, is not counted;
 * counted_first is max(f, first counted measure)), 2 to 16. Its score is
 *
 *   evidence(e) - 0.3                 every unit costs 0.3
 *   - 0.6                             when L is not 4, 8 or 16
 *   - 0.1                             when the unit is elided: it starts on the previous unit's last measure
 *   + cadence(half)                   when L is 8 or 16, the cadence evidence at its half measure,
 *                                     half = counted_first + L / 2 - 1 (an antecedent ends there)
 *
 * The first unit starts at the first measure. The next unit starts at e + 1, or, when an authentic cadence (PERFECT_AUTHENTIC,
 * IMPERFECT_AUTHENTIC, AUTHENTIC_UNSPECIFIED) arrives in e or is held into it, it may also start at e (an elision: the
 * arrival bar is the next unit's first). The last unit ends at the last measure. The grid is the tiling of the highest total
 * score, found by dynamic programming over the states (start measure, elided) in ascending order, elided false first, unit ends
 * ascending, a later candidate replacing an earlier only when strictly higher (by more than 1e-9), so it is deterministic: of two
 * tilings of equal total the one reached first wins (the earlier final start, the shorter earlier units). A piece with no notes,
 * or fewer than two measures, has no units.
 *
 * OUTPUT. units in time order; StandardFormGridUnit.half_measure is -1 for a unit of 4 or any other length. The elided flag is
 * the unit's, not its predecessor's.
 */

typedef struct {
    int first_measure;   /* the unit's first measure; the previous unit's last when elided */
    int counted_first;   /* first_measure, or the first counted measure for a unit starting on the pickup */
    int last_measure;
    int length_measures; /* last_measure - counted_first + 1 */
    bool elided;         /* starts on the previous unit's last measure */
    int half_measure;    /* counted_first + length_measures / 2 - 1 for a unit of 8 or 16, else -1 */
} StandardFormGridUnit;

typedef struct {
    StandardFormGridUnit *units; /* owned; NULL when none */
    size_t count;
} StandardFormPhraseGrid;

/* Finds the grid of a finished input. NULL_ARGUMENT for a NULL argument or an input without a MeterMap; BAD_INPUT for an input not
   finished. On OK, *out is owned by the caller (standard_form_phrase_grid_destroy); otherwise untouched. No notes: no units. */
StandardFormAnalysisStatus standard_form_find_phrase_grid(const StandardFormInput *input, StandardFormPhraseGrid *out);
void standard_form_phrase_grid_destroy(StandardFormPhraseGrid *grid);

/* Lets the grid follow written-out repeats (R-400). For each REPEAT of `repeats` (returns are ignored), in order: when the earlier
   span is tiled by whole units (the first unit starts on earlier_first, or is elided and starts on earlier_first - 1; a unit ends
   on earlier_last), the unit before the later span ends on later_first - 1 and a unit ends on later_last, the units after that
   unit up to the one ending on later_last are replaced by copies of the earlier span's units moved by later_first - earlier_first
   (first, counted first, last and half measures; the elided flag and the lengths kept). Otherwise the grid is unchanged for that
   repeat. A repeat whose earlier span does not end before its later span starts is skipped. NULL_ARGUMENT for a NULL argument;
   ALLOC on failure, the grid untouched. */
StandardFormAnalysisStatus standard_form_phrase_grid_follow_repeats(StandardFormPhraseGrid *grid,
                                                                    const StandardFormRepeats *repeats);

#endif
