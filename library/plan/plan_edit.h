#ifndef URSATZ_PLAN_EDIT_H
#define URSATZ_PLAN_EDIT_H

#include "plan/plan_lanes.h"
#include "plan/plan.h"

/*
 * Whole-plan edits (A20261109-50): what a caller does to a plan without writing into its internals or printing it to JSON and reading
 * it back.
 *
 * plan_clone gives the plan plan_from_json(plan_to_json(plan)) gives, without the text: the live nodes and lane spans renumbered from 1
 * in id order (a removed one is dropped, and the parents, owners, children and the node references of the constraints are remapped),
 * every payload, note (with its id, source and operation), constraint, section description, key relation and state, the form record,
 * the Analyzer version and the next note id. The caller destroys *out; on failure *out is NULL. PLAN_ERR_NULL for no plan or no out,
 * PLAN_ERR_ALLOC.
 *
 * plan_copy_into replaces everything `plan` holds with a clone of `source`: ids and pointers into `plan`'s old contents are void.
 * On failure `plan` is untouched.
 */
PlanResult plan_clone(const PiecePlan *plan, PiecePlan **out);
PlanResult plan_copy_into(PiecePlan *plan, const PiecePlan *source);

/*
 * Edits the operators make to a plan that the plan's own functions do not offer.
 *
 * plan_lane_span_reopen: a Given lane span becomes Realized and gets `owner`, a node whose span holds the span's: the one sanctioned
 * way out of Given, used where a section is reopened for a new pass and a span it held goes to the finer node that now holds it.
 * PLAN_ERR_NOT_FOUND for an unknown span or owner, PLAN_ERR_BAD_SPAN when the owner's span does not hold the span's,
 * PLAN_ERR_BAD_KIND for a span that is not Given.
 */
PlanResult plan_lane_span_reopen(PiecePlan *plan, LaneSpanId span, NodeId owner);

/* Removes every lane span owner owns, whatever its state: the caller has checked that none is Given. */
PlanResult plan_lane_span_remove_owned(PiecePlan *plan, NodeId owner);

/*
 * Removes one lane span: PLAN_ERR_GIVEN_LOCKED for a Given one, PLAN_ERR_NOT_FOUND for an unknown id. What other
 * spans or nodes name it is not touched (nothing names a lane span).
 */
PlanResult plan_lane_span_remove(PiecePlan *plan, LaneSpanId id);

/*
 * Removes one node that has no children, with the lane spans it owns and the constraints that name it, as
 * plan_remove_subtree does for a subtree. PLAN_ERR_BAD_KIND for the root or a node with children,
 * PLAN_ERR_GIVEN_LOCKED when it or anything removed with it is Given, or a Given holder names it.
 */
PlanResult plan_node_remove_leaf(PiecePlan *plan, NodeId id);

#endif
