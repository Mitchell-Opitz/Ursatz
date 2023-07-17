#include "harmony/harmony_internal.h"

/* The most bars a phrase can span, and the most barlines searched before a phrase; guards for the loops below. */
#define MAX_PHRASE_BARS 1024
#define MAX_BARLINES 1000000

static PlanResult time_scaled(PlanTime time, size_t count, PlanTime *out) {
    return plan_time_make(time.num * (long)count, time.den, out);
}

static bool span_constraint(const PiecePlan *plan, LaneSpanId id, PlanConstraintKind kind, PlanConstraint *out) {
    size_t count = plan_lane_span_constraint_count(plan, id);
    for (size_t i = 0; i < count; i++) {
        const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, id, i);
        if (constraint->kind == kind) {
            *out = *constraint;
            return true;
        }
    }
    return false;
}

static bool node_constraint(const PiecePlan *plan, NodeId id, PlanConstraintKind kind, PlanConstraint *out) {
    size_t count = plan_node_constraint_count(plan, id);
    for (size_t i = 0; i < count; i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, id, i);
        if (constraint->kind == kind) {
            *out = *constraint;
            return true;
        }
    }
    return false;
}

/* A constraint of this kind on the span, else on its owner. */
static bool find_constraint(const PiecePlan *plan, LaneSpanId id, NodeId owner, PlanConstraintKind kind,
                            PlanConstraint *out) {
    return span_constraint(plan, id, kind, out) || node_constraint(plan, owner, kind, out);
}

/* The mode of the key lane over the span, else of a key constraint. */
static bool find_mode(const PiecePlan *plan, LaneSpanId id, NodeId owner, PlanSpan span, PlanKeyMode *out) {
    LaneSpanId keys[2];
    size_t found = plan_lane_find(plan, PLAN_LANE_KEY, 0, span, keys, 2);
    if (found == 1) {
        if (!plan_span_contains(plan_lane_span_span(plan, keys[0]), span)) {
            return false;
        }
        *out = plan_lane_span_payload(plan, keys[0])->key.mode;
        return true;
    }
    PlanConstraint constraint;
    if (found == 0 && find_constraint(plan, id, owner, PLAN_CONSTRAINT_KEY, &constraint)) {
        *out = constraint.params.key.mode;
        return true;
    }
    return false;
}

/* The harmonic rhythm of a meter: how many places in a bar a chord may start. */
static size_t cells_per_bar_of(const PlanMeter *meter) {
    (void)meter;
    return 1;
}

bool harmony_phrase_read_request(const PiecePlan *plan, LaneSpanId target, PhraseRequest *out) {
    if (!plan || !out || !plan_has_lane_span(plan, target) || plan_lane_span_lane(plan, target) != PLAN_LANE_HARMONY) {
        return false;
    }
    PhraseRequest request;
    request.target = target;
    request.owner = plan_lane_span_owner(plan, target);
    request.span = plan_lane_span_span(plan, target);

    LaneSpanId meters[2];
    if (plan_lane_find(plan, PLAN_LANE_METER, 0, request.span, meters, 2) != 1) {
        return false;
    }
    PlanSpan meter_span = plan_lane_span_span(plan, meters[0]);
    if (!plan_span_contains(meter_span, request.span)) {
        return false;
    }
    request.meter = plan_lane_span_payload(plan, meters[0])->meter;
    if (plan_time_make(request.meter.beats_per_bar, (unsigned long)request.meter.beat_unit, &request.bar_length) !=
        PLAN_OK) {
        return false;
    }

    /* The span starts on a barline of the meter span, and is a whole number of bars. */
    PlanTime barline = meter_span.start;
    for (size_t bar = 0; plan_time_compare(barline, request.span.start) < 0; bar++) {
        if (bar >= MAX_BARLINES || plan_time_add(barline, request.bar_length, &barline) != PLAN_OK) {
            return false;
        }
    }
    if (plan_time_compare(barline, request.span.start) != 0) {
        return false;
    }
    PlanTime length = {0, 1};
    request.bars = 0;
    while (plan_time_compare(length, request.span.length) < 0) {
        if (request.bars >= MAX_PHRASE_BARS || plan_time_add(length, request.bar_length, &length) != PLAN_OK) {
            return false;
        }
        request.bars++;
    }
    if (request.bars == 0 || plan_time_compare(length, request.span.length) != 0) {
        return false;
    }

    request.cells_per_bar = cells_per_bar_of(&request.meter);
    if (request.cells_per_bar == 0 ||
        plan_time_make(request.bar_length.num, request.bar_length.den * request.cells_per_bar,
                       &request.cell_length) != PLAN_OK) {
        return false;
    }
    request.cells = request.bars * request.cells_per_bar;

    PlanConstraint constraint;
    if (!find_constraint(plan, target, request.owner, PLAN_CONSTRAINT_CADENCE, &constraint)) {
        return false;
    }
    request.cadence = constraint.params.cadence;
    if (!find_mode(plan, target, request.owner, request.span, &request.mode)) {
        return false;
    }
    *out = request;
    return true;
}

PlanResult harmony_request_cells_span(const PhraseRequest *request, size_t first_cell, size_t cells, PlanSpan *out) {
    PlanTime offset;
    PlanResult result = time_scaled(request->cell_length, first_cell, &offset);
    if (result != PLAN_OK) {
        return result;
    }
    result = plan_time_add(request->span.start, offset, &out->start);
    if (result != PLAN_OK) {
        return result;
    }
    return time_scaled(request->cell_length, cells, &out->length);
}
