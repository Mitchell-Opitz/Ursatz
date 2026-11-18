#ifndef URSATZ_CHAIN_CHAIN_RESULT_RHYTHM_H
#define URSATZ_CHAIN_CHAIN_RESULT_RHYTHM_H

#include <stdbool.h>

#include <chain/chain_result.h>
#include <io/imported_score.h>

#include "chain/chain_inputs.h"

#include "analysis/voices/outer_voice.h"
#include "chain/chain_result_form.h"
#include "chain/chain_score_input.h"

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

/* Returns false only on allocation or library failure. input is the chain's neutral input of the score (note ids to score
   indices), lines the chain's outer voice lines and segments its chord segments; the library input is the chain's one meter
   input (chain_build_meter_input). */
bool rhythm_stage_collect(const ImportedScore *notes, const ChainScoreInput *input, const OuterVoiceLines *lines,
                          const ChainSegments *segments, const FormStage *form, ChainResult *report);

#endif
