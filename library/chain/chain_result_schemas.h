#ifndef URSATZ_CHAIN_CHAIN_RESULT_SCHEMAS_H
#define URSATZ_CHAIN_CHAIN_RESULT_SCHEMAS_H

#include <stdbool.h>

#include <chain/chain_result.h>
#include <chain/analysis_chain.h>
#include <io/imported_score.h>

/*
 * The galant schemas and harmonic sequences of the piece, from the library chain (analysis_chain_schemas and
 * analysis_chain_sequences: Ursatz's galant-schemas adapter, analysis/galant_analysis_match.h, and scale-harmonization
 * adapter, analysis/scale_analysis_detect.h, over the neutral per-segment input both read). The analyzer adds no music
 * theory (R-018): it copies the library's matches.
 *
 * Segment i of the input is chord segment i: its outer voices, its local key, its numeral, the strength of its beat and the
 * length of that beat; consecutive chords with the same key, outer voice degrees and numeral (a held chord) are one
 * segment, with the uncertainty of the least certain. The matches name those merged segments; the stage reads them
 * from the chain (analysis_chain_galant_segments) and maps each to the chord entries inside its span.
 *
 * Fills report->schemas and report->sequences from report->entries. A piece with no chords or no key region has none of
 * either; that is not an error. Returns false only on allocation or library failure.
 */
bool schema_stage_collect(const ImportedScore *notes, const AnalysisChain *library, ChainResult *report);

#endif
