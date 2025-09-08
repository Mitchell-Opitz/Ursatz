#ifndef URSATZ_ANALYZER_MELODY_PIPELINE_H
#define URSATZ_ANALYZER_MELODY_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "analysis/outer_voice.h"
#include "pipeline/segmentation.h"

/*
 * The melody of each phrase, from the melody adapter of Ursatz's standard-counterpoint (analysis/analysis_melody.h),
 * ported from the test glue (tests/integration/melody_util.h melody_build_from_line and test_melody_bach114.c, R-053) as
 * a rewrite with error returns. The analyzer adds no music theory (R-018): the adapter reads the outer-voice soprano as
 * a line of notes and gives every interval, direction, climax and verdict; the analyzer builds the line, one per phrase,
 * and groups the results.
 *
 * The line of a phrase holds the soprano notes whose first voice segment starts inside the phrase's span. Consecutive
 * voice segments with the same note id are one note (a note held across segments); a note held in from the phrase
 * before is not part of the phrase's line; a rest or an absent segment breaks the chain. Each note carries its scale
 * degree in the key region it starts in and the beat strength of its attack.
 *
 * Needs the report's phrases and key regions, and fills report->melodies, one per phrase with at least one melody note.
 * A piece without phrases, or whose outer voice lines the library declined, has none; that is not an error.
 */

/* Returns false only on allocation failure. voices and lines are the chain's voice segmentation and its outer voice
   lines (one entry per voice segment). */
bool melody_stage_collect(const ImportedNotes *notes, const Segmentation *voices, const OuterVoiceLines *lines,
                          AnalysisReport *report);

#endif
