#ifndef URSATZ_ANALYZER_ANALYSIS_RESULT_INTERNAL_H
#define URSATZ_ANALYZER_ANALYSIS_RESULT_INTERNAL_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_result.h>

#include <chain/analysis_chain.h>

/* Library-private views of an AnalyzerResult for the exports and the CLI. */

/* The notes the result borrows. */
const ImportedScore *analyzer_result_notes(const AnalyzerResult *result);

/* The chain the result ran, also for a piece with no notes. The Analyzer's tests read the chain through it. */
const AnalysisChain *analyzer_result_run_chain(const AnalyzerResult *result);

/* The snapshot the analysis read its materials from (the options knowledge). */
const KnowledgeSnapshot *analyzer_result_knowledge(const AnalyzerResult *result);

#endif
