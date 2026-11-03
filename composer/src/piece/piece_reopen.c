#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_structure.h>
#include <ursatz_composer/plan_state.h>

#include "piece/piece_internal.h"
#include "plan/plan_internal.h"
#include "util/plan_util.h"

#define PIECE_REOPEN_RUNS_MAX 64

/*
 * What regenerating a piece's section needs besides regenerate.h: finding a section by its id, the sections the loop's
 * end holds back for the seam, the seam's copy of the body's first bar, and the lanes a filled section was given.
 */

NodeId piece_section_find(const PiecePlan *plan, const char *id) {
    for (NodeId node = 1; node <= plan_node_count(plan); node++) {
        if (!plan_has_node(plan, node) || plan_node_kind(plan, node) != PLAN_NODE_SECTION) {
            continue;
        }
        for (size_t i = 0; i < plan_node_constraint_count(plan, node); i++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, node, i);
            if (constraint->kind == PLAN_CONSTRAINT_PIECE_PART && strcmp(constraint->params.piece_part.id, id) == 0) {
                return node;
            }
        }
    }
    return PLAN_NODE_NONE;
}

size_t piece_held_sections(const PiecePlan *plan, NodeId *out, size_t capacity) {
    size_t count = 0;
    for (NodeId node = 1; node <= plan_node_count(plan); node++) {
        if (plan_has_node(plan, node) && plan_node_kind(plan, node) == PLAN_NODE_SECTION &&
            plan_node_has_constraint(plan, node, PLAN_CONSTRAINT_HOLD)) {
            if (out && count < capacity) {
                out[count] = node;
            }
            count++;
        }
    }
    return count;
}

bool piece_section_holds_body_start(const PiecePlan *plan, NodeId section) {
    Rational body_start;
    if (!plan_has_node(plan, section) || !piece_seam_body_start(plan, &body_start)) {
        return false;
    }
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, section, i);
        if (constraint->kind == PLAN_CONSTRAINT_PIECE_PART && constraint->params.piece_part.part == (int)PIECE_PART_BODY &&
            rational_cmp(plan_node_span(plan, section).start, body_start) == 0) {
            return true;
        }
    }
    return false;
}

PieceResult piece_seam_reset(PiecePlan *plan) {
    NodeId seam = piece_seam_find(plan);
    if (seam != PLAN_NODE_NONE && plan_lane_span_remove_owned(plan, seam) != PLAN_OK) {
        return PIECE_ERR_BAD_REQUEST;
    }
    return PIECE_OK;
}

/* Whether the span lies inside the node's. */
static bool span_inside(PlanSpan span, PlanSpan outer) {
    Rational span_end;
    Rational outer_end;
    return plan_span_end(span, &span_end) == PLAN_OK && plan_span_end(outer, &outer_end) == PLAN_OK &&
           rational_cmp(span.start, outer.start) >= 0 && rational_cmp(span_end, outer_end) <= 0;
}

/*
 * A section given its lanes owns every span itself (plan_give_section). The operators read a span's shape and cadence from
 * its owner, and write the chords of a phrase model or a prolongation for one owner, as grow made them: for the phrase,
 * subphrase or idea that holds the whole run. So a reopened span goes to the smallest node under the section that holds
 * its time; for harmony and bass, the time of the run of chords it is in (a run starts at a chord with a harmony shape).
 */
static NodeId finest_owner(const PiecePlan *plan, NodeId section, PlanSpan span) {
    NodeId best = section;
    for (NodeId node = 1; node <= plan_node_count(plan); node++) {
        if (node == section || !plan_has_node(plan, node) || !plan_in_subtree(plan, node, section)) {
            continue;
        }
        PlanSpan inner = plan_node_span(plan, node);
        if (span_inside(span, inner) && (best == section || rational_cmp(inner.length, plan_node_span(plan, best).length) < 0)) {
            best = node;
        }
    }
    return best;
}

typedef struct {
    Rational start;
    Rational end;
    NodeId owner;
} ChordRun;

static bool has_shape(const PiecePlan *plan, LaneSpanId id) {
    for (size_t i = 0; i < plan_lane_span_constraint_count(plan, id); i++) {
        if (plan_lane_span_constraint_at(plan, id, i)->kind == PLAN_CONSTRAINT_HARMONY_SHAPE) {
            return true;
        }
    }
    return false;
}

/* The runs of chords the section owns itself, each with the node that holds it; their number. */
static size_t read_runs(const PiecePlan *plan, NodeId section, ChordRun *runs, size_t capacity) {
    LaneSpanId *ids = calloc(plan_lane_span_count(plan) + 1, sizeof(LaneSpanId));
    size_t count = 0;
    size_t run_count = 0;
    if (!ids) {
        return 0;
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        if (plan_has_lane_span(plan, id) && plan_lane_span_lane(plan, id) == PLAN_LANE_HARMONY &&
            plan_lane_span_owner(plan, id) == section) {
            size_t at = count++;
            while (at > 0 && rational_cmp(plan_lane_span_span(plan, id).start, plan_lane_span_span(plan, ids[at - 1]).start) < 0) {
                ids[at] = ids[at - 1];
                at--;
            }
            ids[at] = id;
        }
    }
    for (size_t i = 0; i < count && run_count <= capacity; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        Rational end;
        if (plan_span_end(span, &end) != PLAN_OK) {
            continue;
        }
        if (run_count == 0 || has_shape(plan, ids[i])) {
            if (run_count == capacity) {
                break;
            }
            runs[run_count].start = span.start;
            runs[run_count].owner = section;
            run_count++;
        }
        runs[run_count - 1].end = end;
    }
    for (size_t r = 0; r < run_count; r++) {
        PlanSpan whole = {runs[r].start, {0, 1}};
        rational_sub(runs[r].end, runs[r].start, &whole.length);
        runs[r].owner = finest_owner(plan, section, whole);
    }
    free(ids);
    return run_count;
}

size_t piece_reopen_lane(PiecePlan *plan, NodeId section, PlanLaneKind lane) {
    ChordRun runs[PIECE_REOPEN_RUNS_MAX];
    size_t run_count = lane == PLAN_LANE_HARMONY || lane == PLAN_LANE_BASS ? read_runs(plan, section, runs, PIECE_REOPEN_RUNS_MAX) : 0;
    size_t reopened = 0;
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        LaneSpan *span = plan_lane_span_at(plan, id);
        if (!span || span->lane != lane || span->state != PLAN_STATE_GIVEN || !plan_in_subtree(plan, span->owner, section)) {
            continue;
        }
        span->state = PLAN_STATE_REALIZED;
        if (span->owner == section) {
            NodeId owner = PLAN_NODE_NONE;
            for (size_t r = 0; r < run_count; r++) {
                if (rational_cmp(span->span.start, runs[r].start) >= 0 && rational_cmp(span->span.start, runs[r].end) < 0) {
                    owner = runs[r].owner;
                }
            }
            span->owner = owner != PLAN_NODE_NONE ? owner : finest_owner(plan, section, span->span);
        }
        reopened++;
    }
    return reopened;
}
