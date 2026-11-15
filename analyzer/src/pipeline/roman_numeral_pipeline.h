#ifndef URSATZ_ANALYZER_ROMAN_NUMERAL_PIPELINE_H
#define URSATZ_ANALYZER_ROMAN_NUMERAL_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_chain/analysis_chain.h>

#include "pipeline/chord_pipeline.h"
#include "standard-tonality/analysis/chord_input.h"
#include "standard-tonality/analysis/roman_numeral_adapter.h"

/*
 * The Roman numeral of every chord segment, from the library chain (analysis_chain_chord_inputs and
 * analysis_chain_numerals: Ursatz's standard-tonality adapter over the chain's chords, key regions, the segments'
 * pitch-class weights and their sounding pitches). Each segment is read in the key region it starts in, so numerals are
 * relative to that local key, not the home key. The label, its figure (6, 6/4, 6/5, 4/3, 4/2), the chord bass the
 * figure is read from (the lowest sounding chord tone), quality marks and the applied form V/x are the library's; this
 * only keeps what comes back, borrowed from the chain.
 */
typedef struct {
    const StandardTonalityChordInput *inputs;  /* borrowed; one per chord segment */
    const StandardTonalityRomanResult *results; /* borrowed; one per chord segment */
    size_t count;
} RomanNumerals;

/* Points romans at the chain's inputs and results. count is 0 when the chain has none. */
bool read_roman_numerals(const AnalysisChain *library, RomanNumerals *out);

/*
 * Copies each segment's numeral into its entry, or the reason the library gives none: the segment lies in no
 * key region, the library identified no chord, or the adapter returns no numeral for the chord in that key.
 * Returns false only on allocation failure.
 */
bool assign_roman_numerals(const RomanNumerals *romans, const ChordEstimate *chords, AnalysisReportEntry *entries);
void roman_numerals_destroy(RomanNumerals *romans);

#endif
