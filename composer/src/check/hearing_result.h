#ifndef URSATZ_COMPOSER_HEARING_RESULT_H
#define URSATZ_COMPOSER_HEARING_RESULT_H

#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/composer_api.h>

#include "check/hearing.h"

/*
 * Private: not installed, not part of the API.
 *
 * Kept apart from hearing.h because the Analyzer's headers share names (AnalyzerResult, ANALYZER_OK) with the
 * kernel's analysis headers, which some sources include: only a source that reads a result includes this.
 */

/*
 * The options every hearing of the Composer's music uses: analyzer_hearing_options_default with harmonic_spans false. The
 * Composer hears one chord per segment, the reading its compose rates and baselines were recorded on; on harmonic spans the
 * Analyzer misreads the two-voice texture the Composer renders at cadences until P40 and P41 (R-378, R-379), and P41 turns
 * spans on here.
 */
AnalyzerHearingOptions hearing_analyzer_options(void);

/*
 * The Analyzer's result over the notes that start inside span, heard with the options hearing_analyzer_options gives. *out is borrowed: valid until hearing_destroy, never released by the caller. The same span gives the same
 * result without a second analysis.
 */
HearingResult hearing_section(ComposerHearing *hearing, PlanSpan span, const AnalyzerResult **out);

/*
 * The result over the whole piece: from the plan's start to where its last section that is not material ends (the whole
 * plan when it has no section). It is one span of hearing_section, so the checks of the piece share one analysis.
 */
HearingResult hearing_piece(ComposerHearing *hearing, const AnalyzerResult **out);

/*
 * The index among hearing_rendered's events of event index of the notes a result was heard from (the ids a violation
 * names are indices into those notes). (size_t)-1 for a result the hearing did not make or an index it does not have.
 */
size_t hearing_source_event(const ComposerHearing *hearing, const AnalyzerResult *result, size_t index);

#endif
