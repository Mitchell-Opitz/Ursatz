#ifndef URSATZ_COMPOSER_PIECE_INTERNAL_H
#define URSATZ_COMPOSER_PIECE_INTERNAL_H

#include <stdbool.h>

#include <plan/plan_constraint.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>

#include "grow/grow_internal.h"
#include "util/plan_util.h"

/* Each section's own motive, the four motives grow-theme makes for its ideas and the ones it makes beside them (GROW_MADE_NODES). */
#define MATERIAL_BARS_PER_SECTION (5 + GROW_MADE_NODES)
/* The weak cadence an antecedent ends in. */
#define ANTECEDENT_CADENCE CADENCE_TYPE_HALF

/*
 * The plan of a form whose sections the request gives (form_build_plan): the loop version, or with a loop plan the ending
 * version (piece_build_ending). The request was not checked.
 */
PieceResult form_build_request_plan(const FormSpec *form, const PieceRequest *request, const Plan *loop_plan,
                                    Plan **out);

/*
 * The material section, its phrase and its idea, Realized, with m0 in it: the motive as a Given melody lane span, or,
 * with none, a Constrained one-bar span with an INVENT_MOTIVE constraint. motive_source is the SOURCE constraint on m0
 * (NULL for none). The m0 node is in *out. The material span is where the section goes; bar is the meter's bar.
 */
PieceResult piece_add_material(Plan *plan, const Motive *motive, const PlanSource *motive_source, NodeId form,
                               PlanSpan material, Rational bar, NodeId *out);

/* Records the form a plan was built from (name, document version, the built-in document's hash, checks, closing), R-608. */
PieceResult piece_set_form_record(Plan *plan, const FormSpec *form);

/*
 * A motive of its own for a section in the material (R-610): the node `own <section>` holds the motive from the bar bar_index of
 * the material (material is its span, bar the meter's); piece_add_own_ref makes the section's MOTIVE_REF name it, with no step.
 */
PieceResult piece_add_own_motive(Plan *plan, NodeId idea, PlanSpan material, Rational bar, size_t bar_index, const char *section,
                                 const Motive *motive, NodeId *out);
PieceResult piece_add_own_ref(Plan *plan, NodeId id, NodeId motive);

/* Whether the plan is a piece structure plan: some section of it has a piece part (a section description with a role of a game loop part). */
bool piece_is_structure_plan(const Plan *plan);

/* The seam section of a plan (R-171), or PLAN_NODE_NONE. */
NodeId piece_seam_find(const Plan *plan);
/* The start of the first body section; false when no section is marked as a body. */
bool piece_seam_body_start(const Plan *plan, Rational *out);
/* Gives the seam a copy of the written lane spans of the bar that starts at body_start; plan_give_section's results. */
GivenResult piece_seam_give(Plan *plan, NodeId seam, Rational body_start);

/*
 * Writes the connective of a held section that has a CONNECTIVE constraint (R-486): its last bar ends with a stepwise
 * line into the first melody note of the section after it, and the constraint's `written` is set. When the line cannot be
 * written within the melodic rules nothing changes and `written` stays false (PIECE_OK either way).
 */
PieceResult piece_connective_write(Plan *plan, NodeId section);

/* Adds a constraint to a node of the plan being built. */
static inline PieceResult piece_add_constraint(Plan *plan, NodeId id, const PlanConstraint *constraint) {
    return piece_result_of_plan(plan_node_add_constraint(plan, id, constraint));
}

#endif
