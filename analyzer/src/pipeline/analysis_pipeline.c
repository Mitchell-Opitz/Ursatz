#include <ursatz_analyzer/analysis_pipeline.h>

#include <string.h>

#include "pipeline/chain.h"

bool run_analysis_pipeline(const ImportedNotes *notes, AnalysisReport *report) {
    memset(report, 0, sizeof(*report));
    report->home_duration_denominator = 1;

    AnalyzerPass pass;
    if (!analyzer_pass_run(&pass, notes)) {
        analyzer_pass_destroy(&pass);
        return false;
    }
    *report = pass.report;
    memset(&pass.report, 0, sizeof(pass.report));
    analyzer_pass_destroy(&pass);
    return true;
}
