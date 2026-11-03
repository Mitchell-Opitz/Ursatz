#ifndef URSATZ_COMPOSER_HEARING_H
#define URSATZ_COMPOSER_HEARING_H

#include <stddef.h>

#include <ursatz_analyzer/analysis_result.h>

#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/render.h>

/*
 * Private: not installed, not part of the API.
 *
 * One hearing of a plan, shared by every check of one compose run (R-257, boundary rule 7): the plan is rendered once,
 * every written lane, and what a listener finds in a section of the written music is the Analyzer's result over the
 * events inside the section's span (analyzer_hear, B-C-9). The events are borrowed from the render, never copied; the
 * result of a span is kept until the hearing is destroyed, so the next check of the same span reads the same result.
 *
 * Times in a result are in whole notes from the start of the plan, the bars of its meter map counted from the plan start.
 * The key given to the Analyzer is the key lane's key under the span's start.
 */

typedef struct ComposerHearing ComposerHearing;

typedef enum {
    HEARING_OK = 0,
    HEARING_ERR_NULL,
    HEARING_ERR_ALLOC,
    HEARING_ERR_PLAN,     /* the plan cannot be rendered, has no meter, or no key under the span's start */
    HEARING_ERR_NOT_WRITTEN, /* no note of the plan starts inside the span */
    HEARING_ERR_ANALYSIS  /* the Analyzer refused or failed */
} HearingResult;

/* Renders plan once. *out is released with hearing_destroy; NULL on failure. The plan must outlive the hearing. */
HearingResult hearing_create(const PiecePlan *plan, ComposerHearing **out);

/*
 * The Analyzer's result over the notes that start inside span, heard with the options analyzer_hearing_options_default
 * gives. *out is borrowed: valid until hearing_destroy, never released by the caller. The same span gives the same
 * result without a second analysis.
 */
HearingResult hearing_section(ComposerHearing *hearing, PlanSpan span, const AnalyzerResult **out);

/* The render the hearing keeps: every written lane of the whole plan. Borrowed; do not destroy. */
const RenderedPlan *hearing_rendered(const ComposerHearing *hearing);

/* The midi number of event i of hearing_rendered, as the Analyzer is given it. */
int hearing_midi(const ComposerHearing *hearing, size_t event);

/* NULL is a no-op. */
void hearing_destroy(ComposerHearing *hearing);

#endif
