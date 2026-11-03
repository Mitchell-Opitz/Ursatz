#ifndef URSATZ_COMPOSER_STYLE_INTERNAL_H
#define URSATZ_COMPOSER_STYLE_INTERNAL_H

#include <stdbool.h>

#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/style.h>

/*
 * The 128-bit unsigned integer the style share ratios compare in (StyleRational, R-251): products of two 64-bit parts
 * cannot overflow it. One definition for the style and check sources.
 */
__extension__ typedef unsigned __int128 Wide;

static inline Wide wide_gcd(Wide a, Wide b) {
    while (b) {
        Wide rest = a % b;
        a = b;
        b = rest;
    }
    return a;
}

/* Adds categories to a style's without list (copied, repeats skipped), for a blend that keeps both lists. */
ComposerStyleResult composer_style_add_without(ComposerStyle *style, const char *const *categories, size_t count);

/* True when category is in the style's without list. */
bool composer_style_without_has(const ComposerStyle *style, const char *category);

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
    ComposerRatio weight;     /* resolved: a denominator of 0 reads as 1/1 before it gets here */
    bool texture;             /* the texture stage: only the accompaniment's candidates gain a style term (R-163) */
} StyleCheckContext;

bool style_stage_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                         const FillCandidate *candidate, void *context, FillCheck *out);

/* Adds weight * closeness to a check's score, in lowest terms and small enough for the loop to compare (below 2^32). */
void style_add_to_score(FillCheck *check, ComposerRatio closeness, ComposerRatio weight);

#endif
