#ifndef URSATZ_ANALYZER_MOTIVE_PIPELINE_H
#define URSATZ_ANALYZER_MOTIVE_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include <ursatz_chain/analysis_chain.h>
#include <ursatz_chain/chain_inputs.h>

#include "pipeline/chain_input.h"
/* Prefixed: standard-harmony has headers of the same names. */
#include "standard-form/analysis/analysis_motive.h"

/*
 * Ursatz's standard-form motive analysis over the library chain (analysis_chain_motives). The analyzer adds no music
 * theory (R-018): the families, their occurrences and every kind are the library's.
 *
 * The motive analysis needs a NOTE-LEVEL soprano, which the outer voice lines cannot give (they keep one note per
 * segment, so a run of sixteenths is thinned to the notes that fall on the segmentation). The chain's soprano is the
 * highest sounding note at each attack: at every onset, among the notes sounding then, the highest (the later attack
 * among equals); it is a soprano note when it is attacked then, so a left-hand note is the line where the right hand
 * rests. Each note carries its scale degree in the home key.
 *
 * The chain runs the phrases, ideas and themes on that soprano with the same cadences, and the motives on them. They
 * are not the report's phrases, which come from the voice-segment soprano, but they tile the same cadences;
 * occurrences are linked to the report's phrases and ideas by span.
 *
 * The chain keeps the note-level input to itself. A MotiveStage rebuilds it with the chain's own builder
 * (chain_build_motive_input) for the notes the occurrences index and the meter. The bass is not read: the default
 * config has include_bass false and the report has no bass motives. The results are the chain's, borrowed: the chain,
 * the chain input and the notes must outlive the stage. A piece without a meter, a home key or a soprano leaves it
 * without motives; that is not an error.
 */
typedef struct {
    ChainFormInput built;       /* owned; the note-level input again, for the indices the occurrences use */
    bool have_input;            /* built holds an input */
    size_t *note_indices;       /* owned; per input note, the ImportedNotes index of the note */
    size_t note_index_count;
    StandardFormMotives motives; /* borrowed from the chain */
} MotiveStage;

/* Takes the chain's motives and rebuilds the input they index. Returns false only on allocation failure. */
bool motive_stage_read(MotiveStage *stage, const ChainInput *input, const AnalysisChain *library);

/* Copies the stage's families and occurrences into the report, each family with its features (motive_features.h) read
   from its prototype, and each occurrence with its degrees. Needs the report's entries, non-chord tones and key
   regions. Returns false only on allocation failure. */
bool motive_stage_collect(const MotiveStage *stage, const ImportedNotes *notes, AnalysisReport *report);

void motive_stage_destroy(MotiveStage *stage);

#endif
