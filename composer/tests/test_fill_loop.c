#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_operator.h>
#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/fill_suggest.h>
#include <ursatz_composer/fill_target.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>

/*
 * The fill loop, proven with fake operators. They are test doubles with no
 * musical meaning:
 *   split   (Refine)  splits a node with no children into 2 (or, for a form or
 *                     section, 4) children of the next kind; motives get a
 *                     melody lane span to fill
 *   close   (Grow)    closes a node whose children (or whose lane spans, for a
 *                     motive) are all closed
 *   melody  (Fill)    writes one note into a melody lane span, pitch 60, 62 or 64
 * The checker prefers splitting in 2 over 4, and rejects pitch 64.
 */

/* ------------------------------------------------------------------- plans */

static Rational fraction(long num, unsigned long den) {
    Rational time;
    assert(rational_make(num, den, &time) == RATIONAL_OK);
    return time;
}

static PlanSpan bars(long start, long length) {
    PlanSpan span = {fraction(start, 1), fraction(length, 1)};
    return span;
}

static NodeId add_node(PiecePlan *plan, NodeId parent, PlanNodeKind kind, PlanSpan span) {
    NodeId id = PLAN_NODE_NONE;
    assert(plan_add_node(plan, parent, kind, span, NULL, &id) == PLAN_OK);
    return id;
}

static void add_constraint(PiecePlan *plan, FillTarget target, int bars_value) {
    PlanConstraint constraint = {PLAN_CONSTRAINT_LENGTH_BARS, {0}};
    constraint.params.bars = bars_value;
    if (target.kind == FILL_TARGET_NODE) {
        assert(plan_node_add_constraint(plan, (NodeId)target.id, &constraint) == PLAN_OK);
    } else {
        assert(plan_lane_span_add_constraint(plan, (LaneSpanId)target.id, &constraint) == PLAN_OK);
    }
}

static LaneSpanId add_melody(PiecePlan *plan, NodeId owner, PlanSpan span, int pitch) {
    PlanNote note = {pitch, fraction(0, 1), span.length};
    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = &note;
    payload.notes.note_count = 1;
    LaneSpanId id = PLAN_LANE_SPAN_NONE;
    assert(plan_add_lane_span(plan, owner, PLAN_LANE_MELODY, 0, span, &payload, &id) == PLAN_OK);
    return id;
}

static PiecePlan *top_down_plan(void) {
    PiecePlan *plan = plan_create();
    NodeId form = add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 8));
    FillTarget target = {FILL_TARGET_NODE, form};
    add_constraint(plan, target, 8);
    return plan;
}

static char *plan_text(const PiecePlan *plan) {
    PlanJsonError error;
    char *text = plan_to_json(plan, &error);
    assert(text != NULL);
    return text;
}

/* --------------------------------------------------------- fake operators */

static bool next_kind(PlanNodeKind kind, PlanNodeKind *out) {
    if (kind == PLAN_NODE_MOTIVE) {
        return false;
    }
    *out = (PlanNodeKind)(kind + 1);
    return true;
}

static bool split_applies(const PiecePlan *plan, FillTarget target, void *context) {
    (void)context;
    PlanNodeKind unused;
    return target.kind == FILL_TARGET_NODE && plan_node_child_count(plan, (NodeId)target.id) == 0 &&
           next_kind(plan_node_kind(plan, (NodeId)target.id), &unused);
}

static void free_int(void *payload) {
    free(payload);
}

static bool add_int_candidate(FillCandidateList *out, const char *label, int value) {
    int *payload = malloc(sizeof(int));
    if (!payload) {
        return false;
    }
    *payload = value;
    return fill_candidates_add(out, label, payload, free_int);
}

static bool split_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                           FillCandidateList *out) {
    (void)rng;
    (void)context;
    PlanNodeKind kind = plan_node_kind(plan, (NodeId)target.id);
    if (!add_int_candidate(out, "split-2", 2)) {
        return false;
    }
    return (kind != PLAN_NODE_FORM && kind != PLAN_NODE_SECTION) || add_int_candidate(out, "split-4", 4);
}

static PlanResult split_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    (void)context;
    int parts = *(const int *)candidate->payload;
    NodeId parent = (NodeId)target.id;
    PlanSpan span = plan_node_span(plan, parent);
    PlanNodeKind kind;
    next_kind(plan_node_kind(plan, parent), &kind);
    Rational child_length;
    PlanResult result = rational_div_int(span.length, parts, &child_length) == RATIONAL_OK ? PLAN_OK : PLAN_ERR_BAD_TIME;
    Rational start = span.start;
    for (int i = 0; result == PLAN_OK && i < parts; i++) {
        PlanSpan child_span = {start, child_length};
        NodeId child = PLAN_NODE_NONE;
        result = plan_add_node(plan, parent, kind, child_span, NULL, &child);
        if (result == PLAN_OK && kind == PLAN_NODE_MOTIVE) {
            add_melody(plan, child, child_span, 60);
        }
        if (result == PLAN_OK) {
            result = rational_add(start, child_length, &start) == RATIONAL_OK ? PLAN_OK : PLAN_ERR_BAD_TIME;
        }
    }
    return result;
}

static void split_propagate(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    (void)candidate;
    (void)context;
    for (size_t i = 0; i < plan_node_child_count(plan, (NodeId)target.id); i++) {
        FillTarget child = {FILL_TARGET_NODE, plan_node_child_at(plan, (NodeId)target.id, i)};
        add_constraint(plan, child, 1);
    }
}

static bool close_applies(const PiecePlan *plan, FillTarget target, void *context) {
    (void)context;
    if (target.kind != FILL_TARGET_NODE) {
        return false;
    }
    NodeId id = (NodeId)target.id;
    size_t children = plan_node_child_count(plan, id);
    if (children > 0) {
        for (size_t i = 0; i < children; i++) {
            if (plan_node_state(plan, plan_node_child_at(plan, id, i)) == PLAN_STATE_CONSTRAINED) {
                return false;
            }
        }
        return true;
    }
    if (plan_node_kind(plan, id) != PLAN_NODE_MOTIVE) {
        return false;
    }
    LaneSpanId spans[8];
    size_t count = plan_node_lane_spans(plan, id, spans, 8);
    for (size_t i = 0; i < count && i < 8; i++) {
        if (plan_lane_span_state(plan, spans[i]) == PLAN_STATE_CONSTRAINED) {
            return false;
        }
    }
    return true;
}

static bool close_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                           FillCandidateList *out) {
    (void)plan;
    (void)target;
    (void)rng;
    (void)context;
    return fill_candidates_add(out, "close", NULL, NULL);
}

static PlanResult close_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    (void)candidate;
    (void)context;
    PlanConstraint archetype = {PLAN_CONSTRAINT_ARCHETYPE, {0}};
    archetype.params.archetype = PLAN_ARCHETYPE_SENTENCE;
    return plan_node_add_constraint(plan, (NodeId)target.id, &archetype);
}

static bool melody_applies(const PiecePlan *plan, FillTarget target, void *context) {
    (void)context;
    return target.kind == FILL_TARGET_LANE_SPAN && plan_lane_span_lane(plan, (LaneSpanId)target.id) == PLAN_LANE_MELODY;
}

static bool melody_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                            FillCandidateList *out) {
    (void)plan;
    (void)target;
    (void)rng;
    (void)context;
    return add_int_candidate(out, "pitch-60", 60) && add_int_candidate(out, "pitch-62", 62) &&
           add_int_candidate(out, "pitch-64", 64);
}

static PlanResult melody_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    (void)context;
    LaneSpanId id = (LaneSpanId)target.id;
    PlanNote note = {*(const int *)candidate->payload, fraction(0, 1), plan_lane_span_span(plan, id).length};
    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.notes.notes = &note;
    payload.notes.note_count = 1;
    return plan_lane_span_write(plan, id, &payload);
}

static bool failing_generate(const PiecePlan *plan, FillTarget target, FillRng *rng, void *context,
                             FillCandidateList *out) {
    (void)plan;
    (void)target;
    (void)rng;
    (void)context;
    (void)out;
    return false;
}

static bool any_applies(const PiecePlan *plan, FillTarget target, void *context) {
    (void)plan;
    (void)target;
    (void)context;
    return true;
}

static PlanResult no_apply(PiecePlan *plan, FillTarget target, const FillCandidate *candidate, void *context) {
    (void)plan;
    (void)target;
    (void)candidate;
    (void)context;
    return PLAN_OK;
}

static FillOperator make_operator(const char *name, FillOperatorKind kind) {
    FillOperator operator_;
    memset(&operator_, 0, sizeof(operator_));
    operator_.generator.name = name;
    operator_.generator.version = "1";
    operator_.generator.deterministic = true;
    operator_.kind = kind;
    return operator_;
}

static FillOperatorRegistry *fake_registry(void) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillOperator split = make_operator("split", FILL_OPERATOR_REFINE);
    split.applies = split_applies;
    split.generate = split_generate;
    split.apply = split_apply;
    split.propagate = split_propagate;
    FillOperator close = make_operator("close", FILL_OPERATOR_GROW);
    close.applies = close_applies;
    close.generate = close_generate;
    close.apply = close_apply;
    FillOperator melody = make_operator("melody", FILL_OPERATOR_FILL);
    melody.applies = melody_applies;
    melody.generate = melody_generate;
    melody.apply = melody_apply;
    assert(fill_registry_register(registry, &split) == FILL_OK);
    assert(fill_registry_register(registry, &close) == FILL_OK);
    assert(fill_registry_register(registry, &melody) == FILL_OK);
    return registry;
}

/* Prefers splitting in 2 to 4, and rejects pitch 64. */
static bool fake_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                         const FillCandidate *candidate, void *context, FillCheck *out) {
    (void)plan;
    (void)target;
    (void)operator_;
    (void)context;
    memset(out, 0, sizeof(*out));
    out->score_numerator = 0;
    out->score_denominator = 1;
    if (strcmp(candidate->label, "split-2") == 0) {
        out->score_numerator = 2;
    } else if (strcmp(candidate->label, "split-4") == 0) {
        out->score_numerator = 1;
    } else if (strcmp(candidate->label, "pitch-64") == 0) {
        out->hard_violation = true;
        snprintf(out->reason, sizeof(out->reason), "forbidden");
    }
    return true;
}

static bool reject_all_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                               const FillCandidate *candidate, void *context, FillCheck *out) {
    (void)plan;
    (void)target;
    (void)operator_;
    (void)candidate;
    (void)context;
    memset(out, 0, sizeof(*out));
    out->hard_violation = true;
    out->score_denominator = 1;
    snprintf(out->reason, sizeof(out->reason), "nothing is allowed");
    return true;
}

static FillConfig config_with(unsigned long seed) {
    FillConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.checker = fake_checker;
    return config;
}

static size_t count_open(const PiecePlan *plan) {
    size_t open = 0;
    for (size_t id = 1; id <= plan_node_count(plan); id++) {
        open += plan_node_state(plan, id) == PLAN_STATE_CONSTRAINED;
    }
    for (size_t id = 1; id <= plan_lane_span_count(plan); id++) {
        open += plan_lane_span_state(plan, id) == PLAN_STATE_CONSTRAINED;
    }
    return open;
}

/* ----------------------------------------------------------------- picking */

static void test_pick(void) {
    PiecePlan *plan = plan_create();
    NodeId form = add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 8));
    assert(plan_node_set_state(plan, form, PLAN_STATE_GIVEN) == PLAN_OK);
    NodeId a = add_node(plan, form, PLAN_NODE_SECTION, bars(0, 4));
    NodeId b = add_node(plan, form, PLAN_NODE_SECTION, bars(4, 4));
    LaneSpanId span = add_melody(plan, a, bars(0, 4), 60);
    FillTarget node_a = {FILL_TARGET_NODE, a};
    FillTarget node_b = {FILL_TARGET_NODE, b};
    FillTarget span_target = {FILL_TARGET_LANE_SPAN, span};
    add_constraint(plan, node_a, 1);
    add_constraint(plan, node_b, 1);
    add_constraint(plan, span_target, 1);

    /* All tie on constraints; A and the span start together, and a node beats a span. */
    FillTarget picked;
    assert(fill_pick_open_target(plan, NULL, 0, &picked));
    assert(fill_target_equals(picked, node_a));

    FillTarget excluded[2] = {node_a, span_target};
    assert(fill_pick_open_target(plan, excluded, 1, &picked) && fill_target_equals(picked, span_target));
    assert(fill_pick_open_target(plan, excluded, 2, &picked) && fill_target_equals(picked, node_b));

    /* More constraints beat everything. */
    add_constraint(plan, node_b, 2);
    assert(fill_pick_open_target(plan, NULL, 0, &picked) && fill_target_equals(picked, node_b));

    /* Given and Realized are not open; with nothing open, nothing is picked. */
    assert(plan_node_set_state(plan, a, PLAN_STATE_GIVEN) == PLAN_OK);
    assert(plan_node_set_state(plan, b, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, span, PLAN_STATE_REALIZED) == PLAN_OK);
    assert(!fill_pick_open_target(plan, NULL, 0, &picked));
    plan_destroy(plan);

    /* Equal everything: the lower id. */
    plan = plan_create();
    form = add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 8));
    assert(plan_node_set_state(plan, form, PLAN_STATE_GIVEN) == PLAN_OK);
    NodeId first = add_node(plan, form, PLAN_NODE_SECTION, bars(0, 4));
    add_node(plan, form, PLAN_NODE_SECTION, bars(0, 4));
    assert(fill_pick_open_target(plan, NULL, 0, &picked) && picked.id == first);
    plan_destroy(plan);
}

/* ---------------------------------------------------------------- registry */

static void test_registry_validation(void) {
    FillOperatorRegistry *registry = fill_registry_create();
    FillOperator operator_ = make_operator("one", FILL_OPERATOR_FILL);
    operator_.applies = any_applies;
    operator_.generate = failing_generate;
    operator_.apply = no_apply;
    assert(fill_registry_register(registry, &operator_) == FILL_OK);
    assert(fill_registry_register(registry, &operator_) == FILL_ERR_DUPLICATE_OPERATOR);
    FillOperator no_version = operator_;
    no_version.generator.name = "two";
    no_version.generator.version = "";
    assert(fill_registry_register(registry, &no_version) == FILL_ERR_BAD_OPERATOR);
    FillOperator no_apply_hook = operator_;
    no_apply_hook.generator.name = "three";
    no_apply_hook.apply = NULL;
    assert(fill_registry_register(registry, &no_apply_hook) == FILL_ERR_BAD_OPERATOR);
    assert(fill_registry_count(registry) == 1);
    assert(strcmp(fill_registry_at(registry, 0)->generator.name, "one") == 0);
    fill_registry_destroy(registry);
}

/* ---------------------------------------------------------- filling a plan */

static void test_top_down_fill(void) {
    PiecePlan *plan = top_down_plan();
    FillOperatorRegistry *registry = fake_registry();
    FillConfig config = config_with(42);
    FillOutcome outcome;
    FillRecord *record = NULL;
    assert(fill_run(plan, registry, &config, &outcome, &record) == FILL_OK);

    assert(outcome.status == FILL_STATUS_COMPLETE);
    assert(outcome.stuck_count == 0);
    assert(count_open(plan) == 0);

    /* 1 form, 2 sections, 4 phrases, 8 subphrases, 16 ideas, 32 motives, each motive with a melody span. */
    assert(plan_node_count(plan) == 63);
    assert(plan_lane_span_count(plan) == 32);
    /* A step per refined node, per motive and per melody span; the checker picked 2-way splits. */
    assert(outcome.steps == 31 + 32 + 32);
    assert(outcome.iterations >= outcome.steps);
    for (size_t id = 1; id <= plan_lane_span_count(plan); id++) {
        int pitch = plan_lane_span_payload(plan, id)->notes.notes[0].midi_pitch;
        assert(pitch == 60 || pitch == 62); /* 64 is rejected */
    }

    /* The record. */
    assert(record != NULL);
    assert(record->seed == 42);
    assert(generation_record_random_seed(record->generation) == 42);
    assert(record->status == FILL_STATUS_COMPLETE);
    assert(record->step_count == outcome.iterations);
    assert(record->steps[0].target.kind == FILL_TARGET_NODE && record->steps[0].target.id == 1);
    assert(record->steps[0].outcome == FILL_STEP_APPLIED);
    assert(record->steps[0].considered_count == 2);
    assert(strcmp(record->steps[0].considered[0].label, "split-2") == 0);
    assert(strcmp(record->steps[0].considered[0].operator_name, "split") == 0);
    assert(record->steps[0].considered[0].score_numerator == 2);
    assert(record->steps[0].chosen == 0);
    assert(record->steps[0].rng_state_before == 42);

    bool saw_rejection = false;
    bool saw_stuck_attempt = false;
    for (size_t i = 0; i < record->step_count; i++) {
        saw_stuck_attempt = saw_stuck_attempt || record->steps[i].outcome == FILL_STEP_NO_OPERATOR;
        for (size_t c = 0; c < record->steps[i].considered_count; c++) {
            const FillConsidered *considered = &record->steps[i].considered[c];
            if (considered->rejected) {
                assert(strcmp(considered->label, "pitch-64") == 0);
                assert(strcmp(considered->reason, "forbidden") == 0);
                saw_rejection = true;
            }
        }
        if (i > 0) {
            /* One random stream: each step starts where the one before it ended. */
            assert(record->steps[i].rng_state_before == record->steps[i - 1].rng_state_after);
        }
    }
    assert(saw_rejection);
    assert(saw_stuck_attempt); /* a motive waits for its melody span and is retried */

    char *json = fill_record_to_json(record);
    assert(json != NULL);
    assert(strstr(json, "\"random_seed\": \"42\"") != NULL);
    assert(strstr(json, "\"status\": \"complete\"") != NULL);
    assert(strstr(json, "\"rejected\": true") != NULL);
    assert(strstr(json, "\"reason\": \"forbidden\"") != NULL);
    free(json);

    fill_record_destroy(record);
    fill_outcome_destroy(&outcome);
    fill_registry_destroy(registry);
    plan_destroy(plan);
}

static char *run_to_text(unsigned long seed, char **record_text) {
    PiecePlan *plan = top_down_plan();
    FillOperatorRegistry *registry = fake_registry();
    FillConfig config = config_with(seed);
    FillOutcome outcome;
    FillRecord *record = NULL;
    assert(fill_run(plan, registry, &config, &outcome, &record) == FILL_OK);
    assert(outcome.status == FILL_STATUS_COMPLETE);
    char *text = plan_text(plan);
    *record_text = fill_record_to_json(record);
    assert(*record_text != NULL);
    fill_record_destroy(record);
    fill_outcome_destroy(&outcome);
    fill_registry_destroy(registry);
    plan_destroy(plan);
    return text;
}

static void test_reproducible_from_seed(void) {
    char *plan_a;
    char *plan_b;
    char *record_a;
    char *record_b;
    plan_a = run_to_text(42, &record_a);
    plan_b = run_to_text(42, &record_b);
    assert(strcmp(plan_a, plan_b) == 0);
    assert(strcmp(record_a, record_b) == 0);
    free(plan_a);
    free(plan_b);
    free(record_a);
    free(record_b);

    /* Different seeds can differ: the tied pitches are settled by the seed. */
    char *first_plan = NULL;
    char *first_record = NULL;
    bool plan_differs = false;
    bool record_differs = false;
    for (unsigned long seed = 1; seed <= 8; seed++) {
        char *record_text;
        char *text = run_to_text(seed, &record_text);
        if (!first_plan) {
            first_plan = text;
            first_record = record_text;
            continue;
        }
        plan_differs = plan_differs || strcmp(first_plan, text) != 0;
        record_differs = record_differs || strcmp(first_record, record_text) != 0;
        free(text);
        free(record_text);
    }
    assert(plan_differs && record_differs);
    free(first_plan);
    free(first_record);
}

/* ------------------------------------------------------------ Given, bottom-up */

static void test_given_is_never_modified(void) {
    PiecePlan *plan = plan_create();
    NodeId form = add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 8));
    FillTarget form_target = {FILL_TARGET_NODE, form};
    add_constraint(plan, form_target, 8);
    NodeId given_section = add_node(plan, form, PLAN_NODE_SECTION, bars(0, 4));
    NodeId open_section = add_node(plan, form, PLAN_NODE_SECTION, bars(4, 4));
    LaneSpanId given_melody = add_melody(plan, given_section, bars(0, 4), 72);
    assert(plan_node_set_state(plan, given_section, PLAN_STATE_GIVEN) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, given_melody, PLAN_STATE_GIVEN) == PLAN_OK);

    FillOperatorRegistry *registry = fake_registry();
    FillConfig config = config_with(7);
    FillOutcome outcome;
    assert(fill_run(plan, registry, &config, &outcome, NULL) == FILL_OK);
    assert(outcome.status == FILL_STATUS_COMPLETE);
    assert(count_open(plan) == 0);

    /* The Given section and its melody are as they were. */
    assert(plan_node_state(plan, given_section) == PLAN_STATE_GIVEN);
    assert(plan_node_child_count(plan, given_section) == 0);
    assert(plan_node_constraint_count(plan, given_section) == 0);
    assert(plan_lane_span_state(plan, given_melody) == PLAN_STATE_GIVEN);
    assert(plan_lane_span_payload(plan, given_melody)->notes.notes[0].midi_pitch == 72);
    assert(plan_lane_span_constraint_count(plan, given_melody) == 0);
    /* The open section was filled, and the form, which waited for it, was closed (bottom-up). */
    assert(plan_node_child_count(plan, open_section) == 2);
    assert(plan_node_state(plan, form) == PLAN_STATE_REALIZED);
    assert(plan_node_constraint_count(plan, form) == 2);

    fill_outcome_destroy(&outcome);
    fill_registry_destroy(registry);
    plan_destroy(plan);
}

/* ------------------------------------------------------------ ending cleanly */

static void test_unfillable_targets_end_cleanly(void) {
    /* No operator applies. */
    PiecePlan *plan = top_down_plan();
    FillOperatorRegistry *empty = fill_registry_create();
    FillOutcome outcome;
    FillRecord *record = NULL;
    FillConfig config = config_with(1);
    assert(fill_run(plan, empty, &config, &outcome, &record) == FILL_OK);
    assert(outcome.status == FILL_STATUS_STUCK);
    assert(outcome.iterations == 1 && outcome.steps == 0);
    assert(outcome.stuck_count == 1 && outcome.stuck[0].reason == FILL_STUCK_NO_OPERATOR);
    assert(outcome.stuck[0].target.id == 1);
    assert(record->step_count == 1 && record->steps[0].outcome == FILL_STEP_NO_OPERATOR && record->steps[0].chosen == -1);
    fill_record_destroy(record);
    fill_outcome_destroy(&outcome);
    fill_registry_destroy(empty);

    /* Every candidate is rejected: the target stays open and the plan is unchanged. */
    FillOperatorRegistry *registry = fake_registry();
    char *before = plan_text(plan);
    config.checker = reject_all_checker;
    assert(fill_run(plan, registry, &config, &outcome, &record) == FILL_OK);
    assert(outcome.status == FILL_STATUS_STUCK);
    assert(outcome.steps == 0 && outcome.stuck_count == 1 && outcome.stuck[0].reason == FILL_STUCK_ALL_REJECTED);
    assert(record->steps[0].outcome == FILL_STEP_ALL_REJECTED && record->steps[0].considered_count == 2);
    assert(record->steps[0].considered[0].rejected && record->steps[0].considered[1].rejected);
    char *after = plan_text(plan);
    assert(strcmp(before, after) == 0);
    assert(plan_node_state(plan, 1) == PLAN_STATE_CONSTRAINED);
    free(before);
    free(after);
    fill_record_destroy(record);
    fill_outcome_destroy(&outcome);

    /* The iteration limit stops a run that still has work. */
    config = config_with(1);
    config.max_iterations = 5;
    assert(fill_run(plan, registry, &config, &outcome, NULL) == FILL_OK);
    assert(outcome.status == FILL_STATUS_MAX_ITERATIONS);
    assert(outcome.iterations == 5);
    assert(count_open(plan) > 0);
    fill_outcome_destroy(&outcome);

    /* An operator that fails ends the run with an error. */
    FillOperatorRegistry *failing = fill_registry_create();
    FillOperator bad = make_operator("bad", FILL_OPERATOR_REFINE);
    bad.applies = any_applies;
    bad.generate = failing_generate;
    bad.apply = no_apply;
    assert(fill_registry_register(failing, &bad) == FILL_OK);
    PiecePlan *fresh = top_down_plan();
    assert(fill_run(fresh, failing, &config, &outcome, &record) == FILL_ERR_OPERATOR_FAILED);
    assert(outcome.status == FILL_STATUS_ERROR);
    assert(record != NULL && record->status == FILL_STATUS_ERROR);
    fill_record_destroy(record);
    fill_outcome_destroy(&outcome);
    fill_registry_destroy(failing);
    plan_destroy(fresh);

    fill_registry_destroy(registry);
    plan_destroy(plan);
}

/* ---------------------------------------------------------------- suggestions */

static void test_suggestions(void) {
    PiecePlan *plan = top_down_plan();
    FillOperatorRegistry *registry = fake_registry();
    FillConfig config = config_with(3);
    FillTarget root = {FILL_TARGET_NODE, 1};
    char *before = plan_text(plan);

    FillSuggestions *suggestions = NULL;
    assert(fill_suggest(plan, registry, &config, root, 5, &suggestions) == FILL_OK);
    assert(fill_suggestions_count(suggestions) == 2);
    const CandidateSet *set = fill_suggestions_set(suggestions);
    assert(candidate_set_candidate_count(set) == 2);
    assert(candidate_set_random_seed(set) == 3);
    assert(candidate_score_numerator(candidate_set_candidate_at(set, 0)) == 2);
    assert(candidate_score_denominator(candidate_set_candidate_at(set, 0)) == 1);
    assert(candidate_score_numerator(candidate_set_candidate_at(set, 1)) == 1);
    assert(strcmp(fill_suggestion_label(suggestions, 0), "split-2") == 0);
    assert(strcmp(fill_suggestion_label(suggestions, 1), "split-4") == 0);
    assert(strcmp(fill_suggestion_operator(suggestions, 0), "split") == 0);
    assert(fill_suggestion_label(suggestions, 2) == NULL);

    /* The plan is as it was. */
    char *after = plan_text(plan);
    assert(strcmp(before, after) == 0);
    free(before);
    free(after);

    /* Applying the second suggestion gives four sections. */
    assert(fill_apply_suggestion(plan, suggestions, 1) == FILL_OK);
    assert(plan_node_child_count(plan, 1) == 4);
    assert(plan_node_state(plan, 1) == PLAN_STATE_REALIZED);
    assert(fill_apply_suggestion(plan, suggestions, 1) == FILL_ERR_TARGET_NOT_OPEN);
    assert(fill_apply_suggestion(plan, suggestions, 9) == FILL_ERR_NULL);
    fill_suggestions_destroy(suggestions);

    /* Top 1, and a target that is not open. */
    PiecePlan *fresh = top_down_plan();
    assert(fill_suggest(fresh, registry, &config, root, 1, &suggestions) == FILL_OK);
    assert(fill_suggestions_count(suggestions) == 1 && candidate_set_candidate_count(fill_suggestions_set(suggestions)) == 1);
    fill_suggestions_destroy(suggestions);
    assert(plan_node_set_state(fresh, 1, PLAN_STATE_GIVEN) == PLAN_OK);
    assert(fill_suggest(fresh, registry, &config, root, 1, &suggestions) == FILL_ERR_TARGET_NOT_OPEN);
    assert(suggestions == NULL);
    plan_destroy(fresh);

    /* A rejected candidate is never suggested. */
    PiecePlan *melody_plan = plan_create();
    NodeId form = add_node(melody_plan, PLAN_NODE_NONE, PLAN_NODE_FORM, bars(0, 1));
    assert(plan_node_set_state(melody_plan, form, PLAN_STATE_GIVEN) == PLAN_OK);
    LaneSpanId span = add_melody(melody_plan, form, bars(0, 1), 60);
    FillTarget span_target = {FILL_TARGET_LANE_SPAN, span};
    assert(fill_suggest(melody_plan, registry, &config, span_target, 10, &suggestions) == FILL_OK);
    assert(fill_suggestions_count(suggestions) == 2);
    assert(strcmp(fill_suggestion_label(suggestions, 0), "pitch-60") == 0);
    assert(strcmp(fill_suggestion_label(suggestions, 1), "pitch-62") == 0);
    assert(fill_apply_suggestion(melody_plan, suggestions, 1) == FILL_OK);
    assert(plan_lane_span_state(melody_plan, span) == PLAN_STATE_REALIZED);
    assert(plan_lane_span_payload(melody_plan, span)->notes.notes[0].midi_pitch == 62);
    fill_suggestions_destroy(suggestions);
    plan_destroy(melody_plan);

    fill_registry_destroy(registry);
    plan_destroy(plan);
}

int main(void) {
    test_pick();
    test_registry_validation();
    test_top_down_fill();
    test_reproducible_from_seed();
    test_given_is_never_modified();
    test_unfillable_targets_end_cleanly();
    test_suggestions();
    return 0;
}
