#include <ursatz_composer/regenerate.h>

#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

#include "grow/grow_internal.h"
#include "plan/plan_internal.h"

const char *regenerate_result_name(RegenerateResult result) {
    static const char *const NAMES[] = {"ok",          "null argument", "out of memory", "not found",
                                        "given (locked)", "cannot be regenerated", "stuck"};
    return (size_t)result < sizeof(NAMES) / sizeof(NAMES[0]) ? NAMES[result] : "unknown";
}

/* ------------------------------------------------------------- the scope */

/* The targets reached so far, marked by id, and the ones whose dependents are still to be read. */
typedef struct {
    const PiecePlan *plan;
    bool *node_marked;
    bool *span_marked;
    bool *returns_added; /* by section: the sections that return it have been added */
    FillTarget *queue;
    size_t head;
    size_t tail;
    bool failed;
} Scope;

static bool scope_init(Scope *scope, const PiecePlan *plan) {
    size_t nodes = plan_node_count(plan) + 1;
    size_t spans = plan_lane_span_count(plan) + 1;
    memset(scope, 0, sizeof(*scope));
    scope->plan = plan;
    scope->node_marked = calloc(nodes, sizeof(bool));
    scope->span_marked = calloc(spans, sizeof(bool));
    scope->returns_added = calloc(nodes, sizeof(bool));
    scope->queue = calloc(nodes + spans, sizeof(FillTarget));
    return scope->node_marked && scope->span_marked && scope->returns_added && scope->queue;
}

static void scope_free(Scope *scope) {
    free(scope->node_marked);
    free(scope->span_marked);
    free(scope->returns_added);
    free(scope->queue);
}

static bool *mark_of(Scope *scope, FillTarget target) {
    return target.kind == FILL_TARGET_NODE ? &scope->node_marked[target.id] : &scope->span_marked[target.id];
}

static void scope_add(Scope *scope, FillTarget target) {
    if (!fill_target_exists(scope->plan, target) || fill_target_state(scope->plan, target) == PLAN_STATE_GIVEN ||
        *mark_of(scope, target)) {
        return;
    }
    *mark_of(scope, target) = true;
    scope->queue[scope->tail++] = target;
}

/* A section the theme operators grow: an archetype and a theme pairing. */
static bool is_theme_section(const PiecePlan *plan, NodeId id) {
    return plan_node_kind(plan, id) == PLAN_NODE_SECTION && plan_node_has_constraint(plan, id, PLAN_CONSTRAINT_ARCHETYPE) &&
           plan_node_has_constraint(plan, id, PLAN_CONSTRAINT_THEME_PAIRING);
}

/* Whether the node is the material section or under it. */
static bool in_material(const PiecePlan *plan, NodeId id) {
    for (; id != PLAN_NODE_NONE; id = plan_node_parent(plan, id)) {
        if (plan_node_has_constraint(plan, id, PLAN_CONSTRAINT_MATERIAL)) {
            return true;
        }
    }
    return false;
}

/* Whether a span the closure reaches is one it reopens: not a meter or key span, an inner voice only where it carries what
   its voices are written from. */
static bool span_is_reopened(const PiecePlan *plan, LaneSpanId id) {
    switch (plan_lane_span_lane(plan, id)) {
    case PLAN_LANE_HARMONY:
    case PLAN_LANE_BASS:
    case PLAN_LANE_MELODY:
        return true;
    case PLAN_LANE_INNER_VOICE:
        return plan_lane_span_find_constraint(plan, id, PLAN_CONSTRAINT_ACCOMPANIMENT) != NULL ||
               plan_lane_span_find_constraint(plan, id, PLAN_CONSTRAINT_VOICING) != NULL ||
               plan_lane_span_find_constraint(plan, id, PLAN_CONSTRAINT_COUNTERPOINT) != NULL;
    case PLAN_LANE_METER:
    case PLAN_LANE_KEY:
        break;
    }
    return false;
}

/* Whether a span of the same owner and lane as `other`, already reached, touches it end to start. */
static bool touches_reached(const Scope *scope, LaneSpanId other) {
    const PiecePlan *plan = scope->plan;
    PlanSpan span = plan_lane_span_span(plan, other);
    PlanTime other_end;
    plan_span_end(span, &other_end);
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        if (!scope->span_marked[id] || plan_lane_span_lane(plan, id) != plan_lane_span_lane(plan, other) ||
            plan_lane_span_owner(plan, id) != plan_lane_span_owner(plan, other)) {
            continue;
        }
        PlanSpan reached = plan_lane_span_span(plan, id);
        PlanTime reached_end;
        plan_span_end(reached, &reached_end);
        if (plan_time_compare(reached_end, span.start) == 0 || plan_time_compare(other_end, reached.start) == 0) {
            return true;
        }
    }
    return false;
}

/*
 * The harmony and bass spans of the same owner that touch the ones reached end to end, none Given: the chords or bass
 * notes one operator wrote by splitting one open span, which are opened again as one.
 */
static void add_run(Scope *scope, LaneSpanId id) {
    const PiecePlan *plan = scope->plan;
    PlanLaneKind lane = plan_lane_span_lane(plan, id);
    if (lane != PLAN_LANE_HARMONY && lane != PLAN_LANE_BASS) {
        return;
    }
    bool grown = true;
    while (grown) {
        grown = false;
        for (LaneSpanId other = 1; other <= plan_lane_span_count(plan); other++) {
            if (plan_has_lane_span(plan, other) && !scope->span_marked[other] && plan_lane_span_lane(plan, other) == lane &&
                plan_lane_span_owner(plan, other) == plan_lane_span_owner(plan, id) && touches_reached(scope, other)) {
                FillTarget target = {FILL_TARGET_LANE_SPAN, other};
                scope_add(scope, target);
                grown = grown || scope->span_marked[other];
            }
        }
    }
}

/* The sections that return section: reopening what a section holds reopens them. */
static void add_returners(Scope *scope, NodeId section) {
    const PiecePlan *plan = scope->plan;
    if (section == PLAN_NODE_NONE || scope->returns_added[section]) {
        return;
    }
    scope->returns_added[section] = true;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        for (size_t i = 0; plan_has_node(plan, id) && i < plan_node_constraint_count(plan, id); i++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, id, i);
            if (constraint->kind == PLAN_CONSTRAINT_RETURN && constraint->params.section_return.section == section) {
                FillTarget target = {FILL_TARGET_NODE, id};
                scope_add(scope, target);
            }
        }
    }
}

static NodeId owner_node(const PiecePlan *plan, FillTarget target) {
    return target.kind == FILL_TARGET_NODE ? (NodeId)target.id : plan_lane_span_owner(plan, (LaneSpanId)target.id);
}

/* The section a section that returns another is reopened as: a return is made from its source. */
static NodeId return_section_of(const PiecePlan *plan, FillTarget target) {
    NodeId section = plan_section_of(plan, owner_node(plan, target));
    if (section != PLAN_NODE_NONE && plan_node_has_constraint(plan, section, PLAN_CONSTRAINT_RETURN) &&
        plan_node_state(plan, section) != PLAN_STATE_GIVEN) {
        return section;
    }
    return PLAN_NODE_NONE;
}

static RegenerateResult scope_build(const PiecePlan *plan, const FillTarget *targets, size_t target_count,
                                    FillTarget **out, size_t *count) {
    if (!plan || !out || !count || !targets || target_count == 0) {
        return REGENERATE_ERR_NULL;
    }
    *out = NULL;
    *count = 0;
    for (size_t t = 0; t < target_count; t++) {
        FillTarget target = targets[t];
        if (!fill_target_exists(plan, target)) {
            return REGENERATE_ERR_NOT_FOUND;
        }
        if (fill_target_state(plan, target) == PLAN_STATE_GIVEN) {
            return REGENERATE_ERR_LOCKED;
        }
        NodeId home = owner_node(plan, target);
        if (target.kind == FILL_TARGET_NODE) {
            PlanNodeKind kind = plan_node_kind(plan, home);
            if (kind == PLAN_NODE_MOTIVE || in_material(plan, home)) {
                return REGENERATE_ERR_BAD_TARGET;
            }
        } else if (in_material(plan, home) || !span_is_reopened(plan, (LaneSpanId)target.id)) {
            return REGENERATE_ERR_BAD_TARGET;
        }
    }

    Scope scope;
    if (!scope_init(&scope, plan)) {
        scope_free(&scope);
        return REGENERATE_ERR_ALLOC;
    }
    for (size_t t = 0; t < target_count; t++) {
        FillTarget target = targets[t];
        /* A target in a return reopens the whole of it. */
        NodeId returned = return_section_of(plan, target);
        if (returned != PLAN_NODE_NONE) {
            target.kind = FILL_TARGET_NODE;
            target.id = returned;
        }
        scope_add(&scope, target);
    }
    while (scope.head < scope.tail) {
        FillTarget now = scope.queue[scope.head++];
        FillTarget *dependents = NULL;
        size_t dependent_count = 0;
        if (plan_dependents(plan, now, &dependents, &dependent_count) != PLAN_OK) {
            scope_free(&scope);
            return REGENERATE_ERR_ALLOC;
        }
        for (size_t i = 0; i < dependent_count; i++) {
            scope_add(&scope, dependents[i]);
        }
        free(dependents);
        if (now.kind == FILL_TARGET_LANE_SPAN) {
            add_run(&scope, (LaneSpanId)now.id);
        }
        /* What a section holds is what its returns are made from. */
        if (!(now.kind == FILL_TARGET_NODE && plan_node_kind(plan, (NodeId)now.id) == PLAN_NODE_FORM)) {
            add_returners(&scope, plan_section_of(plan, owner_node(plan, now)));
        }
    }

    /* The sections reopened whole, then the spans in no such section. */
    size_t capacity = plan_node_count(plan) + plan_lane_span_count(plan) + 1;
    FillTarget *list = calloc(capacity, sizeof(FillTarget));
    bool *whole = calloc(plan_node_count(plan) + 1, sizeof(bool));
    if (!list || !whole) {
        free(list);
        free(whole);
        scope_free(&scope);
        return REGENERATE_ERR_ALLOC;
    }
    size_t found = 0;
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        if (scope.node_marked[id] && is_theme_section(plan, id) && !in_material(plan, id)) {
            list[found++] = (FillTarget){FILL_TARGET_NODE, id};
            whole[id] = true;
        }
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        NodeId section = plan_section_of(plan, plan_lane_span_owner(plan, id));
        if (scope.span_marked[id] && span_is_reopened(plan, id) && !(section != PLAN_NODE_NONE && whole[section]) &&
            !in_material(plan, plan_lane_span_owner(plan, id))) {
            list[found++] = (FillTarget){FILL_TARGET_LANE_SPAN, id};
        }
    }
    free(whole);
    scope_free(&scope);
    if (found == 0) {
        free(list);
        return REGENERATE_ERR_BAD_TARGET;
    }
    *out = list;
    *count = found;
    return REGENERATE_OK;
}

RegenerateResult plan_regenerate_scope_all(const PiecePlan *plan, const FillTarget *targets, size_t target_count,
                                           FillTarget **out, size_t *count) {
    return scope_build(plan, targets, target_count, out, count);
}

RegenerateResult plan_regenerate_scope(const PiecePlan *plan, FillTarget target, FillTarget **out, size_t *count) {
    return scope_build(plan, &target, 1, out, count);
}

/* ------------------------------------------------------------- reopening */

/* Whether the section has Given material among its nodes or the spans they own. */
static bool subtree_has_given(const PiecePlan *plan, NodeId id) {
    for (size_t i = 0; i < plan_node_child_count(plan, id); i++) {
        NodeId child = plan_node_child_at(plan, id, i);
        if (plan_node_state(plan, child) == PLAN_STATE_GIVEN || subtree_has_given(plan, child)) {
            return true;
        }
        for (LaneSpanId span = 1; span <= plan_lane_span_count(plan); span++) {
            if (plan_has_lane_span(plan, span) && plan_lane_span_owner(plan, span) == child &&
                plan_lane_span_state(plan, span) == PLAN_STATE_GIVEN) {
                return true;
            }
        }
    }
    return false;
}

/* Whether any constraint of a live node or lane span names the motive. */
static bool motive_referenced(const PiecePlan *plan, NodeId motive) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        for (size_t i = 0; plan_has_node(plan, id) && i < plan_node_constraint_count(plan, id); i++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, id, i);
            if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF && constraint->params.motive.motive == motive) {
                return true;
            }
        }
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(plan); id++) {
        for (size_t i = 0; plan_has_lane_span(plan, id) && i < plan_lane_span_constraint_count(plan, id); i++) {
            const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, id, i);
            if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF && constraint->params.motive.motive == motive) {
                return true;
            }
        }
    }
    return false;
}

static bool is_candidate(const NodeId *list, size_t count, NodeId id) {
    for (size_t i = 0; i < count; i++) {
        if (list[i] == id) {
            return true;
        }
    }
    return false;
}

/* Adds the motives the spans owned under id name, and the motives those are made from, to list (room for every node). */
static size_t collect_motives(const PiecePlan *plan, NodeId id, NodeId *list, size_t count) {
    size_t before = count;
    for (LaneSpanId span = 1; span <= plan_lane_span_count(plan); span++) {
        if (!plan_has_lane_span(plan, span)) {
            continue;
        }
        NodeId owner = plan_lane_span_owner(plan, span);
        bool under = false;
        for (NodeId up = owner; up != PLAN_NODE_NONE && !under; up = plan_node_parent(plan, up)) {
            under = up == id;
        }
        for (size_t i = 0; under && i < plan_lane_span_constraint_count(plan, span); i++) {
            const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, span, i);
            if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF && !is_candidate(list, count, constraint->params.motive.motive)) {
                list[count++] = constraint->params.motive.motive;
            }
        }
    }
    /* What the motives are made from. */
    for (size_t c = before; c < count; c++) {
        for (size_t i = 0; i < plan_node_constraint_count(plan, list[c]); i++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, list[c], i);
            if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF && !is_candidate(list, count, constraint->params.motive.motive)) {
                list[count++] = constraint->params.motive.motive;
            }
        }
    }
    return count;
}

/* Removes the candidate motives nothing names any more, a motive made from another first. m0 and Given motives stay. */
static void remove_unused_motives(PiecePlan *plan, const NodeId *list, size_t count) {
    bool again = true;
    while (again) {
        again = false;
        for (size_t c = 0; c < count; c++) {
            const char *name = plan_node_name(plan, list[c]);
            if (!plan_has_node(plan, list[c]) || plan_node_kind(plan, list[c]) != PLAN_NODE_MOTIVE ||
                plan_node_state(plan, list[c]) == PLAN_STATE_GIVEN || (name && strcmp(name, GROW_SOURCE_NAME) == 0) ||
                motive_referenced(plan, list[c])) {
                continue;
            }
            if (plan_node_remove_leaf(plan, list[c]) == PLAN_OK) {
                again = true;
            }
        }
    }
}

/* Reopens a section whole. */
static RegenerateResult reopen_section(PiecePlan *plan, NodeId section) {
    NodeId *motives = calloc(plan_node_count(plan) + 1, sizeof(NodeId));
    if (!motives) {
        return REGENERATE_ERR_ALLOC;
    }
    size_t motive_count = collect_motives(plan, section, motives, 0);
    PlanResult result = plan_remove_subtree(plan, section);
    if (result == PLAN_OK) {
        result = plan_node_set_state(plan, section, PLAN_STATE_CONSTRAINED);
    }
    if (result == PLAN_OK) {
        remove_unused_motives(plan, motives, motive_count);
    }
    free(motives);
    return result == PLAN_OK ? REGENERATE_OK : result == PLAN_ERR_ALLOC ? REGENERATE_ERR_ALLOC : REGENERATE_ERR_LOCKED;
}

/* The note, chord or rest an open span holds until it is written. */
static void placeholder(PlanLaneKind lane, PlanSpan span, PlanNote *note, PlanLanePayload *payload) {
    memset(payload, 0, sizeof(*payload));
    if (lane == PLAN_LANE_HARMONY) {
        strcpy(payload->harmony.roman_numeral, "I");
        payload->harmony.function = PLAN_FUNCTION_UNKNOWN;
        return;
    }
    note->midi_pitch = lane == PLAN_LANE_BASS ? BASS_PLACEHOLDER : lane == PLAN_LANE_MELODY ? MELODY_PLACEHOLDER : INNER_PLACEHOLDER;
    note->offset = (PlanTime){0, 1};
    note->duration = span.length;
    payload->notes.notes = note;
    payload->notes.note_count = 1;
}

static bool span_earlier(const PiecePlan *plan, LaneSpanId a, LaneSpanId b) {
    return plan_time_compare(plan_lane_span_span(plan, a).start, plan_lane_span_span(plan, b).start) < 0;
}

/*
 * One run of harmony or bass spans an operator wrote and split, as one open span again over the same time, with the
 * constraints grow gave it: the first span's, and the last span's cadence.
 */
static PlanResult reopen_run(PiecePlan *plan, const LaneSpanId *run, size_t count) {
    PlanLaneKind lane = plan_lane_span_lane(plan, run[0]);
    NodeId owner = plan_lane_span_owner(plan, run[0]);
    PlanSpan first = plan_lane_span_span(plan, run[0]);
    PlanSpan last = plan_lane_span_span(plan, run[count - 1]);
    PlanTime end;
    PlanSpan whole = first;
    PlanResult result = plan_span_end(last, &end);
    PlanConstraint *kept = NULL;
    size_t kept_count = 0;
    size_t first_count = plan_lane_span_constraint_count(plan, run[0]);
    size_t last_count = plan_lane_span_constraint_count(plan, run[count - 1]);
    if (result != PLAN_OK) {
        return result;
    }
    /* length = end - start */
    result = plan_time_make(end.num * (long)first.start.den - first.start.num * (long)end.den, end.den * first.start.den,
                            &whole.length);
    kept = calloc(first_count + last_count + 1, sizeof(PlanConstraint));
    if (!kept) {
        return PLAN_ERR_ALLOC;
    }
    for (size_t i = 0; i < first_count; i++) {
        kept[kept_count++] = *plan_lane_span_constraint_at(plan, run[0], i);
    }
    for (size_t i = 0; count > 1 && i < last_count; i++) {
        const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, run[count - 1], i);
        if (constraint->kind == PLAN_CONSTRAINT_CADENCE) {
            kept[kept_count++] = *constraint;
        }
    }
    for (size_t i = 0; result == PLAN_OK && i < count; i++) {
        result = plan_lane_span_remove(plan, run[i]);
    }
    if (result == PLAN_OK) {
        PlanNote note;
        PlanLanePayload payload;
        LaneSpanId id;
        placeholder(lane, whole, &note, &payload);
        result = plan_add_lane_span(plan, owner, lane, 0, whole, &payload, &id);
        for (size_t i = 0; result == PLAN_OK && i < kept_count; i++) {
            result = plan_lane_span_add_constraint(plan, id, &kept[i]);
        }
    }
    free(kept);
    return result;
}

/* A written melody or inner voice span, open again with a placeholder note. */
static PlanResult reopen_in_place(PiecePlan *plan, LaneSpanId id) {
    PlanNote note;
    PlanLanePayload payload;
    placeholder(plan_lane_span_lane(plan, id), plan_lane_span_span(plan, id), &note, &payload);
    PlanResult result = plan_lane_span_write(plan, id, &payload);
    return result == PLAN_OK ? plan_lane_span_set_state(plan, id, PLAN_STATE_CONSTRAINED) : result;
}

/* The harmony and bass spans among ids, grouped into runs (same owner and lane, end to start), each run opened once. */
static PlanResult reopen_spans(PiecePlan *plan, const LaneSpanId *ids, size_t count) {
    LaneSpanId *sorted = calloc(count + 1, sizeof(LaneSpanId));
    LaneSpanId *run = calloc(count + 1, sizeof(LaneSpanId));
    if (!sorted || !run) {
        free(sorted);
        free(run);
        return PLAN_ERR_ALLOC;
    }
    size_t sorted_count = 0;
    PlanResult result = PLAN_OK;
    for (size_t i = 0; result == PLAN_OK && i < count; i++) {
        PlanLaneKind lane = plan_lane_span_lane(plan, ids[i]);
        if (lane == PLAN_LANE_MELODY || lane == PLAN_LANE_INNER_VOICE) {
            result = reopen_in_place(plan, ids[i]);
        } else {
            size_t at = sorted_count++;
            /* By owner, lane, then start. */
            while (at > 0 && (plan_lane_span_owner(plan, sorted[at - 1]) > plan_lane_span_owner(plan, ids[i]) ||
                              (plan_lane_span_owner(plan, sorted[at - 1]) == plan_lane_span_owner(plan, ids[i]) &&
                               (plan_lane_span_lane(plan, sorted[at - 1]) > lane ||
                                (plan_lane_span_lane(plan, sorted[at - 1]) == lane && span_earlier(plan, ids[i], sorted[at - 1])))))) {
                sorted[at] = sorted[at - 1];
                at--;
            }
            sorted[at] = ids[i];
        }
    }
    size_t run_count = 0;
    for (size_t i = 0; result == PLAN_OK && i < sorted_count; i++) {
        bool joins = false;
        if (run_count > 0) {
            PlanTime end;
            plan_span_end(plan_lane_span_span(plan, run[run_count - 1]), &end);
            joins = plan_lane_span_owner(plan, run[run_count - 1]) == plan_lane_span_owner(plan, sorted[i]) &&
                    plan_lane_span_lane(plan, run[run_count - 1]) == plan_lane_span_lane(plan, sorted[i]) &&
                    plan_time_compare(end, plan_lane_span_span(plan, sorted[i]).start) == 0;
        }
        if (!joins && run_count > 0) {
            result = reopen_run(plan, run, run_count);
            run_count = 0;
        }
        run[run_count++] = sorted[i];
    }
    if (result == PLAN_OK && run_count > 0) {
        result = reopen_run(plan, run, run_count);
    }
    free(sorted);
    free(run);
    return result;
}

/* ------------------------------------------------------------- regenerating */

RegenerateResult plan_regenerate(PiecePlan *plan, FillTarget target, const ComposeConfig *config, ComposeOutcome *outcome) {
    return plan_regenerate_all(plan, &target, 1, config, outcome);
}

RegenerateResult plan_regenerate_all(PiecePlan *plan, const FillTarget *targets, size_t target_count,
                                     const ComposeConfig *config, ComposeOutcome *outcome) {
    if (!plan || !config || !outcome) {
        return REGENERATE_ERR_NULL;
    }
    FillTarget *scope;
    size_t count;
    RegenerateResult scoped = plan_regenerate_scope_all(plan, targets, target_count, &scope, &count);
    if (scoped != REGENERATE_OK) {
        return scoped;
    }

    /* Nothing is changed unless every part can be reopened. */
    RegenerateResult result = REGENERATE_OK;
    for (size_t i = 0; result == REGENERATE_OK && i < count; i++) {
        if (scope[i].kind == FILL_TARGET_NODE) {
            result = subtree_has_given(plan, (NodeId)scope[i].id) ? REGENERATE_ERR_LOCKED : REGENERATE_OK;
        } else if (plan_lane_span_lane(plan, (LaneSpanId)scope[i].id) == PLAN_LANE_HARMONY) {
            /* A harmony span is built again from the shape grow gave it. */
            LaneSpanId id = (LaneSpanId)scope[i].id;
            bool shaped = plan_lane_span_find_constraint(plan, id, PLAN_CONSTRAINT_HARMONY_SHAPE) != NULL;
            for (size_t j = 0; !shaped && j < count; j++) {
                shaped = scope[j].kind == FILL_TARGET_LANE_SPAN && plan_lane_span_lane(plan, (LaneSpanId)scope[j].id) == PLAN_LANE_HARMONY &&
                         plan_lane_span_owner(plan, (LaneSpanId)scope[j].id) == plan_lane_span_owner(plan, id) &&
                         plan_lane_span_find_constraint(plan, (LaneSpanId)scope[j].id, PLAN_CONSTRAINT_HARMONY_SHAPE) != NULL;
            }
            result = shaped ? REGENERATE_OK : REGENERATE_ERR_BAD_TARGET;
        }
    }
    LaneSpanId *spans = calloc(count + 1, sizeof(LaneSpanId));
    size_t span_count = 0;
    if (!spans) {
        result = REGENERATE_ERR_ALLOC;
    }
    for (size_t i = 0; result == REGENERATE_OK && i < count; i++) {
        if (scope[i].kind == FILL_TARGET_NODE) {
            result = reopen_section(plan, (NodeId)scope[i].id);
        } else {
            spans[span_count++] = (LaneSpanId)scope[i].id;
        }
    }
    if (result == REGENERATE_OK && span_count > 0) {
        PlanResult reopened = reopen_spans(plan, spans, span_count);
        result = reopened == PLAN_OK ? REGENERATE_OK : reopened == PLAN_ERR_ALLOC ? REGENERATE_ERR_ALLOC : REGENERATE_ERR_BAD_TARGET;
    }
    free(spans);
    free(scope);
    if (result != REGENERATE_OK) {
        return result;
    }

    PieceResult composed = compose_run(plan, config, outcome);
    switch (composed) {
    case PIECE_OK:
        return REGENERATE_OK;
    case PIECE_ERR_STUCK:
        return REGENERATE_ERR_STUCK;
    case PIECE_ERR_ALLOC:
        return REGENERATE_ERR_ALLOC;
    default:
        return REGENERATE_ERR_BAD_TARGET;
    }
}
