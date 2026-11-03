#ifndef URSATZ_COMPOSER_PLAN_UTIL_H
#define URSATZ_COMPOSER_PLAN_UTIL_H

#include <stdbool.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_plan.h>
#include <ursatz_composer/plan_state.h>

/* Small helpers on the plan the sources share. Private: not installed, not part of the API. */

/* Whether two spans share any time (touching ends do not overlap); false when a span's end overflows. */
bool plan_spans_overlap(PlanSpan a, PlanSpan b);

/* Whether the lane holds notes: the bass, the melody or an inner voice. */
bool plan_is_note_lane(PlanLaneKind lane);

/* Whether node is root or lies below it. */
bool plan_in_subtree(const PiecePlan *plan, NodeId node, NodeId root);

/* The first melody lane span (voice 0) of node that is Given or Realized. */
bool plan_melody_span_of(const PiecePlan *plan, NodeId node, LaneSpanId *out);

/* The piece result of a plan result: OK, an allocation failure, else a bad request. */
PieceResult piece_result_of_plan(PlanResult result);

#endif
