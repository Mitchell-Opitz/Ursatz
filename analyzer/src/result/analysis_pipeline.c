#include <ursatz_analyzer/analysis_pipeline.h>

#include <string.h>

#include <ursatz_analyzer/analysis_result.h>

#include "result/analysis_result_internal.h"

bool run_analysis_pipeline(const ImportedScore *notes, const KnowledgeSnapshot *knowledge, ChainResult *report) {
    memset(report, 0, sizeof(*report));
    report->home_duration.den = 1;

    AnalyzerResult *result = NULL;
    AnalyzerOptions options = analyzer_options_default();
    options.knowledge = knowledge;
    if (analyzer_analyze(notes, &options, &result) != ANALYZER_OK) {
        return false;
    }
    analyzer_result_take_report(result, report);
    analyzer_result_destroy(result);
    return true;
}
