#ifndef URSATZ_COMPOSER_HEARING_RESULT_H
#define URSATZ_COMPOSER_HEARING_RESULT_H

#include <ursatz_analyzer/analysis_result.h>

#include "check/hearing.h"

/*
 * Private: not installed, not part of the API.
 *
 * Kept apart from hearing.h because the Analyzer's headers share names (AnalyzerResult, ANALYZER_OK) with the
 * kernel's analysis headers, which some sources include: only a source that reads a result includes this.
 */

/*
 * The Analyzer's result over the notes that start inside span, heard with the options analyzer_hearing_options_default
 * gives. *out is borrowed: valid until hearing_destroy, never released by the caller. The same span gives the same
 * result without a second analysis.
 */
HearingResult hearing_section(ComposerHearing *hearing, PlanSpan span, const AnalyzerResult **out);

#endif
