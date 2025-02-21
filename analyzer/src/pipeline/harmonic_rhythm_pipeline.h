#ifndef URSATZ_ANALYZER_HARMONIC_RHYTHM_PIPELINE_H
#define URSATZ_ANALYZER_HARMONIC_RHYTHM_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

/*
 * Harmonic rhythm, counted over the library's Roman numerals and key regions (counting is not theory, R-018). A chord
 * segment is a change when its base numeral or its key region differs from the segment before it (the first segment
 * is a change; a segment with no numeral differs from one that has one). Changes are tallied per measure, measures
 * as the library numbers them on the piece's meter map.
 *
 * Fills report->harmonic_rhythm from report->entries: one record per measure from the first segment's to the last
 * segment's, a measure no segment starts in counting 0. A piece with no segments, or one the meter cannot place, is
 * left empty. Also fills report->cadence_approaches, one per cadence of report->cadences: the changes in the two
 * measures before the arrival's measure against those in the two measures before them. Returns false only on
 * allocation failure.
 */
bool harmonic_rhythm_stage_collect(const ImportedNotes *notes, AnalysisReport *report);

#endif
