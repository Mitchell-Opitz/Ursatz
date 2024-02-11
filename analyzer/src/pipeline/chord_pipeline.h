#ifndef URSATZ_ANALYZER_CHORD_PIPELINE_H
#define URSATZ_ANALYZER_CHORD_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>

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
    int root_pitch_class;        /* the best chord's root, 0-11; valid only when found */
    int intervals[4];            /* the best chord's tones above its root, ascending; valid only when found */
    size_t interval_count;
    AnalysisNonChordTone *tones; /* owned; the segment's non-chord tones, chord_index unset */
    size_t tone_count;
    bool in_region; /* the segment lies in a key region */
    size_t region;  /* index of that region; valid only when in_region */
} ChordEstimate;

/*
 * Identifies the chord of every segment with Ursatz's weighted chord
 * identification (common_practice_identify_weighted_sequence), each
 * segment read in the key of the region it starts in: the segments of
 * one region are identified together, in that region's key. On success
 * *out is an array of segmentation->segment_count estimates, owned by
 * the caller (release with chord_estimates_destroy). Returns false
 * only on allocation failure.
 *
 * The library's weighted vocabulary is the major-key set, so a segment
 * in a minor region, or in no region, is left unmatched rather than read
 * in a key it cannot name chords for. A segment whose best score is
 * below the library's minimum coverage is likewise unmatched.
 *
 * The library's leftover tones for each matched segment become the
 * estimate's non-chord tones: each with the library's name for its type,
 * or no type when the library left it unclassified.
 *
 * Root, quality and the runner-up are read from the library's ranked
 * candidates and the chord tones it reports for them. The only work here
 * is naming them (a pitch class name, a triad's quality name) and finding
 * the position of the segment's bass among the chord's tones.
 */
bool identify_chords(const Segmentation *segmentation, const AnalysisKeyRegion *regions, size_t region_count,
                     ChordEstimate **out);

/*
 * Fills the pivot reading of every key change that has a pivot chord: the pivot segment's best chord
 * read in the OLD key (the change's from-region), as the library found the pivot. The library reads
 * a minor old key in its relative major, and so does this. Leaves the reading empty when the library
 * names no chord there. Returns false only on allocation failure.
 */
bool identify_pivot_chords(const Segmentation *segmentation, const AnalysisKeyRegion *regions,
                           AnalysisKeyChange *changes, size_t change_count);

void chord_estimates_destroy(ChordEstimate *estimates, size_t count);

#endif
