#ifndef URSATZ_COMPOSER_LOCKS_H
#define URSATZ_COMPOSER_LOCKS_H

#include <ursatz_composer/fill_target.h>
#include <plan/plan.h>

/*
 * Locking (R-072): what is kept, and what depends on a target.
 *
 * plan_lock makes material Given: a node and every node under it and every lane span they own, or one lane span
 * alone. Given material is never changed by the Composer, and nothing leaves Given, so locking is permanent in the
 * plan; unlocking is reading an earlier plan file.
 *
 * Only what is written can be locked: a node or lane span still open (Constrained) has nothing to keep, and locking it
 * would leave a hole nothing may fill. The call is then refused with PLAN_ERR_BAD_PAYLOAD and nothing is changed.
 * PLAN_ERR_NOT_FOUND for a target that does not exist.
 */
PlanResult plan_lock(Plan *plan, FillTarget target);

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
PlanResult plan_dependents(const Plan *plan, FillTarget target, FillTarget **out, size_t *count);

/* The section a node is in (the node itself for a section), PLAN_NODE_NONE for the form. */
NodeId plan_section_of(const Plan *plan, NodeId node);

#endif
