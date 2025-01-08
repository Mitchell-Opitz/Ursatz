#include <ursatz_composer/regenerate.h>

#include <stdlib.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

/* The dependents found so far, without repeats. */
typedef struct {
    FillTarget *items;
    size_t count;
    size_t capacity;
    bool failed;
} DependentList;

static bool list_has(const DependentList *list, FillTarget target) {
    for (size_t i = 0; i < list->count; i++) {
        if (fill_target_equals(list->items[i], target)) {
            return true;
        }
    }
    return false;
}

/* Adds a target that is not Given and is not there yet; true when it was added. */
static bool list_add(DependentList *list, const PiecePlan *plan, FillTarget target) {
    if (list->failed || fill_target_state(plan, target) == PLAN_STATE_GIVEN || list_has(list, target)) {
        return false;
    }
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 16;
        FillTarget *items = realloc(list->items, capacity * sizeof(FillTarget));
        if (!items) {
            list->failed = true;
            return false;
        }
        list->items = items;
        list->capacity = capacity;
    }
    list->items[list->count++] = target;
    return true;
}

static bool spans_overlap(PlanSpan a, PlanSpan b) {
    PlanTime a_end;
    PlanTime b_end;
    if (plan_span_end(a, &a_end) != PLAN_OK || plan_span_end(b, &b_end) != PLAN_OK) {
        return false;
    }
    return plan_time_compare(a.start, b_end) < 0 && plan_time_compare(b.start, a_end) < 0;
}

NodeId plan_section_of(const PiecePlan *plan, NodeId node) {
    while (node != PLAN_NODE_NONE && plan_node_kind(plan, node) != PLAN_NODE_SECTION) {
        node = plan_node_parent(plan, node);
    }
    return node;
}

/* A lane span's names, for the rules below. */
static bool has_constraint(const PiecePlan *plan, LaneSpanId id, PlanConstraintKind kind) {
    for (size_t i = 0; i < plan_lane_span_constraint_count(plan, id); i++) {
        if (plan_lane_span_constraint_at(plan, id, i)->kind == kind) {
            return true;
        }
    }
    return false;
}

static bool names_motive(const PiecePlan *plan, LaneSpanId id, NodeId motive) {
    for (size_t i = 0; i < plan_lane_span_constraint_count(plan, id); i++) {
        const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, id, i);
        if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF && constraint->params.motive.motive == motive) {
            return true;
        }
    }
    return false;
}

static bool returns_section(const PiecePlan *plan, NodeId node, NodeId section) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, node); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, node, i);
        if (constraint->kind == PLAN_CONSTRAINT_RETURN && constraint->params.section_return.section == section) {
            return true;
        }
    }
    return false;
}

/* The lane spans a node owns. */
static void add_owned_spans(DependentList *list, const PiecePlan *plan, NodeId owner) {
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        if (plan_has_lane_span(plan, id) && plan_lane_span_owner(plan, id) == owner) {
            FillTarget target = {FILL_TARGET_LANE_SPAN, id};
            list_add(list, plan, target);
        }
    }
}

/* The nodes under id and the lane spans they and id own. */
static void add_subtree(DependentList *list, const PiecePlan *plan, NodeId id, bool with_node) {
    if (with_node) {
        FillTarget target = {FILL_TARGET_NODE, id};
        list_add(list, plan, target);
    }
    add_owned_spans(list, plan, id);
    for (size_t i = 0; i < plan_node_child_count(plan, id); i++) {
        add_subtree(list, plan, plan_node_child_at(plan, id, i), true);
    }
}

/* The sections that return section, and what is written from them, and the sections that return those. */
static void add_returns(DependentList *list, const PiecePlan *plan, NodeId section) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        FillTarget target = {FILL_TARGET_NODE, id};
        if (plan_has_node(plan, id) && plan_node_kind(plan, id) == PLAN_NODE_SECTION && returns_section(plan, id, section) &&
            list_add(list, plan, target)) {
            add_subtree(list, plan, id, false);
            add_returns(list, plan, id);
        }
    }
}

static void node_dependents(DependentList *list, const PiecePlan *plan, NodeId id) {
    PlanNodeKind kind = plan_node_kind(plan, id);
    if (kind == PLAN_NODE_MOTIVE) {
        for (LaneSpanId span = 1; span <= plan_lane_span_count(plan); span++) {
            if (plan_has_lane_span(plan, span) && plan_lane_span_lane(plan, span) == PLAN_LANE_MELODY &&
                names_motive(plan, span, id)) {
                FillTarget target = {FILL_TARGET_LANE_SPAN, span};
                list_add(list, plan, target);
            }
        }
        return;
    }
    add_subtree(list, plan, id, false);
    if (kind == PLAN_NODE_SECTION) {
        add_returns(list, plan, id);
    }
}

/* Every span of a lane (any voice) over the time of span, in the section of span only when same_section. */
static void add_over(DependentList *list, const PiecePlan *plan, PlanLaneKind lane, PlanSpan span, bool only_texture) {
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        if (!plan_has_lane_span(plan, id) || plan_lane_span_lane(plan, id) != lane ||
            !spans_overlap(plan_lane_span_span(plan, id), span)) {
            continue;
        }
        if (only_texture && !has_constraint(plan, id, PLAN_CONSTRAINT_VOICING) &&
            !has_constraint(plan, id, PLAN_CONSTRAINT_COUNTERPOINT)) {
            continue;
        }
        FillTarget target = {FILL_TARGET_LANE_SPAN, id};
        list_add(list, plan, target);
    }
}

/* The span of the lane (voice 0) that starts first at or after the end of id, in the same section: the one that joins it. */
static void add_next(DependentList *list, const PiecePlan *plan, LaneSpanId id) {
    PlanLaneKind lane = plan_lane_span_lane(plan, id);
    NodeId section = plan_section_of(plan, plan_lane_span_owner(plan, id));
    PlanTime end;
    LaneSpanId next = PLAN_LANE_SPAN_NONE;
    if (plan_span_end(plan_lane_span_span(plan, id), &end) != PLAN_OK) {
        return;
    }
    for (LaneSpanId other = 1; other <= plan_lane_span_count(plan); other++) {
        if (!plan_has_lane_span(plan, other) || other == id || plan_lane_span_lane(plan, other) != lane ||
            plan_lane_span_voice(plan, other) != 0 ||
            plan_section_of(plan, plan_lane_span_owner(plan, other)) != section ||
            plan_time_compare(plan_lane_span_span(plan, other).start, end) < 0) {
            continue;
        }
        if (next == PLAN_LANE_SPAN_NONE ||
            plan_time_compare(plan_lane_span_span(plan, other).start, plan_lane_span_span(plan, next).start) < 0) {
            next = other;
        }
    }
    if (next != PLAN_LANE_SPAN_NONE) {
        FillTarget target = {FILL_TARGET_LANE_SPAN, next};
        list_add(list, plan, target);
    }
}

static void span_dependents(DependentList *list, const PiecePlan *plan, LaneSpanId id) {
    PlanSpan span = plan_lane_span_span(plan, id);
    switch (plan_lane_span_lane(plan, id)) {
    case PLAN_LANE_HARMONY:
        add_over(list, plan, PLAN_LANE_BASS, span, false);
        add_over(list, plan, PLAN_LANE_INNER_VOICE, span, false);
        add_over(list, plan, PLAN_LANE_MELODY, span, false);
        add_next(list, plan, id);
        break;
    case PLAN_LANE_BASS:
        add_over(list, plan, PLAN_LANE_INNER_VOICE, span, false);
        add_over(list, plan, PLAN_LANE_MELODY, span, false);
        break;
    case PLAN_LANE_MELODY:
        add_over(list, plan, PLAN_LANE_INNER_VOICE, span, true);
        add_next(list, plan, id);
        break;
    case PLAN_LANE_INNER_VOICE:
    case PLAN_LANE_METER:
    case PLAN_LANE_KEY:
        break;
    }
}

/* Nodes before lane spans, each by id. */
static int by_kind_then_id(const void *a, const void *b) {
    const FillTarget *x = a;
    const FillTarget *y = b;
    if (x->kind != y->kind) {
        return x->kind == FILL_TARGET_NODE ? -1 : 1;
    }
    return (x->id > y->id) - (x->id < y->id);
}

PlanResult plan_dependents(const PiecePlan *plan, FillTarget target, FillTarget **out, size_t *count) {
    if (!plan || !out || !count) {
        return PLAN_ERR_NULL;
    }
    *out = NULL;
    *count = 0;
    if (!fill_target_exists(plan, target)) {
        return PLAN_ERR_NOT_FOUND;
    }
    DependentList list = {NULL, 0, 0, false};
    if (target.kind == FILL_TARGET_NODE) {
        node_dependents(&list, plan, (NodeId)target.id);
    } else {
        span_dependents(&list, plan, (LaneSpanId)target.id);
    }
    if (list.failed) {
        free(list.items);
        return PLAN_ERR_ALLOC;
    }
    if (list.count > 1) {
        qsort(list.items, list.count, sizeof(FillTarget), by_kind_then_id);
    }
    *out = list.items;
    *count = list.count;
    return PLAN_OK;
}
