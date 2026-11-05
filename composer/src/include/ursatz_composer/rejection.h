#ifndef URSATZ_COMPOSER_REJECTION_H
#define URSATZ_COMPOSER_REJECTION_H

#include <stddef.h>

#include <ursatz_composer/analyzer_check.h>

/* The runs a compose or a grow threw away (compose.h, grow.h) and why, kept in the outcome. */

/* The part of a rejected run's analysis that contradicted the plan: a section's item (analyzer_check.h) that is not agreed. */
typedef struct {
    char section[32]; /* the section's name in the plan */
    AnalyzerCheckItem item;
} ComposeRejectionItem;

/*
 * A whole run compose_run threw away and started again from the plan as given. reason is "stuck" (a stage left a target
 * open; stage names it), "key" (the plan's keys are not closely related or a key change is not a modulation),
 * "cadence" (the cadences do not follow the plan's) or "analyzer" (the Analyzer's analysis contradicted the plan; items
 * lists what). detail is the checker's own text for key and cadence, empty otherwise.
 */
typedef struct {
    size_t round;       /* 0 for the first run */
    unsigned long seed; /* the seed the run started from: the configuration's plus the round's offset */
    char stage[16];     /* the stage reached: compose_stage_name; "texture" for a run that failed a check after the stages */
    char reason[16];
    char detail[96];
    ComposeRejectionItem *items; /* owned; only for "analyzer" */
    size_t item_count;
} ComposeRejection;

/* Frees the items of each rejection and the array. Safe on NULL. */
void compose_rejections_free(ComposeRejection *rejections, size_t count);

#endif
