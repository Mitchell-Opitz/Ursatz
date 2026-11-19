#ifndef URSATZ_COMPOSER_GROW_H
#define URSATZ_COMPOSER_GROW_H

#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/grow_types.h>
#include <ursatz_composer/rejection.h>

/* The stages of grow-theme and the outcome of running them; the operator's own types are in grow_types.h. */

typedef struct {
    FillRecord *records[GROW_STAGE_COUNT]; /* owned; NULL for a stage that did not run */
    bool stuck;
    GrowStage stuck_stage;                 /* the stage that left a target open, when stuck */
    bool cancelled;                        /* the job was cancelled (GrowConfig.job); stuck is false */
    FormHearResult heard_result;           /* FORM_HEAR_ERR_NOT_WRITTEN when the run did not complete */
    FormHeard heard;                       /* owned; the themes form_hear_in found over the theme section */
    int heard_form;                        /* PhraseLevelForm of the first theme heard, -1 for none */
    int requested_form;                    /* PhraseLevelForm the theme section asks for */
    int requested_beginning;               /* PhraseBeginning and PhraseEnding of the pairing it asks for, -1 for none */
    int requested_ending;
    bool heard_as_requested;               /* the requested form; for a hybrid, with the requested beginning and ending */
    GrowScore score;                       /* the share of the structure's soft rules and the heard form that hold */
    char construction_text[192];           /* why the theme is as long as it is (grow_construction_describe), "" without a construction */
    bool style_measured;                   /* with a style, after a complete run: the Analyzer's distance from it (compose.h) */
    StyleComparison style_distance;        /* owned */
    ComposeRejection *rejections;          /* owned; the runs thrown away before this one (compose.h), NULL when none */
    size_t rejection_count;
} GrowOutcome;

void grow_outcome_destroy(GrowOutcome *outcome);

/*
 * Runs the stages on a plan made by grow_build_plan_checked. GROW_OK with the plan complete, or GROW_ERR_STUCK with the
 * stage that failed in outcome->stuck_stage (the plan is then partly filled). After a complete run the theme is
 * heard (form_hear_in): heard as the requested form adds to the score; a mismatch is reported, not rejected.
 */
GrowResult grow_run(Plan *plan, const GrowConfig *config, GrowOutcome *outcome);

#endif
