#ifndef URSATZ_ANALYZER_CHAIN_H
#define URSATZ_ANALYZER_CHAIN_H

#include <stdbool.h>
#include <stddef.h>

#include <chain/chain_result.h>
#include <ursatz_analyzer/analysis_result.h>
#include <io/imported_score.h>

#include <chain/analysis_chain.h>

#include "analysis/voices/outer_voice.h"
#include "pipeline/chain_input.h"
#include "theory/tonality/common_practice_materials.h"
#include "pipeline/chord_pipeline.h"
#include "pipeline/form_pipeline.h"
#include "pipeline/harmony_pipeline.h"
#include "pipeline/motive_pipeline.h"
#include "pipeline/roman_numeral_pipeline.h"
#include "pipeline/segmentation.h"
#include "model/time/musical_time.h"

/*
 * The analyzer's pass over a piece: it runs the library's analysis chain once (analysis_chain_run of ursatz_chain,
 * R-217, R-241) over the neutral notes of chain_input.h, and collects the library's results into report. The chain
 * does the analysis, including the second key pass on the first pass's chords and phrase ends; the stage files only
 * read its accessors, and the Analyzer-only stages (progressions, harmonic rhythm, meter check, rhythm, markings, form
 * plan, voicing check) read the collected report and the chain's results.
 *
 * The pass borrows notes, which must outlive it. analyzer_pass_run returns false only on allocation or library
 * failure; on false the pass still holds what was built and analyzer_pass_destroy releases it.
 */
typedef struct AnalyzerPass {
    const ImportedScore *notes;
    AnalyzerOptions options;         /* the stages of this run */
    CommonPracticeMaterials materials; /* the four common-practice materials of options.knowledge, resolved once per pass */
    ChainInput input;                /* the library chain's neutral notes and key events */
    AnalysisChain *library;          /* owned; NULL for a piece with no notes */
    ChainResult report;           /* the pass's results: entries, non-chord tones, key regions and changes */

    /* Views of the chain's results that the stage files and the Analyzer-only stages read. */
    Segmentation segmentation;       /* the chord segments (analysis_chain_segments), merged over unchanged harmony */
    Segmentation voice_segmentation; /* the voice segments (analysis_chain_voice_segments), not merged: one per melody
                                        note, for the outer voices */
    ChordEstimate *chords;   /* segmentation.segment_count of them */
    RomanNumerals romans;    /* the chain's Roman numeral inputs and results, one per chord segment */
    OuterVoiceLines lines;   /* the chain's outer lines, one entry per voice segment; borrowed */
    HarmonyStage harmony;    /* the prepared harmony segments and the chain's cadences, one per chord segment */
    FormStage form;          /* the standard-form input, phrases, ideas and subphrases */
    MotiveStage motives;     /* the note-level soprano input, its phrases, ideas and themes, and the motive families */
} AnalyzerPass;

/* Runs the pass over notes with the stages of options (NULL: all stages). */
bool analyzer_pass_run(AnalyzerPass *pass, const ImportedScore *notes, const AnalyzerOptions *options);

/* Releases the pass and the report it still holds. */
void analyzer_pass_destroy(AnalyzerPass *pass);

#endif
