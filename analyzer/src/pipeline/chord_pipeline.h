#ifndef URSATZ_ANALYZER_CHORD_PIPELINE_H
#define URSATZ_ANALYZER_CHORD_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include "pipeline/key_pipeline.h"
#include "pipeline/segmentation.h"

typedef struct {
    bool found;
    char *label; /* owned; NULL unless found */
    AnalysisConfidence confidence; /* from the best Chord's Interpretation */
} ChordEstimate;

/*
 * Identifies the chord of every segment with Ursatz's weighted chord
 * identification (common_practice_identify_weighted_sequence), in the
 * estimated key. On success *out is an array of segmentation->segment_count
 * estimates, owned by the caller (release each with chord_estimate_destroy,
 * then free the array). Returns false only on allocation failure.
 *
 * The library's weighted vocabulary is the major-key set, so a minor key,
 * or no key, leaves every segment unmatched rather than reading it in a
 * key it cannot name chords for. A segment whose best score is below the
 * library's minimum coverage is likewise unmatched.
 */
bool identify_chords(const Segmentation *segmentation, const KeyEstimate *key, ChordEstimate **out);

void chord_estimate_destroy(ChordEstimate *estimate);

#endif
