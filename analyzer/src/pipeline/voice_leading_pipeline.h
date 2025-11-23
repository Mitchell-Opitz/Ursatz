#ifndef URSATZ_ANALYZER_VOICE_LEADING_PIPELINE_H
#define URSATZ_ANALYZER_VOICE_LEADING_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "pipeline/harmony_pipeline.h"

/*
 * The voice leading of the piece, from the voice-pitch grid of Ursatz's standard-counterpoint and standard-harmony
 * adapters (analysis/analysis_grid.h, analysis_voices.h), ported from the test glue (tests/integration/
 * voice_leading_util.h and test_voice_leading_report.c, R-053) as a rewrite with error returns. The analyzer adds no
 * music theory (R-018): it builds the library's grid and copies and counts the library's results.
 *
 * The kernel's rule-based voice separation (analysis/voice_separation.h) runs over every note of the piece as one stream
 * (the texture stage separates per track or part; the voice leading reads the whole texture), and each voice's note
 * sounding at the start of each chord segment goes into the grid: one row per report entry, voice 0 the top. A voice
 * beyond ANALYSIS_VOICE_LEADING_MAX_VOICES is left out. The spelling of a note is the input's own when it is notated and
 * matches the sounding pitch; otherwise the library derives it from the semitone (sharp-preferring).
 *
 * A piece the separation or the library declines has no voice leading (present false), which is not an error.
 */

/* Fills report->voice_leading. harmony is the chain's harmony stage, whose prepared segments the resolution check
   reads; it may be empty. Returns false only on allocation failure. */
bool voice_leading_stage_collect(const ImportedNotes *notes, const HarmonyStage *harmony, AnalysisReport *report);

#endif
