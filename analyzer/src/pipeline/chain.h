#ifndef URSATZ_ANALYZER_CHAIN_H
#define URSATZ_ANALYZER_CHAIN_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "pipeline/chord_pipeline.h"
#include "pipeline/segmentation.h"
#include "time/musical_time.h"

/*
 * The analysis as one chain over a piece, ported from Ursatz's test glue (tests/integration/framework_v1_chain.h,
 * R-053) as a rewrite with error returns. chain_init cuts the notes into segments once; chain_pass runs every
 * stage from the key regions on, in order, and collects the results into report. A pass can be run again after
 * chain_pass_destroy (with the evidence of the pass before it, or with phrase boundaries), and every later stage
 * is added at the end of chain_pass with its results in report.
 *
 * The chain borrows notes, which must outlive it. Every function returns false only on allocation or library
 * failure; on false the chain still holds what was built and chain_destroy releases it.
 */
typedef struct AnalysisChain {
    const ImportedNotes *notes;
    Segmentation segmentation; /* the chord segments */

    /* one pass */
    ChordEstimate *chords;   /* segmentation.segment_count of them */
    AnalysisReport report;   /* the pass's results: entries, non-chord tones, key regions and changes */
} AnalysisChain;

bool chain_init(AnalysisChain *chain, const ImportedNotes *notes);

/* Runs the stages from the key regions on. boundaries are phrase boundary times for the key regions (any
   order, borrowed for the call), or NULL with boundary_count 0. */
bool chain_pass(AnalysisChain *chain, const MusicalTime *const *boundaries, size_t boundary_count);

/* Releases one pass's results, leaving the segments for the next pass. */
void chain_pass_destroy(AnalysisChain *chain);

void chain_destroy(AnalysisChain *chain);

#endif
