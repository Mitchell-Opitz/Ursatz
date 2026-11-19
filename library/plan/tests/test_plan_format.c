#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plan/plan.h"
#include "plan/plan_constraint.h"
#include "plan/plan_edit.h"
#include "plan/plan_json.h"
#include "plan/plan_lanes.h"
#include "plan/plan_state.h"
#include "test_knowledge.h"

#ifndef PLAN_FIXTURE_DIR
#error "PLAN_FIXTURE_DIR is not defined"
#endif

/*
 * Plan format 6 (R-626): three plans of format 5 (kept as fixtures) read and write format 6 with the same nodes, spans, payloads and
 * constraints; regions are written with their kind and read back; a region of a kind that is not built is refused with its message;
 * a derivation round-trips; the metadata members come from the stamp the caller gives.
 */

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    assert(text != NULL && fread(text, 1, (size_t)size, file) == (size_t)size);
    text[size] = '\0';
    fclose(file);
    return text;
}

static PlanProducer stamp(void) {
    PlanProducer producer;
    memset(&producer, 0, sizeof(producer));
    snprintf(producer.name, sizeof(producer.name), "%s", "format-test");
    snprintf(producer.version, sizeof(producer.version), "%s", "1.2.3");
    snprintf(producer.producer_key, sizeof(producer.producer_key), "%s", "0123456789abcdef");
    snprintf(producer.composition_key, sizeof(producer.composition_key), "%s", "fedcba9876543210");
    snprintf(producer.knowledge_hash, sizeof(producer.knowledge_hash), "%s", "00112233aabbccdd");
    return producer;
}

static char *write_plan(const Plan *plan) {
    PlanProducer producer = stamp();
    PlanJsonError error;
    char *text = plan_to_json(plan, &producer, &error);
    if (!text) {
        fprintf(stderr, "cannot write the plan: %s\n", error.message);
    }
    assert(text != NULL);
    return text;
}

static Plan *read_plan(const char *text) {
    Plan *plan = NULL;
    PlanJsonError error;
    if (plan_from_json(test_knowledge(), text, NULL, &plan, &error) != PLAN_OK) {
        fprintf(stderr, "cannot read the plan: %s at %s\n", error.message, error.path);
        assert(false);
    }
    return plan;
}

static bool same_span(PlanSpan a, PlanSpan b) {
    return rational_cmp(a.start, b.start) == 0 && rational_cmp(a.length, b.length) == 0;
}

static bool same_constraints(size_t count, const PlanConstraint *(*at_a)(const Plan *, size_t, size_t), const Plan *a, const Plan *b, size_t id) {
    for (size_t i = 0; i < count; i++) {
        const PlanConstraint *left = at_a(a, id, i);
        const PlanConstraint *right = at_a(b, id, i);
        if (!left || !right || left->kind != right->kind) {
            return false;
        }
        if (left->kind == PLAN_CONSTRAINT_REGION && !spelled_key_equals(&left->params.region, &right->params.region)) {
            return false;
        }
    }
    return true;
}

static const PlanConstraint *node_constraint_at(const Plan *plan, size_t id, size_t index) {
    return plan_node_constraint_at(plan, (NodeId)id, index);
}

static const PlanConstraint *span_constraint_at(const Plan *plan, size_t id, size_t index) {
    return plan_lane_span_constraint_at(plan, (LaneSpanId)id, index);
}

/* The same nodes, lane spans, payloads and constraints, member by member. */
static void assert_same_plan(const Plan *a, const Plan *b) {
    assert(plan_node_count(a) == plan_node_count(b) && plan_lane_span_count(a) == plan_lane_span_count(b));
    assert(plan_next_note_id(a) == plan_next_note_id(b));
    assert(strcmp(plan_analyzer_version(a), plan_analyzer_version(b)) == 0);
    for (NodeId id = 1; id <= plan_node_count(a); id++) {
        assert(plan_has_node(a, id) && plan_has_node(b, id));
        assert(plan_node_kind(a, id) == plan_node_kind(b, id) && plan_node_parent(a, id) == plan_node_parent(b, id));
        assert(same_span(plan_node_span(a, id), plan_node_span(b, id)) && plan_node_state(a, id) == plan_node_state(b, id));
        const char *left = plan_node_name(a, id);
        const char *right = plan_node_name(b, id);
        assert((left == NULL) == (right == NULL) && (left == NULL || strcmp(left, right) == 0));
        assert(plan_node_child_count(a, id) == plan_node_child_count(b, id));
        size_t constraints = plan_node_constraint_count(a, id);
        assert(constraints == plan_node_constraint_count(b, id) && same_constraints(constraints, node_constraint_at, a, b, id));
    }
    for (LaneSpanId id = 1; id <= plan_lane_span_count(a); id++) {
        assert(plan_lane_span_lane(a, id) == plan_lane_span_lane(b, id) && plan_lane_span_voice(a, id) == plan_lane_span_voice(b, id));
        assert(plan_lane_span_owner(a, id) == plan_lane_span_owner(b, id) && plan_lane_span_state(a, id) == plan_lane_span_state(b, id));
        assert(same_span(plan_lane_span_span(a, id), plan_lane_span_span(b, id)));
        size_t constraints = plan_lane_span_constraint_count(a, id);
        assert(constraints == plan_lane_span_constraint_count(b, id) && same_constraints(constraints, span_constraint_at, a, b, id));
        const PlanLanePayload *left = plan_lane_span_payload(a, id);
        const PlanLanePayload *right = plan_lane_span_payload(b, id);
        switch (plan_lane_span_lane(a, id)) {
        case PLAN_LANE_METER:
            assert(left->meter.beats_per_bar == right->meter.beats_per_bar && left->meter.beat_unit == right->meter.beat_unit);
            break;
        case PLAN_LANE_REGION: {
            assert(spelled_key_equals(&left->region, &right->region));
            const PlanKeyRelation *named_left = plan_lane_span_key_relation(a, id);
            const PlanKeyRelation *named_right = plan_lane_span_key_relation(b, id);
            assert((named_left == NULL) == (named_right == NULL));
            assert(!named_left || (strcmp(named_left->relation, named_right->relation) == 0 && named_left->resolved == named_right->resolved));
            break;
        }
        case PLAN_LANE_HARMONY:
            assert(strcmp(left->harmony.roman_numeral, right->harmony.roman_numeral) == 0 && left->harmony.has_function == right->harmony.has_function);
            break;
        case PLAN_LANE_BASS:
        case PLAN_LANE_MELODY:
        case PLAN_LANE_INNER_VOICE:
            assert(left->notes.note_count == right->notes.note_count);
            for (size_t n = 0; n < left->notes.note_count; n++) {
                const PlanNote *x = &left->notes.notes[n];
                const PlanNote *y = &right->notes.notes[n];
                assert(x->midi == y->midi && x->id == y->id && x->source == y->source && x->operation == y->operation);
                assert(rational_cmp(x->offset, y->offset) == 0 && rational_cmp(x->duration, y->duration) == 0);
            }
            break;
        }
    }
}

static void test_format_five_fixtures(void) {
    static const char *const NAMES[] = {"grow-period", "piece-loop-and-ending", "fill-given"};
    for (size_t i = 0; i < sizeof(NAMES) / sizeof(NAMES[0]); i++) {
        char path[1024];
        snprintf(path, sizeof(path), PLAN_FIXTURE_DIR "/plan-format5-%s.json", NAMES[i]);
        char *five = read_file(path);
        assert(strstr(five, "\"format\": \"ursatz-composer-plan\"") != NULL && strstr(five, "\"format_version\": 5") != NULL);
        assert(strstr(five, "\"keys\": [") != NULL && strstr(five, "\"framework\"") != NULL);
        Plan *plan = read_plan(five);
        char *six = write_plan(plan);
        /* Format 6: the new name, the regions, the region constraint and the lane spans without their claim members. */
        assert(strstr(six, "\"format\": \"ursatz-plan\"") != NULL && strstr(six, "\"format_version\": 6") != NULL);
        assert(strstr(six, "\"regions\": [") != NULL && strstr(six, "\"keys\": [") == NULL);
        assert(strstr(six, "\"kind\": \"tonal\"") != NULL && strstr(six, "\"kind\": \"key\"") == NULL);
        assert(strstr(six, "\"framework\"") == NULL && strstr(six, "\"note_ids\"") == NULL && strstr(six, "\"uncertainty\"") == NULL);
        assert(strstr(six, "\"method\"") == NULL && strstr(six, "\"key_label\"") == NULL);
        assert(strstr(six, "\"composer_version\"") == NULL && strstr(six, "\"ursatz_commit\"") == NULL);
        /* It reads back as the same plan, and writes the same text: the clone too. */
        Plan *again = read_plan(six);
        assert_same_plan(plan, again);
        Plan *clone = NULL;
        assert(plan_clone(again, &clone) == PLAN_OK);
        assert_same_plan(plan, clone);
        char *cloned = write_plan(clone);
        char *rewritten = write_plan(again);
        assert(strcmp(six, rewritten) == 0 && strcmp(six, cloned) == 0);
        free(cloned);
        free(rewritten);
        plan_destroy(clone);
        plan_destroy(again);
        plan_destroy(plan);
        free(six);
        free(five);
    }
}

static Rational ratio(long n, long d) {
    return rational_ratio(n, d);
}

static PlanSpan span_of(long start, long length) {
    PlanSpan span = {ratio(start, 1), ratio(length, 1)};
    return span;
}

/* A form of 8 wholes with two regions: C major, then the dominant named by its relation and resolved to G major. */
static Plan *region_plan(LaneSpanId *first, LaneSpanId *second) {
    Plan *plan = plan_create();
    NodeId form;
    assert(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, span_of(0, 8), "form", &form) == PLAN_OK);
    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.region = plan_key_of_pitch_class(0, test_materials()->major);
    assert(plan_add_lane_span(plan, form, PLAN_LANE_REGION, 0, span_of(0, 4), &payload, first) == PLAN_OK);
    payload.region = plan_key_of_pitch_class(7, test_materials()->major);
    assert(plan_add_lane_span(plan, form, PLAN_LANE_REGION, 0, span_of(4, 4), &payload, second) == PLAN_OK);
    assert(plan_lane_span_set_key_relation(plan, *second, "omt.relation.dominant", PLAN_KEY_ANCHOR_HOME, NULL, true) == PLAN_OK);
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_REGION;
    constraint.params.region = plan_key_of_pitch_class(2, test_materials()->natural_minor);
    assert(plan_node_add_constraint(plan, form, &constraint) == PLAN_OK);
    return plan;
}

static void test_regions(void) {
    LaneSpanId first;
    LaneSpanId second;
    Plan *plan = region_plan(&first, &second);
    char *text = write_plan(plan);
    assert(strstr(text, "\"regions\": [") != NULL && strstr(text, "\"kind\": \"tonal\"") != NULL);
    assert(strstr(text, "\"tonic\": \"G\"") != NULL && strstr(text, "\"relation\": \"omt.relation.dominant\"") != NULL);
    assert(strstr(text, "\"kind\": \"region\"") != NULL && strstr(text, "\"tonic\": \"D\"") != NULL);
    Plan *read = read_plan(text);
    assert_same_plan(plan, read);
    assert(spelled_key_tonic_pitch_class(&plan_lane_span_payload(read, second)->region) == 7);
    assert(plan_lane_span_key_relation(read, second) != NULL && plan_lane_span_key_relation(read, second)->resolved);
    assert(plan_lane_span_key_relation(read, first) == NULL);
    plan_destroy(read);

    /* A region named only by its relation is written with no kind and reads back unresolved. */
    assert(plan_lane_span_set_key_relation(plan, second, "omt.relation.dominant", PLAN_KEY_ANCHOR_HOME, NULL, false) == PLAN_OK);
    char *named = write_plan(plan);
    const char *unresolved = strstr(named, "\"relation\": \"omt.relation.dominant\"");
    assert(unresolved != NULL);
    read = read_plan(named);
    assert(plan_lane_span_key_relation(read, second) != NULL && !plan_lane_span_key_relation(read, second)->resolved);
    plan_destroy(read);
    free(named);
    free(text);
    plan_destroy(plan);
}

static void test_other_region_kind_is_refused(void) {
    LaneSpanId first;
    LaneSpanId second;
    Plan *plan = region_plan(&first, &second);
    char *text = write_plan(plan);
    plan_destroy(plan);
    const char *at = strstr(text, "\"kind\": \"tonal\"");
    assert(at != NULL);
    size_t head = (size_t)(at - text);
    char *field = malloc(strlen(text) + 32);
    assert(field != NULL);
    snprintf(field, strlen(text) + 32, "%.*s\"kind\": \"pitch-field\"%s", (int)head, text, at + strlen("\"kind\": \"tonal\""));
    Plan *refused = (Plan *)1;
    PlanJsonError error;
    assert(plan_from_json(test_knowledge(), field, NULL, &refused, &error) == PLAN_ERR_FORMAT && refused == NULL);
    assert(strcmp(error.message, "region kind 'pitch-field' is not built until P69") == 0);
    assert(strstr(error.path, "payload.kind") != NULL && strstr(error.path, "regions[0]") != NULL);
    /* A format 5 file knows no kinds: a "kind" member of a key is left alone. */
    free(field);
    free(text);
}

static void test_derivation(void) {
    LaneSpanId first;
    LaneSpanId second;
    Plan *plan = region_plan(&first, &second);
    assert(plan_derived_from(plan) == NULL);
    char *without = write_plan(plan);
    assert(strstr(without, "derived_from") == NULL);
    free(without);

    PlanDerivation derivation;
    memset(&derivation, 0, sizeof(derivation));
    snprintf(derivation.reading, sizeof(derivation.reading), "%s", "readings/michelle.json");
    snprintf(derivation.realization, sizeof(derivation.realization), "%s", "a1b2c3d4e5f60789");
    assert(plan_set_derived_from(plan, &derivation) == PLAN_OK);
    char *text = write_plan(plan);
    assert(strstr(text, "\"derived_from\": {") != NULL && strstr(text, "\"reading\": \"readings/michelle.json\"") != NULL);
    assert(strstr(text, "\"realization\": \"a1b2c3d4e5f60789\"") != NULL);
    Plan *read = read_plan(text);
    assert(plan_derived_from(read) != NULL && strcmp(plan_derived_from(read)->reading, derivation.reading) == 0);
    assert(strcmp(plan_derived_from(read)->realization, derivation.realization) == 0);
    Plan *clone = NULL;
    assert(plan_clone(read, &clone) == PLAN_OK && plan_derived_from(clone) != NULL);
    assert(strcmp(plan_derived_from(clone)->reading, derivation.reading) == 0);
    char *cloned = write_plan(clone);
    assert(strcmp(text, cloned) == 0);
    free(cloned);
    plan_destroy(clone);
    plan_destroy(read);

    /* A digest that is not 16 lowercase hexadecimal digits, a reading that is empty. */
    PlanDerivation bad = derivation;
    snprintf(bad.realization, sizeof(bad.realization), "%s", "A1B2C3D4E5F60789");
    assert(plan_set_derived_from(plan, &bad) == PLAN_ERR_BAD_PAYLOAD);
    bad = derivation;
    bad.reading[0] = '\0';
    assert(plan_set_derived_from(plan, &bad) == PLAN_ERR_BAD_PAYLOAD);
    assert(plan_set_derived_from(plan, NULL) == PLAN_ERR_NULL);
    const char *digest = strstr(text, "a1b2c3d4e5f60789");
    assert(digest != NULL);
    char *wrong = malloc(strlen(text) + 1);
    assert(wrong != NULL);
    memcpy(wrong, text, strlen(text) + 1);
    wrong[(size_t)(digest - text)] = 'G';
    Plan *refused = (Plan *)1;
    PlanJsonError error;
    assert(plan_from_json(test_knowledge(), wrong, NULL, &refused, &error) == PLAN_ERR_FORMAT && refused == NULL);
    assert(strcmp(error.path, "derived_from") == 0);
    free(wrong);
    free(text);
    plan_destroy(plan);
}

static void test_metadata(void) {
    LaneSpanId first;
    LaneSpanId second;
    Plan *plan = region_plan(&first, &second);
    char *text = write_plan(plan);
    assert(strstr(text, "\"producer\": \"format-test\"") != NULL && strstr(text, "\"producer_version\": \"1.2.3\"") != NULL);
    assert(strstr(text, "\"producer_key\": \"0123456789abcdef\"") != NULL && strstr(text, "\"composition_key\": \"fedcba9876543210\"") != NULL);
    assert(strstr(text, "\"knowledge_hash\": \"00112233aabbccdd\"") != NULL);
    assert(strstr(text, "\"analyzer_version\"") == NULL);
    /* The Analyzer's version is kept when the plan has one; the stamp is read and not kept. */
    assert(plan_set_analyzer_version(plan, "9.8.7") == PLAN_OK);
    free(text);
    text = write_plan(plan);
    assert(strstr(text, "\"analyzer_version\": \"9.8.7\"") != NULL);
    Plan *read = read_plan(text);
    assert(strcmp(plan_analyzer_version(read), "9.8.7") == 0);
    plan_destroy(read);
    /* No stamp, no plan file. */
    PlanJsonError error;
    assert(plan_to_json(plan, NULL, &error) == NULL);
    free(text);
    plan_destroy(plan);
}

int main(void) {
    test_format_five_fixtures();
    test_regions();
    test_other_region_kind_is_refused();
    test_derivation();
    test_metadata();
    printf("test_plan_format: ok\n");
    return 0;
}
