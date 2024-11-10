#include <ursatz_analyzer/analysis_pipeline.h>

#include <string.h>

#include "pipeline/chain.h"

bool run_analysis_pipeline(const ImportedNotes *notes, AnalysisReport *report) {
    memset(report, 0, sizeof(*report));
    report->home_duration_denominator = 1;

    /* Pass 1 reads the key regions from the pitch classes alone; its chords are then the functional evidence for
       the key regions of pass 2, and every stage after the key regions re-runs on that answer (R-043). */
    AnalysisChain chain;
    if (!chain_init(&chain, notes) || !chain_pass(&chain, NULL, 0) || !chain_keep_chord_evidence(&chain)) {
        chain_destroy(&chain);
        return false;
    }
    chain_pass_destroy(&chain);
    if (!chain_pass(&chain, NULL, 0)) {
        chain_destroy(&chain);
        return false;
    }
    *report = chain.report;
    memset(&chain.report, 0, sizeof(chain.report));
    chain_destroy(&chain);
    return true;
}
