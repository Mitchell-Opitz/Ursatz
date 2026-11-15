#ifndef URSATZ_ANALYZER_PROGRESSION_PIPELINE_H
#define URSATZ_ANALYZER_PROGRESSION_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>

/* The shortest and longest progression the analyzer looks for, in chords. */
#define PROGRESSION_MIN_LENGTH 2
#define PROGRESSION_MAX_LENGTH 6

/*
 * Recurring progressions, counted over the library's Roman numerals (counting is not theory, R-018). A progression is
 * a run of 2 to 6 consecutive labelled chords inside one key region, each named by the numeral's base_numeral;
 * consecutive chords with the same base numeral are one chord (a held chord), and a run does not cross an unlabelled
 * chord or a change of key region. Every run is counted, overlapping runs included; a pattern recurs when it occurs at
 * least twice. A pattern contained in a longer recurring pattern that has the same number of occurrences is dropped (it
 * only ever occurs inside the longer one). The result is ranked by occurrences, then length, then first occurrence.
 *
 * Fills report->progressions from report->entries, replacing nothing (the report must have none yet). Returns false
 * only on allocation failure.
 */
bool progression_stage_collect(AnalysisReport *report);

/* The label of a pattern as Ursatz's corpus statistics print one: "I > IV > V > I". Owned; NULL on allocation failure. */
char *progression_label(const AnalysisProgression *progression);

#endif
