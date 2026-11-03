#ifndef URSATZ_ANALYZER_ANALYSIS_RESULT_INTERNAL_H
#define URSATZ_ANALYZER_ANALYSIS_RESULT_INTERNAL_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_result.h>

/* Library-private views of an AnalyzerResult for the exports and the CLI. */

/* The report, for a check that writes into it before the export (counterpoint_check_collect). */
AnalysisReport *analyzer_result_report_mutable(AnalyzerResult *result);

/* The notes the result borrows. */
const ImportedNotes *analyzer_result_notes(const AnalyzerResult *result);

/* True when every chain stage ran, so the report view is complete and can be exported. */
bool analyzer_result_report_complete(const AnalyzerResult *result);

/* Moves the report out into *report (the caller releases it with analysis_report_destroy); the result keeps an empty one. */
bool analyzer_result_take_report(AnalyzerResult *result, AnalysisReport *report);

/* Gives the result something to release after the pass (the notes a hearing built) and a rendered document. The result
   frees both on destroy; the owner's destroy runs after the pass is released. */
void analyzer_result_set_owner(AnalyzerResult *result, void *owner, void (*owner_destroy)(void *));
void analyzer_result_set_document(AnalyzerResult *result, char *document_json);

#endif
