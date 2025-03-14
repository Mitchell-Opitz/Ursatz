#ifndef URSATZ_ANALYZER_FORM_PIPELINE_H
#define URSATZ_ANALYZER_FORM_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "analysis/outer_voice.h"
/* Prefixed: standard-harmony has headers of the same names (analysis_input.h, analysis_phrase.h). */
#include "standard-form/analysis/analysis_ideas.h"
#include "standard-form/analysis/analysis_input.h"
#include "standard-form/analysis/analysis_phrase.h"
#include "pipeline/harmony_pipeline.h"
#include "pipeline/segmentation.h"
#include "time/time_span.h"

/*
 * Ursatz's standard-form analysis over the chain: phrases, then ideas and subphrases. Ported from the test glue
 * (tests/integration/form_util.h form_build_*, test_form_reference_pieces.c add_chain_cadences and form_analyze,
 * R-053) as a rewrite with error returns. The analyzer adds no music theory: the input is the outer-voice soprano, the
 * chord segments, the meter and the cadences of the harmony stage, and every phrase, idea and subphrase is the
 * library's.
 *
 * The soprano. Consecutive voice segments in which the soprano is the same note are ONE note (a held note) that spans
 * from the start of its first segment to the end of its last; a voice segment with no soprano is a gap. The outer
 * voice lines are indexed by voice segment, not by chord segment (do not port Ursatz's glue, which indexes them by
 * chord segment).
 *
 * The cadences are the harmony stage's, in its order, each with the span from its first arrival chord's start to its
 * last arrival chord's end, ids "chain-<index into the stage's cadences>".
 *
 * A FormStage owns the input and the results; the phrases and ideas borrow the notes, the meter and the segmentation,
 * which must outlive the stage. A piece the library cannot read as phrases (no soprano, a time the meter cannot place)
 * leaves the stage without phrases; that is not an error.
 */
typedef struct {
    StandardFormInput input;
    TimeSpan **spans; /* owned: every span copied into the input */
    size_t span_count;
    size_t span_capacity;
    bool built;       /* input holds a finished input */
    StandardFormPhrases phrases;
    StandardFormSegmentation segmentation;
} FormStage;

/* Builds the input and runs standard_form_segment_phrases and standard_form_segment_ideas with the default config.
   lines is the outer voice lines over voices; harmony the stage's cadences. Returns false only on allocation
   failure. */
bool form_stage_run(FormStage *stage, const ImportedNotes *notes, const Segmentation *voices,
                    const Segmentation *segmentation, const OuterVoiceLines *lines, const HarmonyStage *harmony);

/* The index into the harmony stage's cadences of phrase i's cadence, or -1 when it has none. */
long form_stage_phrase_cadence(const FormStage *stage, size_t phrase);

void form_stage_destroy(FormStage *stage);

#endif
