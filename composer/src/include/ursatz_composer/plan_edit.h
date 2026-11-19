#ifndef URSATZ_COMPOSER_PLAN_EDIT_H
#define URSATZ_COMPOSER_PLAN_EDIT_H

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>

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

#endif
