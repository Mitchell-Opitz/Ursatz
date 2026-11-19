#ifndef URSATZ_CHAIN_CHAIN_COLLECT_H
#define URSATZ_CHAIN_CHAIN_COLLECT_H

#include <stdbool.h>
#include <stddef.h>

#include <io/imported_score.h>

#include "analysis/voices/outer_voice.h"
#include "chain/analysis_chain.h"
#include "chain/chain_result.h"
#include "chain/chain_result_chords.h"
#include "chain/chain_result_form.h"
#include "chain/chain_result_harmony.h"
#include "chain/chain_result_motive.h"
#include "chain/chain_result_numerals.h"
#include "chain/chain_result_segmentation.h"
#include "chain/chain_score_input.h"
#include "model/time/musical_time.h"
#include "theory/tonality/common_practice_materials.h"

/*
 * What analysis_chain_run_score keeps beside the chain (R-615): the neutral input it built from the score, and the views of
 * the chain's results that the collectors read to fill the ChainResult. The chain runs once (analysis_chain_run) over the
 * input's notes, including the second key pass on the first pass's chords and phrase ends; the collectors only read its
 * accessors, and the six stages that exist only in the result (progressions, harmonic rhythm, meter check, rhythm,
 * markings, form plan) read the collected result and the chain's results.
 *
 * The score is borrowed and must outlive the chain. The collect belongs to its chain and is released with it.
 */
typedef struct ChainCollect {
    const ImportedScore *notes;
    unsigned stages;                 /* the stage bits of this run (analysis_chain.h) */
    CommonPracticeMaterials materials; /* the four common-practice materials of the knowledge, resolved once per run */
    ChainScoreInput input;           /* the library chain's neutral notes and key events */
    AnalysisChain *library;          /* the chain this collects from; borrowed */
    ChainResult *report;             /* the chain's result, filled by the collect; borrowed from the chain */

    /* Views of the chain's results that the collectors and the result-only stages read. */
    Segmentation segmentation;       /* the chord segments (analysis_chain_segments), merged over unchanged harmony */
    Segmentation voice_segmentation; /* the voice segments (analysis_chain_voice_segments), not merged: one per melody
                                        note, for the outer voices */
    ChordEstimate *chords;   /* segmentation.segment_count of them */
    RomanNumerals romans;    /* the chain's Roman numeral inputs and results, one per chord segment */
    OuterVoiceLines lines;   /* the chain's outer lines, one entry per voice segment; borrowed */
    HarmonyStage harmony;    /* the prepared harmony segments and the chain's cadences, one per chord segment */
    FormStage form;          /* the standard-form input, phrases, ideas and subphrases */
    MotiveStage motives;     /* the note-level soprano input, its phrases, ideas and themes, and the motive families */
} ChainCollect;

/* The ImportedScore index of each note of a stage input (built->input.note_count entries, in the input's order), in a new array
   *indices with its length *count: one map for the stages that index the score by the chain's notes (form, motive). False on
   allocation failure or a note id the score does not hold; *indices is then NULL. */
bool chain_collect_map_notes(const ChainScoreInput *input, const ChainFormInput *built, size_t **indices, size_t *count);

/* Releases the collect's views, stage states and the result it filled; the chain calls it first when it is destroyed. */
void chain_collect_destroy_results(AnalysisChain *chain);

/* Releases the input and the collect itself; the chain calls it last when it is destroyed. */
void chain_collect_destroy_input(AnalysisChain *chain);

#endif
