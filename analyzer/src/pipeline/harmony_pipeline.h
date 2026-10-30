#ifndef URSATZ_ANALYZER_HARMONY_PIPELINE_H
#define URSATZ_ANALYZER_HARMONY_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "standard-harmony/analysis/analysis_cadence.h"
#include "standard-harmony/analysis/analysis_function.h"
#include "standard-harmony/analysis/analysis_prolongation.h"
#include "standard-harmony/analysis/analysis_six_four.h"
#include "common-practice-minimal/key_regions.h"
#include "standard-harmony/analysis/analysis_input.h"
#include <ursatz_chain/analysis_chain.h>
#include <ursatz_chain/chain_inputs.h>
#include "time/meter_map.h"

/*
 * Ursatz's standard-harmony analysis over the library chain's chord segments. The cadences are the chain's
 * (analysis_chain_cadences: found, with the boundary cadences at section starts, and the key changes they confirm). The
 * functions, six-fours, prolongations and phrase model are not chain results: this stage prepares the library's
 * harmony segments from the chain's own inputs (chain_build_harmony_segments: the Roman numeral claims, the outer
 * voices' degrees, the meter) and calls the library for them. The analyzer adds no music theory: every cadence, type
 * and verdict is the library's.
 *
 * A HarmonyStage owns the inputs and the prepared segments the later stages (functions, six-fours and prolongations)
 * read; the cadences are the chain's, borrowed, so the chain must outlive the stage. A piece the library cannot
 * prepare (a segment it cannot place on the meter) leaves the stage without segments, as a piece with no functions;
 * that is not an error.
 */
typedef struct {
    size_t count;                 /* chord segments prepared; 0 when there are none or the library declined */
    ChainHarmonyInputs inputs;    /* owned; one per chord segment */
    HarmonySegment *segments;     /* owned; prepared, pointing at inputs */
    HarmonyCadences cadences;     /* borrowed from the chain, in order of arrival */
    bool *boundary;               /* owned; parallel to cadences.items: a half cadence with an alternative reading,
                                     which the chain's boundary step makes; NULL when the stage is empty */
} HarmonyStage;

/* Prepares the library's harmony segments from the chain's results, and takes its cadences. Returns false only on
   allocation failure. */
bool harmony_stage_prepare(const MeterMap *map, const AnalysisChain *library, HarmonyStage *out);

/* Copies the stage's cadences into report->cadences, each with its entry ranges, type, soprano degree (the entry's
   label), beat strength, key region and uncertainty. entries must be the ones the stage was prepared from. Returns
   false only on allocation failure. */
bool harmony_stage_collect_cadences(const HarmonyStage *stage, AnalysisReport *report);

/* Reads the library's harmonic function of every prepared segment into report->entries (function,
   function_applied, function_applied_to, function_uncertainty). Marks the cadential six-fours first
   (harmony_analysis_mark_cadential) so the functions read them as the dominant's embellishment. entries must be the
   ones the stage was prepared from. A stage the library declined leaves the entries without a function. Returns
   false only on allocation failure. */
bool harmony_stage_collect_functions(HarmonyStage *stage, AnalysisReport *report);

/* Reads the library's six-four types into report->entries (six_four) and its prolongations into
   report->prolongations. Run after harmony_stage_collect_functions. entries must be the ones the stage was prepared
   from. A stage the library declined leaves both empty. Returns false only on allocation failure. */
bool harmony_stage_collect_six_fours_and_prolongations(HarmonyStage *stage, AnalysisReport *report);

/* Labels the chords of every phrase of report->phrases for the phrase model (harmony_analysis_phrase_model_label):
   Tb, PD, D and Te as the library names them, into report->entries (phrase_model) and report->phrases
   (phrase_model_labeled). A phrase spans the chords that start in it up to its cadence's arrival (an extension after
   the cadence takes no label); a phrase with no cadence gets none. Run after the phrases are in the report. A stage
   the library declined, or no phrases, leaves the report without labels. Returns false only on allocation failure. */
bool harmony_stage_collect_phrase_model(const HarmonyStage *stage, AnalysisReport *report);

void harmony_stage_destroy(HarmonyStage *stage);

#endif
