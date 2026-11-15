#ifndef URSATZ_ANALYZER_MELODY_PIPELINE_H
#define URSATZ_ANALYZER_MELODY_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include <ursatz_chain/analysis_chain.h>

#include "pipeline/chain_input.h"
#include "pipeline/segmentation.h"

/*
 * The melody of each phrase, from the library chain's melody (analysis_chain_melody: the soprano as a line of notes with
 * every interval, direction, climax and verdict, in the home key) and the melody adapter of Ursatz's standard-counterpoint
 * (analysis/analysis_melody.h). The analyzer adds no music theory (R-018): a phrase takes its part of the chain's line
 * and the library's span-based results for it are grouped here.
 *
 * The line of a phrase holds the chain's entries whose first chord segment starts inside the phrase's span; a note held
 * across segments is one note, a rest or an absent segment breaks the chain. Each note carries its scale degree in the
 * home key and the beat strength of its attack.
 *
 * Needs the report's phrases and key regions, and fills report->melodies, one per phrase with at least one melody note.
 * A piece without phrases, or without a melody line, has none; that is not an error.
 */
/* Returns false only on allocation failure. segmentation is the chain's chord segments. */
bool melody_stage_collect(const ImportedNotes *notes, const ChainInput *input, const AnalysisChain *library,
                          const Segmentation *segmentation, AnalysisReport *report);
#endif
