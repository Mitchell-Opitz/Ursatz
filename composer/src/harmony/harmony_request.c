#include <ursatz_composer/plan_state.h>

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

bool harmony_find_constraint(const PiecePlan *plan, LaneSpanId id, NodeId owner, PlanConstraintKind kind,
                             PlanConstraint *out) {
    return span_constraint(plan, id, kind, out) || node_constraint(plan, owner, kind, out);
}

bool harmony_span_has_constraint(const PiecePlan *plan, LaneSpanId id, PlanConstraintKind kind) {
    PlanConstraint unused;
    return span_constraint(plan, id, kind, &unused);
}

bool harmony_find_key(const PiecePlan *plan, LaneSpanId id, NodeId owner, PlanSpan span, PlanKey *out) {
    LaneSpanId keys[2];
    size_t found = plan_lane_find(plan, PLAN_LANE_KEY, 0, span, keys, 2);
    if (found == 1) {
        if (!plan_span_contains(plan_lane_span_span(plan, keys[0]), span)) {
            return false;
        }
        *out = plan_lane_span_payload(plan, keys[0])->key;
        return true;
    }
    PlanConstraint constraint;
    if (found == 0 && harmony_find_constraint(plan, id, owner, PLAN_CONSTRAINT_KEY, &constraint)) {
        *out = constraint.params.key;
        return true;
    }
    return false;
}

bool harmony_find_mode(const PiecePlan *plan, LaneSpanId id, NodeId owner, PlanSpan span, PlanKeyMode *out) {
    PlanKey key;
    if (!harmony_find_key(plan, id, owner, span, &key)) {
        return false;
    }
    *out = key.mode;
    return true;
}

bool harmony_read_metre(const PiecePlan *plan, PlanSpan span, PhraseMetre *out) {
    PhraseMetre metre;
    LaneSpanId meters[2];
    if (!plan || !out || plan_lane_find(plan, PLAN_LANE_METER, 0, span, meters, 2) != 1) {
        return false;
    }
    PlanSpan meter_span = plan_lane_span_span(plan, meters[0]);
    if (!plan_span_contains(meter_span, span)) {
        return false;
    }
    metre.meter = plan_lane_span_payload(plan, meters[0])->meter;
    if (plan_time_make(metre.meter.beats_per_bar, (unsigned long)metre.meter.beat_unit, &metre.bar_length) !=
        PLAN_OK) {
        return false;
    }

    /* The span starts on a barline of the meter span, and is a whole number of bars. */
    PlanTime barline = meter_span.start;
    for (size_t bar = 0; plan_time_compare(barline, span.start) < 0; bar++) {
        if (bar >= MAX_BARLINES || plan_time_add(barline, metre.bar_length, &barline) != PLAN_OK) {
            return false;
        }
    }
    if (plan_time_compare(barline, span.start) != 0) {
        return false;
    }
    PlanTime length = {0, 1};
    metre.bars = 0;
    while (plan_time_compare(length, span.length) < 0) {
        if (metre.bars >= MAX_PHRASE_BARS || plan_time_add(length, metre.bar_length, &length) != PLAN_OK) {
            return false;
        }
        metre.bars++;
    }
    if (metre.bars == 0 || plan_time_compare(length, span.length) != 0) {
        return false;
    }

    if (!harmony_rhythm_cells_per_bar(&metre.meter, &metre.cells_per_bar) ||
        plan_time_make(metre.bar_length.num, metre.bar_length.den * metre.cells_per_bar, &metre.cell_length) !=
            PLAN_OK) {
        return false;
    }
    metre.cells = metre.bars * metre.cells_per_bar;
    *out = metre;
    return true;
}

bool harmony_phrase_read_request(const PiecePlan *plan, LaneSpanId target, PhraseRequest *out) {
    if (!plan || !out || !plan_has_lane_span(plan, target) || plan_lane_span_lane(plan, target) != PLAN_LANE_HARMONY) {
        return false;
    }
    PhraseRequest request;
    PhraseMetre metre;
    request.target = target;
    request.owner = plan_lane_span_owner(plan, target);
    request.span = plan_lane_span_span(plan, target);
    if (!harmony_read_metre(plan, request.span, &metre) || harmony_span_has_constraint(plan, target, PLAN_CONSTRAINT_EMBELLISH)) {
        return false;
    }
    request.meter = metre.meter;
    request.bar_length = metre.bar_length;
    request.bars = metre.bars;
    request.cells_per_bar = metre.cells_per_bar;
    request.cell_length = metre.cell_length;
    request.cells = metre.cells;
    /* A Given bass over the span means the harmony is to be found from the bass (harmony_octave.h). */
    LaneSpanId basses[HARMONY_MAX_CHORDS];
    size_t bass_count = plan_lane_find(plan, PLAN_LANE_BASS, 0, request.span, basses, HARMONY_MAX_CHORDS);
    for (size_t i = 0; i < bass_count && i < HARMONY_MAX_CHORDS; i++) {
        if (plan_lane_span_state(plan, basses[i]) == PLAN_STATE_GIVEN) {
            return false;
        }
    }

    PlanConstraint constraint;
    if (!harmony_find_constraint(plan, target, request.owner, PLAN_CONSTRAINT_CADENCE, &constraint)) {
        return false;
    }
    request.cadence = constraint.params.cadence;
    if (!harmony_find_mode(plan, target, request.owner, request.span, &request.mode)) {
        return false;
    }
    *out = request;
    return true;
}

PlanResult harmony_request_cells_span(const PhraseRequest *request, size_t first_cell, size_t cells, PlanSpan *out) {
    return harmony_cells_span(request->span.start, request->cell_length, first_cell, cells, out);
}

PlanResult harmony_cells_span(PlanTime start, PlanTime cell_length, size_t first_cell, size_t cells, PlanSpan *out) {
    PlanTime offset;
    PlanResult result = time_scaled(cell_length, first_cell, &offset);
    if (result != PLAN_OK) {
        return result;
    }
    result = plan_time_add(start, offset, &out->start);
    if (result != PLAN_OK) {
        return result;
    }
    return time_scaled(cell_length, cells, &out->length);
}

bool harmony_count_cells(PlanTime cell_length, PlanTime length, size_t *cells) {
    PlanTime sum = {0, 1};
    size_t count = 0;
    while (plan_time_compare(sum, length) < 0) {
        if (count >= MAX_PHRASE_BARS * 4 || plan_time_add(sum, cell_length, &sum) != PLAN_OK) {
            return false;
        }
        count++;
    }
    *cells = count;
    return count > 0 && plan_time_compare(sum, length) == 0;
}
