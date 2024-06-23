#ifndef URSATZ_ANALYZER_ROMAN_NUMERAL_PIPELINE_H
#define URSATZ_ANALYZER_ROMAN_NUMERAL_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>

#include "pipeline/chord_pipeline.h"
#include "pipeline/segmentation.h"

/*
 * Gives every chord segment the Roman numeral Ursatz's standard-tonality adapter
 * (standard_tonality_build_roman_numerals) reads for it in the key of the region it starts in, or the reason
 * the library gives none. Numerals are relative to that local key, not the home key. The label, its figure
 * (6, 6/4, 6/5, 4/3, 4/2), quality marks and the applied form V/x are the library's; this only hands it the
 * chord the weighted identification found (root, tones above the root, bass), the key region's tonic and mode,
 * and the segment's pitch-class weights, in time order, and copies what comes back.
 *
 * A segment is left unlabelled, with a reason, when it lies in no key region, when the library identified no
 * chord, or when the adapter returns no numeral for the chord in that key.
 *
 * entries must hold segmentation->segment_count entries whose chord confidence and region are filled in.
 * Returns false only on allocation or library failure.
 */
bool assign_roman_numerals(const Segmentation *segmentation, const ChordEstimate *chords,
                           const AnalysisKeyRegion *regions, size_t region_count, AnalysisReportEntry *entries);

#endif
