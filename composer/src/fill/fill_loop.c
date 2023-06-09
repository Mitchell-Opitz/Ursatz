#include <ursatz_composer/fill_loop.h>

#include <stdlib.h>
#include <string.h>

#include "fill/fill_internal.h"

/* ------------------------------------------------------------- relatives */

typedef struct {
    FillTarget *out;
    size_t capacity;
    size_t count;
} Relatives;

static void relative_add(Relatives *relatives, FillTarget target) {
    if (relatives->out && relatives->count < relatives->capacity) {
        relatives->out[relatives->count] = target;
    }
    relatives->count++;
}

static void node_relatives(const PiecePlan *plan, NodeId id, Relatives *relatives) {
    NodeId parent = plan_node_parent(plan, id);
    if (parent != PLAN_NODE_NONE) {
        FillTarget target = {FILL_TARGET_NODE, parent};
        relative_add(relatives, target);
    }
    for (size_t i = 0; i < plan_node_child_count(plan, id); i++) {
        FillTarget target = {FILL_TARGET_NODE, plan_node_child_at(plan, id, i)};
        relative_add(relatives, target);
    }
    if (parent != PLAN_NODE_NONE) {
        size_t siblings = plan_node_child_count(plan, parent);
        for (size_t i = 0; i < siblings; i++) {
            if (plan_node_child_at(plan, parent, i) != id) {
                continue;
            }
            if (i > 0) {
                FillTarget before = {FILL_TARGET_NODE, plan_node_child_at(plan, parent, i - 1)};
                relative_add(relatives, before);
            }
            if (i + 1 < siblings) {
                FillTarget after = {FILL_TARGET_NODE, plan_node_child_at(plan, parent, i + 1)};
                relative_add(relatives, after);
            }
        }
    }
}

static void span_relatives(const PiecePlan *plan, LaneSpanId id, Relatives *relatives) {
    FillTarget owner = {FILL_TARGET_NODE, plan_lane_span_owner(plan, id)};
    relative_add(relatives, owner);

    PlanLaneKind lane = plan_lane_span_lane(plan, id);
    int voice = plan_lane_span_voice(plan, id);
    PlanSpan span = plan_lane_span_span(plan, id);
    PlanTime end;
    plan_span_end(span, &end);
    LaneSpanId before = PLAN_LANE_SPAN_NONE;
    LaneSpanId after = PLAN_LANE_SPAN_NONE;
    for (LaneSpanId other = 1; other <= plan_lane_span_count(plan); other++) {
        if (other == id || plan_lane_span_lane(plan, other) != lane || plan_lane_span_voice(plan, other) != voice) {
            continue;
        }
        PlanSpan other_span = plan_lane_span_span(plan, other);
        if (plan_time_compare(other_span.start, span.start) < 0) {
            if (before == PLAN_LANE_SPAN_NONE ||
                plan_time_compare(other_span.start, plan_lane_span_span(plan, before).start) > 0) {
                before = other;
            }
        } else if (plan_time_compare(other_span.start, end) >= 0) {
            if (after == PLAN_LANE_SPAN_NONE ||
                plan_time_compare(other_span.start, plan_lane_span_span(plan, after).start) < 0) {
                after = other;
            }
        }
    }
    if (before != PLAN_LANE_SPAN_NONE) {
        FillTarget target = {FILL_TARGET_LANE_SPAN, before};
        relative_add(relatives, target);
    }
    if (after != PLAN_LANE_SPAN_NONE) {
        FillTarget target = {FILL_TARGET_LANE_SPAN, after};
        relative_add(relatives, target);
    }
}

size_t fill_relatives(const PiecePlan *plan, FillTarget target, FillTarget *out, size_t capacity) {
    Relatives relatives = {out, capacity, 0};
    if (!plan || !fill_target_exists(plan, target)) {
        return 0;
    }
    if (target.kind == FILL_TARGET_NODE) {
        node_relatives(plan, (NodeId)target.id, &relatives);
    } else {
        span_relatives(plan, (LaneSpanId)target.id, &relatives);
    }
    return relatives.count;
}

/* --------------------------------------------------------------- checker */

bool fill_default_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                          const FillCandidate *candidate, void *context, FillCheck *out) {
    (void)plan;
    (void)target;
    (void)operator_;
    (void)candidate;
    (void)context;
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;
    return true;
}

bool fill_check_scores_higher(const FillCheck *a, const FillCheck *b) {
    return (unsigned long long)a->score_numerator * b->score_denominator >
           (unsigned long long)b->score_numerator * a->score_denominator;
}

/* --------------------------------------------------------------- gathering */

void fill_entries_destroy(FillEntries *entries) {
    for (size_t i = 0; i < entries->count; i++) {
        if (entries->items[i].candidate.destroy) {
            entries->items[i].candidate.destroy(entries->items[i].candidate.payload);
        }
    }
    free(entries->items);
    memset(entries, 0, sizeof(*entries));
}

static bool operator_fits_target(const FillOperator *operator_, FillTarget target) {
    return (operator_->kind == FILL_OPERATOR_FILL) == (target.kind == FILL_TARGET_LANE_SPAN);
}

static bool entries_append(FillEntries *entries, const FillEntry *entry) {
    if (entries->count == entries->capacity) {
        size_t capacity = entries->capacity ? entries->capacity * 2 : 8;
        FillEntry *items = realloc(entries->items, capacity * sizeof(FillEntry));
        if (!items) {
            return false;
        }
        entries->items = items;
        entries->capacity = capacity;
    }
    entries->items[entries->count++] = *entry;
    return true;
}

FillResult fill_gather(const PiecePlan *plan, const FillOperatorRegistry *registry, const FillConfig *config,
                       FillTarget target, FillEntries *out) {
    memset(out, 0, sizeof(*out));
    FillCheckerFn checker = config && config->checker ? config->checker : fill_default_checker;
    void *checker_context = config ? config->checker_context : NULL;

    for (size_t i = 0; i < fill_registry_count(registry); i++) {
        const FillOperator *operator_ = fill_registry_at(registry, i);
        if (!operator_fits_target(operator_, target) || !operator_->applies(plan, target, operator_->context)) {
            continue;
        }
        out->applicable++;

        FillCandidateList list;
        memset(&list, 0, sizeof(list));
        if (!operator_->generate(plan, target, operator_->context, &list)) {
            fill_candidates_clear(&list);
            fill_entries_destroy(out);
            return FILL_ERR_OPERATOR_FAILED;
        }
        for (size_t c = 0; c < list.count; c++) {
            FillEntry entry;
            memset(&entry, 0, sizeof(entry));
            entry.operator_index = i;
            entry.candidate = list.items[c];
            if (!checker(plan, target, operator_, &entry.candidate, checker_context, &entry.check) ||
                entry.check.score_denominator == 0) {
                /* Candidates already moved into out are freed with it; the rest of the list is freed here. */
                list.items[c].destroy = entry.candidate.destroy;
                fill_candidates_clear(&list);
                fill_entries_destroy(out);
                return FILL_ERR_CHECKER_FAILED;
            }
            if (!entries_append(out, &entry)) {
                fill_candidates_clear(&list);
                fill_entries_destroy(out);
                return FILL_ERR_ALLOC;
            }
            /* The entry owns the payload now. */
            list.items[c].destroy = NULL;
        }
        fill_candidates_clear(&list);
    }
    return FILL_OK;
}

/* ---------------------------------------------------------------- applying */

FillResult fill_apply_entry(PiecePlan *plan, const FillOperatorRegistry *registry, FillTarget target,
                            const FillEntry *entry) {
    const FillOperator *operator_ = fill_registry_at(registry, entry->operator_index);
    if (operator_->apply(plan, target, &entry->candidate, operator_->context) != PLAN_OK) {
        return FILL_ERR_OPERATOR_FAILED;
    }
    if (fill_target_state(plan, target) == PLAN_STATE_CONSTRAINED) {
        PlanResult result = target.kind == FILL_TARGET_NODE
                                ? plan_node_set_state(plan, (NodeId)target.id, PLAN_STATE_REALIZED)
                                : plan_lane_span_set_state(plan, (LaneSpanId)target.id, PLAN_STATE_REALIZED);
        if (result != PLAN_OK) {
            return FILL_ERR_OPERATOR_FAILED;
        }
    }
    if (operator_->propagate) {
        operator_->propagate(plan, target, &entry->candidate, operator_->context);
    }
    return FILL_OK;
}

/* --------------------------------------------------------------------- run */

typedef enum {
    STEP_DONE,
    STEP_NO_OPERATOR,
    STEP_ALL_REJECTED
} StepKind;

static StepKind choose_entry(const FillEntries *entries, size_t *chosen) {
    if (entries->applicable == 0) {
        return STEP_NO_OPERATOR;
    }
    bool found = false;
    for (size_t i = 0; i < entries->count; i++) {
        if (entries->items[i].check.hard_violation) {
            continue;
        }
        if (!found || fill_check_scores_higher(&entries->items[i].check, &entries->items[*chosen].check)) {
            *chosen = i;
            found = true;
        }
    }
    return found ? STEP_DONE : STEP_ALL_REJECTED;
}

void fill_outcome_destroy(FillOutcome *outcome) {
    if (outcome) {
        free(outcome->stuck);
        memset(outcome, 0, sizeof(*outcome));
    }
}

typedef struct {
    FillStuck *items;
    size_t count;
    size_t capacity;
} StuckSet;

static bool stuck_add(StuckSet *set, FillTarget target, FillStuckReason reason) {
    if (set->count == set->capacity) {
        size_t capacity = set->capacity ? set->capacity * 2 : 8;
        FillStuck *items = realloc(set->items, capacity * sizeof(FillStuck));
        if (!items) {
            return false;
        }
        set->items = items;
        set->capacity = capacity;
    }
    set->items[set->count].target = target;
    set->items[set->count].reason = reason;
    set->count++;
    return true;
}

FillResult fill_run(PiecePlan *plan, const FillOperatorRegistry *registry, const FillConfig *config,
                    FillOutcome *outcome) {
    if (!outcome) {
        return FILL_ERR_NULL;
    }
    memset(outcome, 0, sizeof(*outcome));
    if (!plan || !registry) {
        outcome->status = FILL_STATUS_ERROR;
        return FILL_ERR_NULL;
    }
    size_t max_iterations = config && config->max_iterations ? config->max_iterations : FILL_DEFAULT_MAX_ITERATIONS;

    StuckSet stuck;
    memset(&stuck, 0, sizeof(stuck));
    FillTarget *excluded = NULL;
    FillResult result = FILL_OK;

    for (;;) {
        if (outcome->iterations >= max_iterations) {
            outcome->status = FILL_STATUS_MAX_ITERATIONS;
            break;
        }
        /* The set-aside targets, as the picker wants them. */
        free(excluded);
        excluded = stuck.count ? malloc(stuck.count * sizeof(FillTarget)) : NULL;
        if (stuck.count && !excluded) {
            result = FILL_ERR_ALLOC;
            break;
        }
        for (size_t i = 0; i < stuck.count; i++) {
            excluded[i] = stuck.items[i].target;
        }
        FillTarget target;
        if (!fill_pick_open_target(plan, excluded, stuck.count, &target)) {
            outcome->status = stuck.count ? FILL_STATUS_STUCK : FILL_STATUS_COMPLETE;
            break;
        }
        outcome->iterations++;

        FillEntries entries;
        result = fill_gather(plan, registry, config, target, &entries);
        if (result != FILL_OK) {
            break;
        }
        size_t chosen = 0;
        StepKind kind = choose_entry(&entries, &chosen);
        if (kind == STEP_DONE) {
            result = fill_apply_entry(plan, registry, target, &entries.items[chosen]);
        }
        fill_entries_destroy(&entries);
        if (result != FILL_OK) {
            break;
        }
        if (kind == STEP_DONE) {
            outcome->steps++;
            stuck.count = 0; /* a success can make an operator applicable to a target that was stuck */
        } else if (!stuck_add(&stuck, target,
                              kind == STEP_NO_OPERATOR ? FILL_STUCK_NO_OPERATOR : FILL_STUCK_ALL_REJECTED)) {
            result = FILL_ERR_ALLOC;
            break;
        }
    }

    free(excluded);
    if (result != FILL_OK) {
        outcome->status = FILL_STATUS_ERROR;
        free(stuck.items);
        return result;
    }
    outcome->stuck = stuck.items;
    outcome->stuck_count = stuck.count;
    return FILL_OK;
}
