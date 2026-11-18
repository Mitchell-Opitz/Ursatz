#ifndef URSATZ_COMPOSER_HEARING_RESULT_H
#define URSATZ_COMPOSER_HEARING_RESULT_H

#include <chain/analysis_chain.h>
#include <chain/chain_result.h>

#include "check/hearing.h"

/*
 * Private: not installed, not part of the API.
 *
 * Kept apart from hearing.h because the chain's headers share names with the kernel's analysis headers, which some sources
 * include: only a source that reads a result includes this.
 */

/*
 * The library chain's result over the notes that start inside span, run by analysis_chain_run_score with the chain's default
 * configuration and `knowledge`, the snapshot the chain reads its materials from (it outlives the chain). *out is borrowed: valid until hearing_destroy, never released by the caller. The same span gives the same
 * chain without a second analysis.
 */
HearingResult hearing_section(ComposerHearing *hearing, PlanSpan span, const AnalysisChain **out);

/*
 * The chain over the whole piece: from the plan's start to where its last section that is not material ends (the whole
 * plan when it has no section). It is one span of hearing_section, so the checks of the piece share one analysis.
 */
HearingResult hearing_piece(ComposerHearing *hearing, const AnalysisChain **out);

/*
 * The index among hearing_rendered's events of event index of the notes a chain was run on (the ids a voice fault names are
 * indices into those notes). (size_t)-1 for a chain the hearing did not make or an index it does not have.
 */
size_t hearing_source_event(const ComposerHearing *hearing, const AnalysisChain *chain, size_t index);

#endif
