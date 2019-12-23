#ifndef URSATZ_ANALYZER_ANALYSIS_PIPELINE_H
#define URSATZ_ANALYZER_ANALYSIS_PIPELINE_H

#include <stdbool.h>

#include "pipeline/analysis_report.h"
#include "pipeline/midi_pipeline.h"

/*
 * Runs KeyEstimation once over the whole imported sequence and
 * ChordIdentification once per onset-grouped sonority (see
 * pipeline/segmentation.h and pipeline/key_pipeline.h for the two
 * judgment calls involved), collecting the results into report, which
 * the caller owns (release with analysis_report_destroy). Returns
 * false only on allocation failure.
 */
bool run_analysis_pipeline(const ImportedNotes *notes, AnalysisReport *report);

#endif
