#ifndef URSATZ_COMPOSER_STYLE_CHECK_H
#define URSATZ_COMPOSER_STYLE_CHECK_H

#include <stdbool.h>

#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/motive.h>
#include <plan/plan.h>
#include <ursatz_composer/style.h>

/* Private: not installed, not part of the API. What the critic does with a style: the checker of a compose stage and the labels of a motive. */

/*
 * The motive labels of a full motive (style_labels.h, the Analyzer's words): `motive-degrees` (each note's pitch class
 * as a degree of the tonic, joined by spaces), `motive-intervals` (the signed semitone steps, none for a single note) and
 * `motive-rhythm` (each note's duration, joined). A rhythm or contour motive has no pitches and adds nothing.
 */
bool composer_style_labels_add_motive(ComposerStyleLabels *labels, const Motive *motive, int tonic_pc);

/*
 * The checker a compose stage uses when a style is set (R-080): the stage's own checker decides what is rejected and
 * what the rules score; a candidate it accepts then gains weight times how close the candidate is to the style. A
 * candidate that is rejected stays rejected (Laws 12 and 13: a style prefers, it never forbids). Candidates of an
 * operator that has no labels (bass, texture, anything but the harmony and melody operators) are neutral, 1/1.
 */
typedef struct {
    FillCheckerFn inner;      /* the stage's checker */
    void *inner_context;      /* ...and the context it is called with (NULL for the stages whose checker has none) */
    const ComposerStyle *style;
    Rational weight;     /* resolved: a denominator of 0 reads as 1/1 before it gets here */
    bool texture;             /* the texture stage: only the accompaniment's candidates gain a style term (R-163) */
} StyleCheckContext;

bool style_stage_checker(const Plan *plan, FillTarget target, const FillOperator *operator_,
                         const FillCandidate *candidate, void *context, FillCheck *out);

/* Adds weight * closeness to a check's score, in lowest terms and small enough for the loop to compare (below 2^32). */
void style_add_to_score(FillCheck *check, Rational closeness, Rational weight);

#endif
