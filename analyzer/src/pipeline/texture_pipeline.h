#ifndef URSATZ_ANALYZER_TEXTURE_PIPELINE_H
#define URSATZ_ANALYZER_TEXTURE_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

/*
 * The texture of the piece, from the neutral input of Ursatz's standard-texture adapter
 * (texture_analysis_input.h), ported from the test glue (tests/integration/texture_util.h, R-053) as a rewrite with
 * error returns. The analyzer adds no music theory (R-018): it builds the library's input and copies the library's
 * results; every texture, category, feature and confidence is the library's.
 *
 * The kernel's rule-based voice separation (analysis/voice_separation.h) runs per stream of the import (ImportedNotes
 * .streams: a MIDI track, a MusicXML part), the recommended setup, and each note enters the input with its stream, the
 * voice the separation gave it inside that stream (0 the top) and the separation's confidence. The library measures
 * every measure, classifies it with the default configuration and merges equal measures into passages. A note the
 * library cannot take (no length) is left out.
 *
 * A piece the library or the separation declines has no texture section, which is not an error.
 */

/* Fills the report's texture passages and changes. Returns false only on allocation failure. */
bool texture_stage_collect(const ImportedNotes *notes, AnalysisReport *report);

#endif
