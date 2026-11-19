#ifndef URSATZ_COMPOSER_HEARING_RESULT_H
#define URSATZ_COMPOSER_HEARING_RESULT_H

#include <chain/analysis_chain.h>
#include <chain/chain_result.h>
#include <reading/reading.h>

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
 * The reading of the notes that start inside span: the library reading_build of the span's chain (hearing_section: the same cache), source
 * the Composer (its composition key, all the chain's stages, the snapshot's hash), target work `w1`, movement `m1` and no digest (the layers
 * output sets it), and the performer note ids of rendered_plan_work's work as evidence (`rendered_plan_note_id`). Built on the first request
 * and kept until hearing_destroy, so two requests of one span give one reading, borrowed. The errors are hearing_section's.
 */
HearingResult hearing_section_reading(ComposerHearing *hearing, PlanSpan span, const Reading **out);

/*
 * hearing_section_reading over the whole piece: from the plan's start to where its last section that is not material ends (the whole
 * plan when it has no section). It is one span of hearing_section_reading, so the checks of the piece share one analysis.
 */
HearingResult hearing_piece_reading(ComposerHearing *hearing, const Reading **out);

/*
 * The rendered event (an index among hearing_rendered's events) of a performer note id of the work: the inverse of rendered_plan_note_id.
 * (size_t)-1 for an id the work does not have.
 */
size_t hearing_event_of_note(const ComposerHearing *hearing, const char *note_id);

#endif
