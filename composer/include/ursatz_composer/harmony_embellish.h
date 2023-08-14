#ifndef URSATZ_COMPOSER_HARMONY_EMBELLISH_H
#define URSATZ_COMPOSER_HARMONY_EMBELLISH_H

#include <ursatz_composer/fill_operator.h>
#include <ursatz_composer/piece_plan.h>

/*
 * The harmony embellishment operator: a Fill operator that embellishes a plain
 * progression written by the phrase-model operator (harmony_phrase.h). It works
 * one chord at a time. A chord span the operator may change is a harmony lane
 * span that is Constrained and carries an embellish constraint, with the rest
 * of its phrase's chords beside it. harmony_embellish_open puts those marks on a
 * progression; the fill loop then visits the marked chords left to right.
 *
 * What it may change: only the marked chord (which it may replace or subdivide)
 * and, to lead into it with an applied chord, the end of the chord before it
 * when that chord is not Given. The phrase's span and its cadence are kept: the
 * chords still tile the phrase exactly, the last chord keeps the cadence
 * constraint, and standard-harmony's cadence recognizer must still hear the
 * requested cadence. The checker (harmony_phrase_checker) enforces this.
 *
 * Embellishments come in families, each switched on in the parameters:
 *   prolongation  neighbor, passing and six-four chords around a tonic or
 *                 dominant, and tonic prolongation by an inverted V or V7
 *   predominant   strong predominants (IV, ii6, ii) and predominant sevenths
 *   applied       applied dominants and leading-tone chords before their target
 * Chords are written with inversion figures (I6/4, V6/5) and applied chords
 * with standard-harmony's labels (V7/V). Only the harmony lane is written.
 */

#define HARMONY_EMBELLISH_OPERATOR_NAME "harmony-embellish"

typedef enum {
    HARMONY_EMBELLISH_PROLONGATION = 1u << 0,
    HARMONY_EMBELLISH_PREDOMINANT = 1u << 1,
    HARMONY_EMBELLISH_APPLIED = 1u << 2,
    HARMONY_EMBELLISH_ALL = (1u << 3) - 1
} HarmonyEmbellishFamily;

/*
 * density is the chance, in thousandths, that a marked chord is embellished at
 * all: 0 never changes the progression, 1000 always tries (a chord nothing fits
 * is left as it is). families is a mask of HarmonyEmbellishFamily; with none set
 * the progression is left as it is. A density above 1000 counts as 1000.
 */
typedef struct {
    unsigned density;
    unsigned families;
} HarmonyEmbellishParams;

/* The defaults: density 500, every family on. */
HarmonyEmbellishParams harmony_embellish_params_default(void);

/*
 * Registers the operator with these parameters. The registry keeps the pointer,
 * not a copy, so params must outlive it (NULL means the defaults).
 */
FillResult harmony_embellish_register(FillOperatorRegistry *registry, const HarmonyEmbellishParams *params);

/*
 * Marks a progression for embellishment: every Realized harmony lane span that
 * overlaps range becomes Constrained with an embellish constraint. Given spans
 * and spans that are already open are left alone.
 */
PlanResult harmony_embellish_open(PiecePlan *plan, PlanSpan range);

#endif
