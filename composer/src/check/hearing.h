#ifndef URSATZ_COMPOSER_HEARING_H
#define URSATZ_COMPOSER_HEARING_H

#include <stddef.h>

#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/piece_plan_keys.h>
#include <ursatz_composer/render.h>
#include <ursatz_composer/return_hear.h>
#include <ursatz_composer/style.h>

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

/* The render the hearing keeps: every written lane of the whole plan. Borrowed; do not destroy. */
const RenderedPlan *hearing_rendered(const ComposerHearing *hearing);

/* The plan the hearing was made from. */
const PiecePlan *hearing_plan(const ComposerHearing *hearing);

/* Test only: how many spans, in all hearings of this process so far, were handed to analyzer_hear. A test reads it before
   and after a run to see which sections the run heard. */
size_t hearing_analyzer_calls(void);

/* NULL is a no-op. */
void hearing_destroy(ComposerHearing *hearing);

/*
 * The checks that hear the written music, over a hearing the caller already has: the public check of the same name
 * (analyzer_check_section, analyzer_check_piece, piece_check_keys, return_hear, composer_style_measure) makes one
 * hearing of its own and calls these. One compose run makes one hearing and every check reads it.
 */
AnalyzerCheckResult analyzer_check_section_in(ComposerHearing *hearing, NodeId section, AnalyzerCheck *out);
AnalyzerCheckResult analyzer_check_piece_in(ComposerHearing *hearing, AnalyzerCheck *out);
bool piece_check_keys_in(ComposerHearing *hearing, const PiecePlan *plan, PieceKeyCheck *out);
ReturnHearResult return_hear_in(ComposerHearing *hearing, const PiecePlan *plan, ReturnHeard *out);
ComposerMeasureResult composer_style_measure_in(ComposerHearing *hearing, const ComposerStyle *style,
                                                StyleComparison *out);

#endif
