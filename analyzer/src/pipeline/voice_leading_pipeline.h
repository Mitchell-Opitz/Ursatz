#ifndef URSATZ_ANALYZER_VOICE_LEADING_PIPELINE_H
#define URSATZ_ANALYZER_VOICE_LEADING_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include <ursatz_chain/analysis_chain.h>

#include "pipeline/chain_input.h"
#include "pipeline/harmony_pipeline.h"

/*
 * The voice leading of the piece, from the library chain (analysis_chain_voice_leading: the motions and perfect-interval
 * violations of Ursatz's standard-counterpoint adapter, the resolutions of standard-harmony's, over the voice-pitch grid
 * of analysis/analysis_grid.h and analysis_voices.h). The analyzer adds no music theory (R-018): it copies and counts the
 * library's results.
 *
 * The chain's grid is the kernel's rule-based voice separation (analysis/voice_separation.h) over every note of the piece
 * as one stream (the texture stage separates per track or part; the voice leading reads the whole texture), with each
 * voice's note sounding at the start of each chord segment: one row per report entry, voice 0 the top. The stage reads
 * it from the chain (analysis_chain_voice_grid) for the note in each cell and the spellings the doubling and spacing
 * read. A voice beyond ANALYSIS_VOICE_LEADING_MAX_VOICES is left out of
 * the doubling and spacing.
 *
 * A piece the chain or the library has no voice leading for has none (present false), which is not an error.
 */

/* Fills report->voice_leading. harmony is the harmony stage, whose segments the doubling and spacing read; it may be
   empty. Returns false only on allocation failure. */
bool voice_leading_stage_collect(const ImportedNotes *notes, const ChainInput *input, const AnalysisChain *library,
                                 const HarmonyStage *harmony, AnalysisReport *report);

#endif
