#include <ursatz_analyzer/analysis_pipeline.h>

#include <string.h>

#include <ursatz_analyzer/analysis_result.h>

#include "pipeline/analysis_result_internal.h"

bool run_analysis_pipeline(const ImportedNotes *notes, AnalysisReport *report) {
    memset(report, 0, sizeof(*report));
    report->home_duration_denominator = 1;

    AnalyzerResult *result = NULL;
    if (analyzer_analyze(notes, NULL, &result) != ANALYZER_OK) {
        return false;
    }
    analyzer_result_take_report(result, report);
    analyzer_result_destroy(result);
    return true;
}
