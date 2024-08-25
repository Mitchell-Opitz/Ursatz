#ifndef URSATZ_COMPOSER_PIECE_PLAN_KEYS_H
#define URSATZ_COMPOSER_PIECE_PLAN_KEYS_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/fill_rng.h>
#include <ursatz_composer/form_template.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_plan.h>

/*
 * The keys of a piece plan (R-069). A section is in the home key, or in a closely related key
 * (standard-harmony's harmony_related_key_at and harmony_key_is_closely_related) reached by a pivot chord. The key lane
 * is Given, owned by the form; a section that changes key halfway has two spans, split at its subphrase boundary (4 of
 * 8 bars, 2 of 4), and spans that follow each other in one key are one span.
 *
 *   binary          A ends in V (major) or III (minor), B starts there and returns home; or both stay home.
 *   rounded binary  the same for A and B; A' is home.
 *   ternary         A home; B starts home and ends in IV, vi or V (major) or III, iv or VI (minor); A home again.
 *   variations      all home, except with three variations or more the middle one, wholly in the relative key
 *                   (vi in major, III in minor). The parallel mode is not closely related and has no pivot.
 *   loop, layered   home.
 *
 * A section that starts in another key than the one before it, or ends in another key than it started in, is
 * where a new key opens: the target of its chords that starts there asks for a pivot chord (PLAN_HARMONY_ANCHOR_PIVOT).
 * Only a section that begins with an antecedent can open a key, so a key never opens in a presentation.
 */

/* The keys a section starts and ends in. */
typedef struct {
    PlanKey start;
    PlanKey end;
} PieceSectionKeys;

typedef struct {
    size_t count;
    PieceSectionKeys sections[FORM_TEMPLATE_MAX_SECTIONS];
    bool modulates; /* some section is in a key other than home */
} PieceKeyPlan;

/*
 * Plans the keys of the plan made by piece_build_plan before its form is Given: adds the Given key lane spans to the
 * form, over the sections and the material, and sets the KEY constraint of a section that ends in another key to the
 * key it ends in. The home key is the KEY constraint of the first section. rng NULL plans no modulation (one span, the
 * home key); otherwise every choice is drawn from rng. *out is the plan made.
 */
PieceResult piece_plan_keys(PiecePlan *plan, FillRng *rng, PieceKeyPlan *out);

#endif
