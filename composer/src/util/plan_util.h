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

/* Adds a node under parent and sets it Given. */
bool plan_add_given_node(PiecePlan *plan, NodeId parent, PlanNodeKind kind, PlanSpan span, NodeId *out);

/* The lane names the plan file and the command lines use, in PlanLaneKind order. */
#define PLAN_LANE_NAME_COUNT 6
extern const char *const plan_lane_names[PLAN_LANE_NAME_COUNT];
/* The name of a lane; "" for a value that is not one. */
const char *plan_lane_name(PlanLaneKind lane);

/* The fragment position names ("any", "leading", "trailing", "middle"), indexed by the fragment step's position (0 for none). */
#define PLAN_FRAGMENT_POSITION_NAME_COUNT 4
extern const char *const plan_fragment_position_names[PLAN_FRAGMENT_POSITION_NAME_COUNT];
/* The name of a fragment position; "any" for a value that is not one. */
const char *plan_fragment_position_name(long position);

/*
 * The cadence types a plan holds are the library's CadenceType: PAC, IAC, HC, DC and PC (perfect and imperfect authentic,
 * half, deceptive and plagal). The plan file and the command line name them with these tables, in this order.
 */
#define PLAN_CADENCE_COUNT 5
extern const CadenceType plan_cadence_types[PLAN_CADENCE_COUNT];
extern const char *const plan_cadence_names[PLAN_CADENCE_COUNT];      /* "PAC", as the command line writes it */
extern const char *const plan_cadence_file_names[PLAN_CADENCE_COUNT]; /* "pac", as the plan file writes it */
/* The place of a type in the tables; false for a type the plan does not hold (subverted, unspecified, unknown). */
bool plan_cadence_index(CadenceType type, size_t *index);
/* The command line name of a type the plan holds, else "". */
const char *plan_cadence_name(CadenceType type);

/* PLAN_OK for RATIONAL_OK, PLAN_ERR_NULL for a NULL result, else PLAN_ERR_BAD_TIME (a zero denominator or overflow). */
PlanResult plan_result_of_rational(RationalResult result);

#endif
