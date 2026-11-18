#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/result_chain.h>

#include <stdlib.h>
#include <string.h>

#include <chain/analysis_chain.h>

#include "pipeline/analysis_result_internal.h"

struct AnalyzerResult {
    AnalysisChain *chain; /* owned: the library chain and its collected result */
    const ImportedScore *notes;
    AnalyzerOptions options;
    void *owner; /* what the chain borrows, released after it (a hearing's notes) */
    void (*owner_destroy)(void *);
};

AnalyzerOptions analyzer_options_default(void) {
    AnalyzerOptions options = {ANALYSIS_CHAIN_STAGE_ALL, true, NULL};
    return options;
}

void analyzer_result_destroy(AnalyzerResult *result) {
    if (!result) {
        return;
    }
    analysis_chain_destroy(result->chain);
    if (result->owner_destroy) {
        result->owner_destroy(result->owner);
    }
    free(result);
}

AnalyzerStatus analyzer_analyze(const ImportedScore *notes, const AnalyzerOptions *options, AnalyzerResult **out) {
    if (!out) {
        return ANALYZER_ERR_ARGUMENT;
    }
    *out = NULL;
    AnalyzerOptions used = options ? *options : analyzer_options_default();
    if (!notes || !used.knowledge || (used.stages & ANALYSIS_CHAIN_STAGE_PERCEPTION) == 0) {
        return ANALYZER_ERR_ARGUMENT;
    }
    AnalyzerResult *result = calloc(1, sizeof(*result));
    if (!result) {
        return ANALYZER_ERR_ALLOC;
    }
    result->notes = notes;
    result->options = used;
    AnalysisChainConfig config = analysis_chain_config_default();
    config.stages = used.stages;
    config.harmonic_spans = used.harmonic_spans;
    config.refine_keys = true;
    config.knowledge = used.knowledge;
    AnalysisChainStatus status = analysis_chain_run_score(notes, &config, &result->chain);
    if (status != ANALYSIS_CHAIN_OK) {
        free(result);
        return status == ANALYSIS_CHAIN_ERR_ALLOC ? ANALYZER_ERR_ALLOC : ANALYZER_ERR_LIBRARY;
    }
    *out = result;
    return ANALYZER_OK;
}

const AnalysisChain *analyzer_result_chain(const AnalyzerResult *result) {
    return result && result->notes->count > 0 ? result->chain : NULL;
}

const AnalysisChain *analyzer_result_run_chain(const AnalyzerResult *result) {
    return result ? result->chain : NULL;
}

const ChainResult *analyzer_result_report(const AnalyzerResult *result) {
    return result ? analysis_chain_result(result->chain) : NULL;
}

const ImportedScore *analyzer_result_notes(const AnalyzerResult *result) {
    return result ? result->notes : NULL;
}

const KnowledgeSnapshot *analyzer_result_knowledge(const AnalyzerResult *result) {
    return result ? result->options.knowledge : NULL;
}

bool analyzer_result_report_complete(const AnalyzerResult *result) {
    return result && (result->options.stages & ANALYSIS_CHAIN_STAGE_PERCEPTION) == ANALYSIS_CHAIN_STAGE_PERCEPTION;
}

bool analyzer_result_take_report(AnalyzerResult *result, ChainResult *report) {
    if (!result) {
        return false;
    }
    chain_result_take(result->chain, report);
    return true;
}

void analyzer_result_set_owner(AnalyzerResult *result, void *owner, void (*owner_destroy)(void *)) {
    result->owner = owner;
    result->owner_destroy = owner_destroy;
}
