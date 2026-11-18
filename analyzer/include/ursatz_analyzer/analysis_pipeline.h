#ifndef URSATZ_ANALYZER_ANALYSIS_PIPELINE_H
#define URSATZ_ANALYZER_ANALYSIS_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>
#include <theory/knowledge/knowledge_snapshot.h>
#include <ursatz_analyzer/midi_pipeline.h>

/*
 * Cuts the imported notes into sonority segments, finds the local key
 * regions with Ursatz's key-region detection, and identifies the chord
 * of every segment in its region's key with Ursatz's weighted chord
 * identification (see pipeline/segmentation.h,
 * pipeline/key_region_pipeline.h and pipeline/chord_pipeline.h),
 * collecting the results into report, which the caller owns (release
 * with analysis_report_destroy). The pitch materials are those of
 * `knowledge`, which must outlive the report's use of them. Returns false
 * on allocation failure or when `knowledge` is NULL.
 */
bool run_analysis_pipeline(const ImportedNotes *notes, const KnowledgeSnapshot *knowledge, AnalysisReport *report);

#endif
