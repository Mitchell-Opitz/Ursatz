#ifndef URSATZ_COMPOSER_REGENERATE_H
#define URSATZ_COMPOSER_REGENERATE_H

#include <ursatz_composer/compose.h>
#include <ursatz_composer/fill_target.h>
#include <ursatz_composer/locks.h>
#include <plan/plan.h>

/*
 * Regenerating one target (R-072): what depends on it is written again and nothing else is.
 *
 * What is reopened:
 *   a section                 it is reopened whole: its phrases, ideas and lane spans are removed (plan_remove_subtree),
 *                             the motives it added to the material go with them, and it goes back to Constrained; then
 *                             grow-theme builds it again. A section that returns it is reopened the same way
 *   the form                  every section that is not Given
 *   a phrase, subphrase or idea   its nodes stay (nothing builds a phrase alone): the harmony, bass, melody and
 *                             texture spans of the node and of what is under it are reopened, as for lane spans
 *   a lane span               it and its dependents (plan_dependents, repeated for each one reopened) go back to
 *                             Constrained with their constraints and an empty payload, a placeholder note or chord.
 *                             The chords and bass notes an operator wrote one span for each of are one open span
 *                             again, as grow made them. An inner voice span is reopened where it carries the
 *                             accompaniment, voicing or counterpoint asked for; the other voices are written with it
 * A target in a section that returns another section (a RETURN) reopens that section whole: a return is made from
 * its source. Whatever is reopened in a section reopens the sections that return it.
 *
 * Then compose_run runs with config's seed: the same plan, target and seed give the same plan and records. Given
 * material is never changed, so what it leaves unfillable stays open: the result is REGENERATE_ERR_STUCK and the
 * outcome names the stage (outcome->stuck_stage), as compose_run does.
 *
 * REGENERATE_ERR_LOCKED: the target is Given, or a section to reopen has Given material among its phrases and ideas.
 * REGENERATE_ERR_BAD_TARGET: a motive, the material section and what is in it, the meter and key lanes, and a span
 * the Composer did not open (a harmony span with no shape to build it from). Nothing is changed for any error but
 * REGENERATE_ERR_STUCK and REGENERATE_ERR_ALLOC.
 */
typedef enum {
    REGENERATE_OK = 0,
    REGENERATE_ERR_NULL,
    REGENERATE_ERR_ALLOC,
    REGENERATE_ERR_NOT_FOUND,
    REGENERATE_ERR_LOCKED,
    REGENERATE_ERR_BAD_TARGET,
    REGENERATE_ERR_STUCK
} RegenerateResult;

const char *regenerate_result_name(RegenerateResult result);

/*
 * What plan_regenerate_all reopens for the targets, without changing the plan: the sections reopened whole (nodes), then
 * the lane spans reopened one by one (those inside a section reopened whole are not listed), into a new array the
 * caller frees with free (NULL when empty). The errors are plan_regenerate_all's, for the first target that has one.
 */
RegenerateResult plan_regenerate_scope_all(const Plan *plan, const FillTarget *targets, size_t target_count,
                                           FillTarget **out, size_t *count);

/*
 * Reopens what depends on any of the targets (every harmony span of a section, say) and composes it once. A single
 * target is a list of one.
 */
RegenerateResult plan_regenerate_all(Plan *plan, const FillTarget *targets, size_t target_count,
                                     const ComposeConfig *config, ComposeOutcome *outcome);

#endif
