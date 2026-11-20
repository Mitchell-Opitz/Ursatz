#include <ursatz_analyzer_internal/analysis_result.h>
#include <ursatz_analyzer_internal/version.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <chain/analysis_chain.h>
#include <io/imported_realization.h>
#include <realization/realization_json.h>
#include <reading/reading_build.h>

#include "result/analysis_result_internal.h"

struct AnalyzerResult {
    AnalysisChain *chain; /* owned: the library chain and its collected result */
    Work *realization;    /* owned: the realization of the notes (R-628) */
    Reading *reading;     /* owned: the Analyzer's reading of it, whose detail is the chain */
    const ImportedScore *notes;
    AnalyzerOptions options;
};

AnalyzerOptions analyzer_options_default(void) {
    AnalyzerOptions options = {ANALYSIS_CHAIN_STAGE_ALL, NULL};
    return options;
}

void analyzer_result_destroy(AnalyzerResult *result) {
    if (!result) {
        return;
    }
    reading_destroy(result->reading);
    analysis_chain_destroy(result->chain);
    work_destroy(result->realization);
    free(result);
}

/* The realization of the notes and the reading of the chain run over it (R-630). */
static UrsatzStatus build_layers(AnalyzerResult *result) {
    char(*note_ids)[REALIZATION_ID_MAX] = NULL;
    ImportedScoreResult imported = imported_score_realization(result->notes, NULL, &result->realization, &note_ids);
    if (imported != IMPORTED_SCORE_OK) {
        return imported == IMPORTED_SCORE_ERR_ALLOC ? URSATZ_ERR_ALLOC : URSATZ_ERR_INTERNAL;
    }
    ReadingSource source = {READING_SOURCE_ANALYZER, "ursatz-analyzer", "", result->options.stages, ""};
    snprintf(source.producer_key, sizeof(source.producer_key), "%s", analyzer_producer_key());
    snprintf(source.knowledge_hash, sizeof(source.knowledge_hash), "%s", knowledge_snapshot_hash(result->options.knowledge));
    ReadingTarget target = {"w1", "m1", ""};
    if (!realization_digest(result->realization, target.digest)) {
        free(note_ids);
        return URSATZ_ERR_INTERNAL;
    }
    RealizationResult built = reading_build(result->chain, &source, &target, (const char(*)[REALIZATION_ID_MAX])note_ids,
                                            result->notes->count, &result->reading);
    free(note_ids);
    if (built != REALIZATION_OK) {
        return built == REALIZATION_ERR_ALLOC ? URSATZ_ERR_ALLOC : URSATZ_ERR_INTERNAL;
    }
    return URSATZ_OK;
}

UrsatzStatus analyzer_analyze(const ImportedScore *notes, const AnalyzerOptions *options, AnalyzerResult **out) {
    if (!out) {
        return URSATZ_ERR_REQUEST;
    }
    *out = NULL;
    AnalyzerOptions used = options ? *options : analyzer_options_default();
    if (!notes || !used.knowledge || (used.stages & ANALYSIS_CHAIN_STAGE_PERCEPTION) == 0) {
        return URSATZ_ERR_REQUEST;
    }
    AnalyzerResult *result = calloc(1, sizeof(*result));
    if (!result) {
        return URSATZ_ERR_ALLOC;
    }
    result->notes = notes;
    result->options = used;
    AnalysisChainConfig config = analysis_chain_config_default();
    config.stages = used.stages;
    config.refine_keys = true;
    config.knowledge = used.knowledge;
    AnalysisChainStatus status = analysis_chain_run_score(notes, &config, &result->chain);
    if (status != ANALYSIS_CHAIN_OK) {
        free(result);
        return status == ANALYSIS_CHAIN_ERR_ALLOC ? URSATZ_ERR_ALLOC : URSATZ_ERR_INTERNAL;
    }
    UrsatzStatus built = build_layers(result);
    if (built != URSATZ_OK) {
        analyzer_result_destroy(result);
        return built;
    }
    *out = result;
    return URSATZ_OK;
}

const AnalysisChain *analyzer_result_run_chain(const AnalyzerResult *result) {
    return result ? result->chain : NULL;
}

const ChainResult *analyzer_result_report(const AnalyzerResult *result) {
    return result ? analysis_chain_result(result->chain) : NULL;
}

const Work *analyzer_result_realization(const AnalyzerResult *result) {
    return result ? result->realization : NULL;
}

const Reading *analyzer_result_reading(const AnalyzerResult *result) {
    return result ? result->reading : NULL;
}

const ImportedScore *analyzer_result_notes(const AnalyzerResult *result) {
    return result ? result->notes : NULL;
}

const KnowledgeSnapshot *analyzer_result_knowledge(const AnalyzerResult *result) {
    return result ? result->options.knowledge : NULL;
}
