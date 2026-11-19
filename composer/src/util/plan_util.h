#ifndef URSATZ_COMPOSER_PLAN_UTIL_H
#define URSATZ_COMPOSER_PLAN_UTIL_H

#include <stdbool.h>

#include <model/time/time_signature.h>

#include <plan/plan_constraint.h>
#include <plan/plan_lanes.h>
#include <ursatz_composer/piece.h>
#include <plan/plan.h>
#include <plan/plan_state.h>

#include <plan/plan_common.h>

/* Small helpers on the plan the sources share. Private: not installed, not part of the API. */

/* Whether node is root or lies below it. */
bool plan_in_subtree(const PiecePlan *plan, NodeId node, NodeId root);

/* The first melody lane span (voice 0) of node that is Given or Realized. */
bool plan_melody_span_of(const PiecePlan *plan, NodeId node, LaneSpanId *out);

/* The part of a larger form a section node is (its description's function, R-503); false when it has none. */
bool plan_section_function_of(const PiecePlan *plan, NodeId node, SectionFunction *out);

/* The piece result of a plan result: OK, an allocation failure, else a bad request. */
PieceResult piece_result_of_plan(PlanResult result);

/* Adds a node under parent and sets it Given. */
bool plan_add_given_node(PiecePlan *plan, NodeId parent, PlanNodeKind kind, PlanSpan span, NodeId *out);

/* The fragment position names ("any", "leading", "trailing", "middle"), indexed by the fragment step's position (0 for none). */
#define PLAN_FRAGMENT_POSITION_NAME_COUNT 4
extern const char *const plan_fragment_position_names[PLAN_FRAGMENT_POSITION_NAME_COUNT];
/* The name of a fragment position; "any" for a value that is not one. */
const char *plan_fragment_position_name(long position);

#endif
