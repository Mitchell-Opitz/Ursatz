#ifndef URSATZ_COMPOSER_STYLE_INTERNAL_H
#define URSATZ_COMPOSER_STYLE_INTERNAL_H

#include <stdbool.h>

#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/style.h>

/* The Analyzer's motive categories, left out of every style read and distance until P16 makes them preferences. */
#define COMPOSER_STYLE_MOTIVE_CATEGORY_COUNT 4
extern const char *const composer_style_motive_categories[COMPOSER_STYLE_MOTIVE_CATEGORY_COUNT];

/*
 * The checker a compose stage uses when a style is set (R-080): the stage's own checker decides what is rejected and
 * what the rules score; a candidate it accepts then gains weight times how close the candidate is to the style. A
 * candidate that is rejected stays rejected (Laws 12 and 13: a style prefers, it never forbids). Candidates of an
 * operator that has no labels (bass, texture, anything but the harmony and melody operators) are neutral, 1/1.
 */
typedef struct {
    FillCheckerFn inner;      /* the stage's checker, called with a NULL context as it is without a style */
    const ComposerStyle *style;
    ComposerRatio weight;     /* resolved: a denominator of 0 reads as 1/1 before it gets here */
} StyleCheckContext;

bool style_stage_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                         const FillCandidate *candidate, void *context, FillCheck *out);

/* Adds weight * closeness to a check's score, in lowest terms and small enough for the loop to compare (below 2^32). */
void style_add_to_score(FillCheck *check, ComposerRatio closeness, ComposerRatio weight);

#endif
