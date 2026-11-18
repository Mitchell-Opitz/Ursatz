#ifndef URSATZ_ANALYZER_ANALYSIS_RESULT_INTERNAL_H
#define URSATZ_ANALYZER_ANALYSIS_RESULT_INTERNAL_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_result.h>

#include <chain/analysis_chain.h>

/* Library-private views of an AnalyzerResult for the exports and the CLI. */

/* The notes the result borrows. */
const ImportedScore *analyzer_result_notes(const AnalyzerResult *result);

/* The chain the result ran, also for a piece with no notes (analyzer_result_chain gives NULL for that). */
const AnalysisChain *analyzer_result_run_chain(const AnalyzerResult *result);

/* The snapshot the analysis read its materials from (the options knowledge). */
const KnowledgeSnapshot *analyzer_result_knowledge(const AnalyzerResult *result);

/* True when every chain stage ran, so the report view is complete and can be exported. */
bool analyzer_result_report_complete(const AnalyzerResult *result);

/* Moves the report out into *report (the caller releases it with chain_result_destroy); the result keeps an empty one. */
bool analyzer_result_take_report(AnalyzerResult *result, ChainResult *report);

/* Gives the result something to release after the pass (the notes a hearing built). The result frees it on destroy;
   the owner's destroy runs after the pass is released. */
void analyzer_result_set_owner(AnalyzerResult *result, void *owner, void (*owner_destroy)(void *));

#endif
