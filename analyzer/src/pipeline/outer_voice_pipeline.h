#ifndef URSATZ_ANALYZER_OUTER_VOICE_PIPELINE_H
#define URSATZ_ANALYZER_OUTER_VOICE_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "pipeline/chord_pipeline.h"
#include "pipeline/segmentation.h"

/*
 * The soprano and bass of every chord segment, from Ursatz's outer-voice extraction
 * (outer_voice_extract in analysis/outer_voice.h): the highest and lowest sounding line of each segment,
 * smoothed across short gaps and register leaps by the library. The analyzer adds no music theory here: it
 * names each line's pitch and asks the library for its scale degree
 * (common_practice_scale_degree_label in frameworks/common-practice-minimal/outer_scale_degrees.h) against
 * the key of the region the segment lies in, so a note inside a tonicized or modulated region is read in that
 * region's key. A segment outside every key region has pitches but no degrees.
 *
 * Fills entries[i].soprano and entries[i].bass, and the entry's key region, for every segment, using the
 * regions the chord estimates recorded. entries must hold segmentation->segment_count entries. A line the
 * library leaves absent in a segment (a rest, a bridged gap, a register guard) is reported as such, never
 * filled from another voice. The library declining a piece (a negative pitch, mixed tunings) leaves every
 * entry's lines absent. Returns false only on allocation failure.
 */
bool extract_outer_voices(const ImportedNotes *notes, const Segmentation *segmentation, const ChordEstimate *chords,
                          const AnalysisKeyRegion *regions, AnalysisReportEntry *entries);

#endif
