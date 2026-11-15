#ifndef URSATZ_ANALYZER_RHYTHM_PIPELINE_H
#define URSATZ_ANALYZER_RHYTHM_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "analysis/outer_voice.h"
#include "pipeline/form_pipeline.h"

/*
 * The rhythm and meter of the piece, from the neutral input of Ursatz's standard-meter adapter
 * (analysis/meter_analysis_input.h), ported from the test glue (tests/integration/rhythm_util.h, R-053) as a rewrite
 * with error returns. The analyzer adds no music theory (R-018): it builds the library's input and copies the library's
 * results; every count, share, level and verdict is the library's.
 *
 * The input holds every imported note (its onset to its release; line 0 when it is a note of the outer-voice soprano, 1
 * for the bass and 2 for any other), a chord change at the start of every chord segment whose chord differs from the
 * one before it (the harmonic rhythm's definition: another base numeral or key region) and the form stage's phrases
 * with the arrival of their cadence and the length change of their expansion. The form adapter does not report elided
 * cadences, so a phrase is never elided. A note the library cannot take (no length) is left out.
 *
 * Needs the form stage's phrases and the report's entries; a piece without phrases has no rhythm section, which is not
 * an error. Fills the report's densities here and the later results in the stages below it.
 */

/* Returns false only on allocation or library failure. lines is the chain's outer voice lines. */
bool rhythm_stage_collect(const ImportedNotes *notes, const OuterVoiceLines *lines, const FormStage *form,
                          AnalysisReport *report);

#endif
