#ifndef URSATZ_COMPOSER_PIECE_INTERNAL_H
#define URSATZ_COMPOSER_PIECE_INTERNAL_H

#include <stdbool.h>

#include <ursatz_composer/given.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>

/*
 * The material section, its phrase and its idea, Realized, with m0 in it: the motive as a Given melody lane span, or,
 * with none, a Constrained one-bar span with an INVENT_MOTIVE constraint. motive_source is the SOURCE constraint on m0
 * (NULL for none). The m0 node is in *out. The material span is where the section goes; bar is the meter's bar.
 */
PieceResult piece_add_material(PiecePlan *plan, const Motive *motive, const PlanSource *motive_source, NodeId form,
                               PlanSpan material, PlanTime bar, NodeId *out);

/* Whether the plan is a piece structure plan: some section of it has a piece part (PLAN_CONSTRAINT_PIECE_PART). */
bool piece_is_structure_plan(const PiecePlan *plan);

/* The seam section of a plan (R-171), or PLAN_NODE_NONE. */
NodeId piece_seam_find(const PiecePlan *plan);
/* The start of the first body section; false when no section is marked as a body. */
bool piece_seam_body_start(const PiecePlan *plan, PlanTime *out);
/* Gives the seam a copy of the written lane spans of the bar that starts at body_start; plan_give_section's results. */
GivenResult piece_seam_give(PiecePlan *plan, NodeId seam, PlanTime body_start);

#endif
