#include <ursatz_analyzer/analysis_pipeline.h>

#include <string.h>

#include "pipeline/chain.h"

bool run_analysis_pipeline(const ImportedNotes *notes, AnalysisReport *report) {
    memset(report, 0, sizeof(*report));
    report->home_duration_denominator = 1;

    AnalysisChain chain;
    if (!chain_init(&chain, notes) || !chain_pass(&chain, NULL, 0)) {
        chain_destroy(&chain);
        return false;
    }
    *report = chain.report;
    memset(&chain.report, 0, sizeof(chain.report));
    chain_destroy(&chain);
    return true;
}
