#ifndef URSATZ_ANALYZER_ANALYSIS_RESULT_H
#define URSATZ_ANALYZER_ANALYSIS_RESULT_H

#include <common/status.h>
#include <stdbool.h>
#include <stddef.h>

#include <chain/chain_result.h>
#include <io/imported_score.h>
#include <reading/reading.h>
#include <realization/work.h>

#include <theory/knowledge/knowledge_snapshot.h>

/*
 * The analysis as library results (R-220, R-530): one run over imported notes gives the library chain's results and the
 * report view the document is exported from. The document JSON
 * (analysis_document.h) is an export of an AnalyzerResult; it holds nothing the result does not.
 */

typedef struct AnalyzerResult AnalyzerResult;

/*
 * The stages of a run. `stages` holds ANALYSIS_CHAIN_STAGE_* bits (chain/analysis_chain.h): bits 0 to 11 are the library chain,
 * bits 12 to 17 the stages of the collected result only. The chain stages pull in the stages they need.
 *
 * The report view, and so the document, is collected only when every chain stage (ANALYSIS_CHAIN_STAGE_PERCEPTION) is selected.
 * With fewer, the result holds the chain's results of those stages, and analyzer_result_report is an empty result.
 * The result-only stages read the chain's results: a bit set without the chain stage it reads gives no entries.
 */
typedef struct {
    unsigned stages;
    /* The knowledge snapshot the analysis reads its pitch materials from (borrowed). Required: the defaults carry none, so
       analyzer_analyze with no knowledge is URSATZ_ERR_REQUEST. It must outlive the result: the keys of the result point into it. */
    const KnowledgeSnapshot *knowledge;
} AnalyzerOptions;

/* All stages, the chain's default settings (chords on harmonic spans), no knowledge snapshot (the caller sets one). */
AnalyzerOptions analyzer_options_default(void);

/*
 * Analyzes notes. options may be NULL for the defaults, which have no knowledge snapshot, so every caller passes options with one
 * (URSATZ_ERR_REQUEST otherwise). On URSATZ_OK *out is the result, which the caller releases with
 * analyzer_result_destroy and which borrows notes: notes must outlive it. On any other status *out is left NULL and
 * nothing is kept: URSATZ_ERR_REQUEST for a NULL notes, options with no stages, no knowledge or out, URSATZ_ERR_ALLOC,
 * URSATZ_ERR_INTERNAL when the chain refuses the notes.
 */
UrsatzStatus analyzer_analyze(const ImportedScore *notes, const AnalyzerOptions *options, AnalyzerResult **out);

/* NULL is a no-op. */
void analyzer_result_destroy(AnalyzerResult *result);

/* The realization of the notes (R-628) and the Analyzer's reading of it (R-630): the reading's target names the work, the movement and
   the realization's digest, its evidence the performer notes of the realization, its detail the chain of the result. Never NULL for a
   result; both are owned by it. */
const Work *analyzer_result_realization(const AnalyzerResult *result);
const Reading *analyzer_result_reading(const AnalyzerResult *result);

/* The report view the document is exported from (R-252). Never NULL for a result. */
const ChainResult *analyzer_result_report(const AnalyzerResult *result);

#endif
