#ifndef URSATZ_COMPOSER_REGENERATE_H
#define URSATZ_COMPOSER_REGENERATE_H

#include <ursatz_composer/compose.h>
#include <ursatz_composer/fill_target.h>
#include <ursatz_composer/piece_plan.h>

/*
 * Locking and regenerating (R-072).
 *
 * plan_lock makes material Given: a node and every node under it and every lane span they own, or one lane span
 * alone. Given material is never changed by the Composer, and nothing leaves Given, so locking is permanent in the
 * plan; unlocking is reading an earlier plan file.
 *
 * Only what is written can be locked: a lane span still open (Constrained) has nothing to keep, and locking it would
 * leave a hole nothing may fill. The call is then refused with PLAN_ERR_BAD_PAYLOAD and nothing is changed.
 * PLAN_ERR_NOT_FOUND for a target that does not exist.
 */
PlanResult plan_lock(PiecePlan *plan, FillTarget target);

/*
 * What is written from a target (its direct dependents; Given ones are left out), into a new array the caller frees
 * with free (NULL when there are none), nodes before lane spans, each by id:
 *   a structure node          its whole subtree and the lane spans of the node and of the subtree
 *   a section                 as a structure node, and every section whose RETURN names it, with its subtree and its
 *                             lane spans, and the sections that return those
 *   a harmony span            the bass, inner voice and melody spans over its time, and the next harmony span of the
 *                             same section (the chord that joins it)
 *   a bass span               the inner voice and melody spans over its time
 *   a melody span             the inner voice spans with a VOICING or COUNTERPOINT constraint over its time, and the
 *                             next melody span of the same section (the idea that joins it)
 *   a material motive node    every melody span whose MOTIVE_REF names it
 * An inner voice, meter or key span has none. PLAN_ERR_NOT_FOUND for a target that does not exist.
 */
PlanResult plan_dependents(const PiecePlan *plan, FillTarget target, FillTarget **out, size_t *count);

/* The section a node is in (the node itself for a section), PLAN_NODE_NONE for the form. */
NodeId plan_section_of(const PiecePlan *plan, NodeId node);

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
 * What plan_regenerate reopens for a target, without changing the plan: the sections reopened whole (nodes), then
 * the lane spans reopened one by one (those inside a section reopened whole are not listed), into a new array the
 * caller frees with free (NULL when empty). The errors are plan_regenerate's.
 */
RegenerateResult plan_regenerate_scope(const PiecePlan *plan, FillTarget target, FillTarget **out, size_t *count);

RegenerateResult plan_regenerate(PiecePlan *plan, FillTarget target, const ComposeConfig *config, ComposeOutcome *outcome);

#endif
