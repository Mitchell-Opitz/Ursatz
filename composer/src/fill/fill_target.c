#include <ursatz_composer/fill_target.h>

bool fill_target_equals(FillTarget a, FillTarget b) {
    return a.kind == b.kind && a.id == b.id;
}

bool fill_target_exists(const PiecePlan *plan, FillTarget target) {
    return target.kind == FILL_TARGET_NODE ? plan_has_node(plan, (NodeId)target.id)
                                           : plan_has_lane_span(plan, (LaneSpanId)target.id);
}

PlanState fill_target_state(const PiecePlan *plan, FillTarget target) {
    return target.kind == FILL_TARGET_NODE ? plan_node_state(plan, (NodeId)target.id)
                                           : plan_lane_span_state(plan, (LaneSpanId)target.id);
}

size_t fill_target_constraint_count(const PiecePlan *plan, FillTarget target) {
    return target.kind == FILL_TARGET_NODE ? plan_node_constraint_count(plan, (NodeId)target.id)
                                           : plan_lane_span_constraint_count(plan, (LaneSpanId)target.id);
}

PlanSpan fill_target_span(const PiecePlan *plan, FillTarget target) {
    return target.kind == FILL_TARGET_NODE ? plan_node_span(plan, (NodeId)target.id)
                                           : plan_lane_span_span(plan, (LaneSpanId)target.id);
}

static bool is_excluded(FillTarget target, const FillTarget *excluded, size_t excluded_count) {
    for (size_t i = 0; i < excluded_count; i++) {
        if (fill_target_equals(target, excluded[i])) {
            return true;
        }
    }
    return false;
}

/* True when candidate should be picked over the current best. Callers visit nodes before lane spans, by id. */
static bool beats(const PiecePlan *plan, FillTarget candidate, FillTarget best) {
    size_t candidate_constraints = fill_target_constraint_count(plan, candidate);
    size_t best_constraints = fill_target_constraint_count(plan, best);
    if (candidate_constraints != best_constraints) {
        return candidate_constraints > best_constraints;
    }
    int by_start = plan_time_compare(fill_target_span(plan, candidate).start, fill_target_span(plan, best).start);
    if (by_start != 0) {
        return by_start < 0;
    }
    if (candidate.kind != best.kind) {
        return candidate.kind == FILL_TARGET_NODE;
    }
    return candidate.id < best.id;
}

bool fill_pick_open_target(const PiecePlan *plan, const FillTarget *excluded, size_t excluded_count,
                           FillTarget *out) {
    if (!plan || !out) {
        return false;
    }
    bool found = false;
    FillTarget best = {FILL_TARGET_NODE, 0};
    for (size_t id = 1; id <= plan_node_count(plan); id++) {
        FillTarget target = {FILL_TARGET_NODE, id};
        if (!fill_target_exists(plan, target) || fill_target_state(plan, target) != PLAN_STATE_CONSTRAINED ||
            is_excluded(target, excluded, excluded_count)) {
            continue;
        }
        if (!found || beats(plan, target, best)) {
            best = target;
            found = true;
        }
    }
    for (size_t id = 1; id <= plan_lane_span_count(plan); id++) {
        FillTarget target = {FILL_TARGET_LANE_SPAN, id};
        if (!fill_target_exists(plan, target) || fill_target_state(plan, target) != PLAN_STATE_CONSTRAINED ||
            is_excluded(target, excluded, excluded_count)) {
            continue;
        }
        if (!found || beats(plan, target, best)) {
            best = target;
            found = true;
        }
    }
    if (found) {
        *out = best;
    }
    return found;
}
