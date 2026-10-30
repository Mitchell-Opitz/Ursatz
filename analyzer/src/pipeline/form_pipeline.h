#ifndef URSATZ_ANALYZER_FORM_PIPELINE_H
#define URSATZ_ANALYZER_FORM_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "analysis/outer_voice.h"
/* Prefixed: standard-harmony has headers of the same names (analysis_input.h, analysis_phrase.h). */
#include "standard-form/analysis/analysis_ideas.h"
#include "standard-form/analysis/analysis_expansion.h"
#include "standard-form/analysis/analysis_theme.h"
#include "standard-form/analysis/analysis_input.h"
#include "standard-form/analysis/analysis_phrase.h"
#include <ursatz_chain/analysis_chain.h>
#include <ursatz_chain/chain_inputs.h>

#include "pipeline/chain_input.h"

/*
 * Ursatz's standard-form analysis over the library chain: phrases, then ideas and subphrases, then themes and
 * expansions (analysis_chain_phrases, analysis_chain_ideas, analysis_chain_themes, analysis_chain_expansions), read from
 * the outer-voice soprano, the chord segments, the meter and the cadences of the harmony stage. The analyzer adds no
 * music theory: every phrase, idea and subphrase is the library's.
 *
 * The chain keeps its form input to itself. A FormStage rebuilds it with the chain's own builder (chain_build_form_input)
 * from the same segments, lines and cadences, for what the results index into: the input's notes (the soprano as notes,
 * consecutive voice segments on one note being one note), its cadences (ids "chain-<index into the cadences>") and the
 * meter. The results are the chain's, borrowed: the chain, the chain input and the notes must outlive the stage.
 * A piece the library cannot read as phrases leaves the stage without phrases; that is not an error.
 */
typedef struct {
    ChainFormInput built;                  /* owned; the form input again, for the indices the results use */
    bool have_input;                       /* built holds an input */
    size_t *note_indices;                  /* owned; per input note, the ImportedNotes index of its first constituent note */
    size_t note_index_count;
    StandardFormPhrases phrases;           /* borrowed from the chain */
    StandardFormSegmentation segmentation; /* borrowed */
    StandardFormThemes themes;             /* borrowed */
    StandardFormExpansions expansions;     /* borrowed */
} FormStage;

/* Takes the chain's phrases, ideas, themes and expansions, and rebuilds the input they index. Returns false only on
   allocation failure. */
bool form_stage_read(FormStage *stage, const ImportedNotes *notes, const ChainInput *input,
                     const AnalysisChain *library);

/* The index into the harmony stage's cadences of phrase i's cadence, or -1 when it has none. */
long form_stage_phrase_cadence(const FormStage *stage, size_t phrase);
/* Copies the stage's phrases, ideas, subphrases, themes and expansions into the report, each phrase with its span, the chord entries inside it (entries must be the
   ones the stage was built from), measures, cadence and signal, and its ideas and subphrases. A stage without phrases leaves the report without.
   Returns false only on allocation failure. */
bool form_stage_collect(const FormStage *stage, AnalysisReport *report);
void form_stage_destroy(FormStage *stage);

#endif
