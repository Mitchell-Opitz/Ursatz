#ifndef URSATZ_ANALYZER_ANALYSIS_PIPELINE_H
#define URSATZ_ANALYZER_ANALYSIS_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

/*
 * Cuts the imported notes into sonority segments, finds the local key
 * regions with Ursatz's key-region detection, and identifies the chord
 * of every segment in its region's key with Ursatz's weighted chord
 * identification (see pipeline/segmentation.h,
 * pipeline/key_region_pipeline.h and pipeline/chord_pipeline.h),
 * collecting the results into report, which the caller owns (release
 * with analysis_report_destroy). Returns false only on allocation
 * failure.
 */
bool run_analysis_pipeline(const ImportedNotes *notes, AnalysisReport *report);

#endif
