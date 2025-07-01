#ifndef URSATZ_COMPOSER_VARIATIONS_H
#define URSATZ_COMPOSER_VARIATIONS_H

#include <stddef.h>
#include <stdint.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/fill_target.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/regenerate.h>

/*
 * Variations (R-088): several distinct versions of one part of a filled plan.
 *
 * plan_variations regenerates the targets (plan_regenerate_all) on a copy of the plan for the seeds config->seed,
 * config->seed + 1, ..., at most 4 * count of them, and keeps the copies that sound different: a copy is kept when its
 * signature differs from the original plan's and from every copy kept before. A seed that gets stuck is skipped. The
 * input plan is never changed; each copy is made with plan_to_json and plan_from_json, so a plan must have the ids a
 * plan file has (no removed nodes or lane spans), or REGENERATE_ERR_BAD_TARGET is returned.
 *
 * The signature is the FNV-1a 64 hash over the events of plan_render, in order: for each its part index, its onset and
 * duration as reduced numerator and denominator, and its MIDI pitch. Two plans with equal signatures render the same
 * notes in the same parts.
 *
 * count is 1 to 16, else REGENERATE_ERR_BAD_TARGET. The errors that plan_regenerate_all gives before any seed (locked,
 * bad target, not found) are returned as it gives them, and out is then empty. Otherwise REGENERATE_OK, with
 * out->count between 0 and count: the caller decides what none kept means. out->seeds_tried is the seeds run.
 */
typedef struct {
    PiecePlan *plan;       /* owned */
    unsigned long seed;    /* the seed that made it */
    uint64_t signature;
    ComposeOutcome outcome; /* owned */
} PlanVariation;

typedef struct {
    PlanVariation *items; /* owned; count kept, in the order found */
    size_t count;
    size_t seeds_tried;
} PlanVariations;

#define PLAN_VARIATIONS_MAX 16

RegenerateResult plan_variations(const PiecePlan *plan, const FillTarget *targets, size_t target_count,
                                 const ComposeConfig *config, size_t count, PlanVariations *out);

void plan_variations_destroy(PlanVariations *variations);

/*
 * Alternative harmonizations (R-088): the harmony of one section written again over a melody that is given. The melody
 * lane spans of the section must all be Given, and there must be at least one (else REGENERATE_ERR_BAD_TARGET, as it is
 * when section is not a section); the targets are every harmony lane span of the section that is not Given; then
 * plan_variations, except that the signature is the FNV-1a 64 hash of the harmony lane alone (each span's place and
 * numeral), so a harmonization is kept only when its chords differ from the plan's and from those kept before. The melody is never changed, so every
 * variation keeps every melody note.
 */
RegenerateResult plan_harmonizations(const PiecePlan *plan, NodeId section, const ComposeConfig *config, size_t count,
                                     PlanVariations *out);

/* The signature of a plan's rendering, described above. 0 when the plan cannot be rendered. */
uint64_t plan_signature(const PiecePlan *plan);

#endif
