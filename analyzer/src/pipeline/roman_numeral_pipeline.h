#ifndef URSATZ_ANALYZER_ROMAN_NUMERAL_PIPELINE_H
#define URSATZ_ANALYZER_ROMAN_NUMERAL_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>

#include "pipeline/chord_pipeline.h"
#include "standard-tonality/analysis/chord_input.h"
#include "common-practice-minimal/key_regions.h"
#include "pipeline/segmentation.h"
#include "standard-tonality/analysis/roman_numeral_adapter.h"

/*
 * The Roman numeral of every chord segment, from Ursatz's standard-tonality adapter: the chord inputs
 * (standard_tonality_build_chord_inputs) from the library's own chords, key regions, the segments' pitch-class
 * weights and their sounding pitches, then the numerals (standard_tonality_build_roman_numerals). Each segment
 * is read in the key region it starts in, so numerals are relative to that local key, not the home key. The
 * label, its figure (6, 6/4, 6/5, 4/3, 4/2), the chord bass the figure is read from (the lowest sounding chord
 * tone), quality marks and the applied form V/x are the library's; this only builds the neutral inputs and keeps
 * what comes back.
 */
typedef struct {
    StandardTonalityKeyRegionInput *regions;  /* owned; one per key region */
    StandardTonalitySegmentInput *segments;   /* owned; one per chord segment */
    MusicalTime **starts;                     /* owned; the segments' starts, which segments borrow */
    double (*weights)[12];                    /* owned; the segments' weights, which segments borrow */
    int *pitches;                             /* owned; the segments' sounding pitches, which segments borrow */
    size_t segment_count;
    StandardTonalityChordInput *inputs;       /* owned; the adapter's inputs, one per chord segment */
    StandardTonalityRomanResult *results;     /* owned; one per chord segment */
    size_t count;
} RomanNumerals;

/* Builds the inputs and the numerals. found holds the key regions the chords were read in. On false *out holds
   what was built (release with roman_numerals_destroy). Returns false only on allocation or library failure. */
bool build_roman_numerals(const Segmentation *segmentation, const ChordEstimate *chords,
                          const CommonPracticeKeyRegions *found, RomanNumerals *out);

/*
 * Copies each segment's numeral into its entry, or the reason the library gives none: the segment lies in no
 * key region, the library identified no chord, or the adapter returns no numeral for the chord in that key.
 * Returns false only on allocation failure.
 */
bool assign_roman_numerals(const RomanNumerals *romans, const ChordEstimate *chords, AnalysisReportEntry *entries);

void roman_numerals_destroy(RomanNumerals *romans);

#endif
