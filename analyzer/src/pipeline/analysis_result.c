#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/result_chain.h>

#include <stdlib.h>
#include <string.h>

#include <chain/analysis_chain.h>

#include "pipeline/analysis_result_internal.h"
#include "pipeline/chain.h"

struct AnalyzerResult {
    AnalyzerPass pass;
    void *owner; /* what the pass borrows, released after it (a hearing's notes) */
    void (*owner_destroy)(void *);
};

_Static_assert(ANALYZER_STAGE_ALL == (ANALYSIS_CHAIN_STAGE_ALL | ((1u << 18) - (1u << 12))),
               "ANALYZER_STAGE_ALL covers the chain stages and the Analyzer bits");

AnalyzerOptions analyzer_options_default(void) {
    AnalyzerOptions options = {ANALYZER_STAGE_ALL, true, NULL};
    return options;
}

void analyzer_result_destroy(AnalyzerResult *result) {
    if (!result) {
        return;
    }
    analyzer_pass_destroy(&result->pass);
    if (result->owner_destroy) {
        result->owner_destroy(result->owner);
    }
    free(result);
}

AnalyzerStatus analyzer_analyze(const ImportedNotes *notes, const AnalyzerOptions *options, AnalyzerResult **out) {
    if (!out) {
        return ANALYZER_ERR_ARGUMENT;
    }
    *out = NULL;
    AnalyzerOptions used = options ? *options : analyzer_options_default();
    if (!notes || !used.knowledge || (used.stages & ANALYSIS_CHAIN_STAGE_ALL) == 0) {
        return ANALYZER_ERR_ARGUMENT;
    }
    AnalyzerResult *result = calloc(1, sizeof(*result));
    if (!result) {
        return ANALYZER_ERR_ALLOC;
    }
    if (!analyzer_pass_run(&result->pass, notes, &used)) {
        AnalyzerStatus status = result->pass.library || notes->count == 0 ? ANALYZER_ERR_ALLOC : ANALYZER_ERR_LIBRARY;
        analyzer_result_destroy(result);
        return status;
    }
    *out = result;
    return ANALYZER_OK;
}

const AnalysisChain *analyzer_result_chain(const AnalyzerResult *result) {
    return result ? result->pass.library : NULL;
}

const AnalysisReport *analyzer_result_report(const AnalyzerResult *result) {
    return result ? &result->pass.report : NULL;
}

const ImportedNotes *analyzer_result_notes(const AnalyzerResult *result) {
    return result ? result->pass.notes : NULL;
}

const KnowledgeSnapshot *analyzer_result_knowledge(const AnalyzerResult *result) {
    return result ? result->pass.options.knowledge : NULL;
}

bool analyzer_result_report_complete(const AnalyzerResult *result) {
    return result && (result->pass.options.stages & ANALYSIS_CHAIN_STAGE_ALL) == ANALYSIS_CHAIN_STAGE_ALL;
}

bool analyzer_result_take_report(AnalyzerResult *result, AnalysisReport *report) {
    if (!result) {
        return false;
    }
    *report = result->pass.report;
    memset(&result->pass.report, 0, sizeof(result->pass.report));
    result->pass.report.home_duration_denominator = 1;
    return true;
}

void analyzer_result_set_owner(AnalyzerResult *result, void *owner, void (*owner_destroy)(void *)) {
    result->owner = owner;
    result->owner_destroy = owner_destroy;
}
