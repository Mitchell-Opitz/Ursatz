#ifndef URSATZ_ANALYZER_OUTER_VOICE_PIPELINE_H
#define URSATZ_ANALYZER_OUTER_VOICE_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "pipeline/chord_pipeline.h"
#include "analysis/outer_voice.h"
#include "pipeline/segmentation.h"

/*
 * The soprano and bass lines, from the library chain's outer voices (analysis_chain_outer_voices: Ursatz's
 * outer-voice extraction over the voice segments, which are not merged over unchanged harmony, so one segment per
 * melody note): the highest and lowest sounding line of each segment, smoothed across short gaps and register leaps by
 * the library. lines holds them, one entry per voice segment, borrowed from the chain. The melody and bass lines are
 * report->outer_lines, one entry per voice segment (several per chord on a harmonic span), each with the chord whose
 * segment holds its start. Each chord segment's soprano and bass (the chord's own view) are the lines' entries at the
 * chord's start (the voice segment holding it). The analyzer adds no music theory
 * here: it names each line's pitch and asks the library for its scale degree
 * (common_practice_scale_degree_label in frameworks/common-practice-minimal/outer_scale_degrees.h) against the key of
 * the region the segment lies in, so a note inside a tonicized or modulated region is read in that region's key. A
 * segment outside every key region has pitches but no degrees.
 *
 * Fills report->entries[i].soprano and .bass, and the entry's key region, for every chord segment, and
 * report->outer_lines, using the regions the chord estimates recorded. report->entries must hold
 * segmentation->segment_count entries. A line the library leaves absent in a segment (a rest, a bridged gap, a
 * register guard) is reported as such, never filled from another voice. Lines the library did not give (NULL, or not
 * one per voice segment) leave every entry's lines absent and report->outer_lines empty. Returns false only on
 * allocation failure.
 */
bool collect_outer_voices(const ImportedNotes *notes, const Segmentation *voices, const Segmentation *segmentation,
                          const ChordEstimate *chords, const AnalysisKeyRegion *regions, const OuterVoiceLines *lines,
                          AnalysisReport *report);

#endif
