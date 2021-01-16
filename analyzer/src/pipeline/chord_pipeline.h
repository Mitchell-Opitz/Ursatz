#ifndef URSATZ_ANALYZER_CHORD_PIPELINE_H
#define URSATZ_ANALYZER_CHORD_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include "pipeline/key_pipeline.h"
#include "pipeline/segmentation.h"

/*
 * What the library's weighted identification established for one
 * segment: the best chord and the runner-up, each as an
 * AnalysisChordReading (label NULL when absent). found is true when a
 * best chord was identified.
 */
typedef struct {
    bool found;
    AnalysisChordReading best;
    AnalysisChordReading runner_up;
} ChordEstimate;

/*
 * Identifies the chord of every segment with Ursatz's weighted chord
 * identification (common_practice_identify_weighted_sequence), in the
 * estimated key. On success *out is an array of segmentation->segment_count
 * estimates, owned by the caller (release with chord_estimates_destroy).
 * Returns false only on allocation failure.
 *
 * The library's weighted vocabulary is the major-key set, so a minor key,
 * or no key, leaves every segment unmatched rather than reading it in a
 * key it cannot name chords for. A segment whose best score is below the
 * library's minimum coverage is likewise unmatched.
 *
 * Root, quality and the runner-up are read from the library's ranked
 * candidates and the chord tones it reports for them. The only work here
 * is naming them (a pitch class name, a triad's quality name) and finding
 * the position of the segment's bass among the chord's tones.
 */
bool identify_chords(const Segmentation *segmentation, const KeyEstimate *key, ChordEstimate **out);

void chord_estimates_destroy(ChordEstimate *estimates, size_t count);

#endif
