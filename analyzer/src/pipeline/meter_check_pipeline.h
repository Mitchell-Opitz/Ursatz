#ifndef URSATZ_ANALYZER_METER_CHECK_PIPELINE_H
#define URSATZ_ANALYZER_METER_CHECK_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

/*
 * The meter checked against the harmonic rhythm and the cadences (R-110). A file's meter can be wrong for its music (a
 * 3/4 piece written in 4/4), and the harmony shows it: chords change and cadences arrive on the downbeats of the
 * meter the music is in. The check scores the effective meter and a set of other readings by how well their barlines
 * carry that evidence, and reports the best other reading. It never applies one.
 *
 * Candidates: every meter of 2/4, 3/4, 4/4, 3/8, 6/8, 9/8 and 12/8, at the scales 1/2, 1 and 2 (the notes' starts and
 * durations multiplied, as a notation reading does), with a pickup of 0 to one beat less than the measure, in whole
 * beats (a beat is the denominator's note, or a dotted one in 6/8, 9/8 and 12/8). Barlines are placed in the file's
 * time: a measure of a meter at a scale lasts the meter's length over the scale. Candidates with the same barlines and
 * beats are one candidate; of them the shown reading is the one whose most common inter-onset interval, scaled, is
 * nearest an eighth note, then the one with denominator 4 over 8.
 *
 * Score: the share of the harmony changes (report entries as harmonic_rhythm_is_change reads them, the first segment
 * included) on a downbeat, weight 2, plus the share on any beat, weight 1, plus the share of the cadence arrivals on a
 * downbeat, weight 3, over the weights of the evidence the piece has. A piece with no cadences scores on its changes
 * alone.
 *
 * The effective reading is the analysis's own meter map: the first full measure's signature at scale 1, with the
 * length of a pickup region as its pickup; its score reads the map, so meter changes are honoured. A candidate
 * with the effective reading's barlines and beats is not an alternative to it.
 */

/* An alternative is offered when the best other reading beats the effective one by at least this. */
#define METER_CHECK_MIN_MARGIN 0.15

/* Fills *out from the chord segments and the cadences (indices into entries). present is false when there are no
   segments. Returns false only on allocation failure. */
bool meter_check_run(const ImportedNotes *notes, const AnalysisReportEntry *entries, size_t entry_count,
                     const AnalysisCadence *cadences, size_t cadence_count, AnalysisMeterCheck *out);

/* Fills report->meter_check from report->entries and report->cadences; run after the harmonic rhythm stage. */
bool meter_check_stage_collect(const ImportedNotes *notes, AnalysisReport *report);

#endif
