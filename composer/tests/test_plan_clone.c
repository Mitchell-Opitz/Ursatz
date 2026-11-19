#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer.h>
#include <plan/plan_lanes.h>
#include <plan/plan.h>
#include <ursatz_composer/piece_plan_keys.h>
#include <plan/plan_edit.h>
#include <plan/plan_json.h>
#include <plan/plan_state.h>

#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_SNAPSHOT_DIR
#error "COMPOSER_SNAPSHOT_DIR is not defined"
#endif

/*
 * Plan edits (B2 of P62): plan_clone gives what a JSON round trip gives, for every stored plan and for a plan with a removed subtree;
 * plan_copy_into replaces a plan's contents; plan_lane_span_reopen moves a Given span to Realized under an owner that holds it; the
 * producer stamp's members appear in the text; the reader's hooks refuse a material and migrate a form record.
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

static Plan *read_plan(const char *text) {
    Plan *plan = NULL;
    PlanJsonError error;
    if (plan_from_json(test_knowledge(), text, composer_plan_read_hooks(), &plan, &error) != PLAN_OK) {
        fprintf(stderr, "cannot read plan: %s at %s\n", error.message, error.path);
        assert(false);
    }
    return plan;
}

static char *write_plan(const Plan *plan) {
    char *text = plan_to_json(plan, test_plan_producer(), NULL);
    assert(text != NULL);
    return text;
}

/* The text of a plan after a round trip through JSON: what plan_clone has to give. */
static char *round_trip_text(const Plan *plan) {
    char *first = write_plan(plan);
    Plan *again = read_plan(first);
    char *second = write_plan(again);
    plan_destroy(again);
    free(first);
    return second;
}

static bool is_stored_plan(const char *name) {
    size_t length = strlen(name);
    if (length > 10 && strcmp(name + length - 10, ".plan.json") == 0) {
        return true;
    }
    return strstr(name, ".variation-") != NULL && length > 5 && strcmp(name + length - 5, ".json") == 0;
}

static void test_clone_of_every_stored_plan(void) {
    DIR *directory = opendir(COMPOSER_SNAPSHOT_DIR);
    assert(directory != NULL);
    size_t checked = 0;
    for (struct dirent *entry = readdir(directory); entry; entry = readdir(directory)) {
        if (!is_stored_plan(entry->d_name)) {
            continue;
        }
        char path[1024];
        snprintf(path, sizeof(path), COMPOSER_SNAPSHOT_DIR "/%s", entry->d_name);
        char *text = read_file(path);
        Plan *plan = read_plan(text);
        char *expected = round_trip_text(plan);
        Plan *clone = NULL;
        assert(plan_clone(plan, &clone) == PLAN_OK && clone != NULL);
        char *cloned = write_plan(clone);
        if (strcmp(expected, cloned) != 0) {
            fprintf(stderr, "the clone of %s differs from its round trip\n", entry->d_name);
            assert(false);
        }
        assert(plan_node_count(clone) == plan_node_count(plan) && plan_lane_span_count(clone) == plan_lane_span_count(plan));
        assert(plan_next_note_id(clone) == plan_next_note_id(plan));
        free(cloned);
        free(expected);
        free(text);
        plan_destroy(clone);
        plan_destroy(plan);
        checked++;
    }
    closedir(directory);
    assert(checked >= 20);
}

static void test_clone_of_a_plan_with_a_removed_subtree(void) {
    char *text = read_file(COMPOSER_SNAPSHOT_DIR "/grow-period.plan.json");
    Plan *plan = read_plan(text);
    free(text);
    size_t nodes = plan_node_count(plan);
    bool removed = false;
    for (NodeId id = nodes; id >= 1 && !removed; id--) {
        removed = plan_node_child_count(plan, id) > 0 && plan_remove_subtree(plan, id) == PLAN_OK;
    }
    assert(removed);
    bool gap = false;
    for (NodeId id = 1; id <= nodes; id++) {
        gap = gap || !plan_has_node(plan, id);
    }
    assert(gap);

    char *expected = round_trip_text(plan);
    Plan *clone = NULL;
    assert(plan_clone(plan, &clone) == PLAN_OK);
    char *cloned = write_plan(clone);
    assert(strcmp(expected, cloned) == 0);
    /* The clone has no gaps: its ids run from 1 to the number of live nodes. */
    for (NodeId id = 1; id <= plan_node_count(clone); id++) {
        assert(plan_has_node(clone, id));
    }
    assert(plan_node_count(clone) < nodes);
    free(cloned);
    free(expected);
    plan_destroy(clone);
    plan_destroy(plan);
}

static void test_copy_into(void) {
    char *text = read_file(COMPOSER_SNAPSHOT_DIR "/piece-loop-and-ending.plan.json");
    Plan *source = read_plan(text);
    free(text);
    char *expected = round_trip_text(source);

    Plan *target = plan_create();
    assert(plan_copy_into(target, source) == PLAN_OK);
    char *copied = write_plan(target);
    assert(strcmp(expected, copied) == 0);
    free(copied);

    /* A plan that holds something else is replaced whole, and the source is not touched by what follows. */
    text = read_file(COMPOSER_SNAPSHOT_DIR "/grow-sentence.plan.json");
    Plan *other = read_plan(text);
    free(text);
    assert(plan_copy_into(other, source) == PLAN_OK);
    copied = write_plan(other);
    assert(strcmp(expected, copied) == 0);
    assert(plan_set_analyzer_version(other, "changed") == PLAN_OK);
    char *source_after = write_plan(source);
    assert(strcmp(expected, source_after) == 0);
    assert(plan_copy_into(NULL, source) == PLAN_ERR_NULL && plan_copy_into(other, NULL) == PLAN_ERR_NULL);
    Plan *none = (Plan *)1;
    assert(plan_clone(NULL, &none) == PLAN_ERR_NULL && none == NULL);
    free(source_after);
    free(copied);
    free(expected);
    plan_destroy(other);
    plan_destroy(target);
    plan_destroy(source);
}

static Rational ratio(long n, long d) {
    return rational_ratio(n, d);
}

static PlanSpan span_of(long start, long length) {
    PlanSpan span = {ratio(start, 1), ratio(length, 1)};
    return span;
}

static void test_reopen(void) {
    Plan *plan = plan_create();
    NodeId form;
    NodeId first;
    NodeId second;
    assert(plan_add_node(plan, PLAN_NODE_NONE, PLAN_NODE_FORM, span_of(0, 4), "form", &form) == PLAN_OK);
    assert(plan_add_node(plan, form, PLAN_NODE_SECTION, span_of(0, 2), "A", &first) == PLAN_OK);
    assert(plan_add_node(plan, form, PLAN_NODE_SECTION, span_of(2, 2), "B", &second) == PLAN_OK);
    PlanLanePayload payload;
    memset(&payload, 0, sizeof(payload));
    payload.harmony = (PlanHarmony){"I", HARMONIC_FUNCTION_TONIC, true};
    LaneSpanId span;
    assert(plan_add_lane_span(plan, form, PLAN_LANE_HARMONY, 0, span_of(0, 2), &payload, &span) == PLAN_OK);

    /* Only a Given span is reopened. */
    assert(plan_lane_span_reopen(plan, span, first) == PLAN_ERR_BAD_KIND);
    assert(plan_lane_span_set_state(plan, span, PLAN_STATE_GIVEN) == PLAN_OK);
    assert(plan_lane_span_set_state(plan, span, PLAN_STATE_REALIZED) == PLAN_ERR_GIVEN_LOCKED);
    /* An owner whose span does not hold the span's, an owner that is not a node and a span that is not one. */
    assert(plan_lane_span_reopen(plan, span, second) == PLAN_ERR_BAD_SPAN);
    assert(plan_lane_span_reopen(plan, span, 99) == PLAN_ERR_NOT_FOUND);
    assert(plan_lane_span_reopen(plan, 99, first) == PLAN_ERR_NOT_FOUND);
    assert(plan_lane_span_state(plan, span) == PLAN_STATE_GIVEN && plan_lane_span_owner(plan, span) == form);
    /* An owner that holds it takes it, and the span is Realized. */
    assert(plan_lane_span_reopen(plan, span, first) == PLAN_OK);
    assert(plan_lane_span_state(plan, span) == PLAN_STATE_REALIZED && plan_lane_span_owner(plan, span) == first);
    assert(plan_lane_span_reopen(NULL, span, first) == PLAN_ERR_NULL);
    plan_destroy(plan);
}

static void test_stamp(void) {
    char *text = read_file(COMPOSER_SNAPSHOT_DIR "/fill.plan.json");
    Plan *plan = read_plan(text);
    free(text);
    PlanProducer producer;
    memset(&producer, 0, sizeof(producer));
    snprintf(producer.name, sizeof(producer.name), "%s", "test-program");
    snprintf(producer.version, sizeof(producer.version), "%s", "9.8.7");
    snprintf(producer.producer_key, sizeof(producer.producer_key), "%s", "0123456789abcdef");
    snprintf(producer.composition_key, sizeof(producer.composition_key), "%s", "fedcba9876543210");
    snprintf(producer.knowledge_hash, sizeof(producer.knowledge_hash), "%s", "00112233aabbccdd");
    char *written = plan_to_json(plan, &producer, NULL);
    assert(written != NULL);
    /* Format 5 writes the version and the producer key; the other members are format 6's. */
    assert(strstr(written, "\"composer_version\": \"9.8.7\"") != NULL);
    assert(strstr(written, "\"ursatz_commit\": \"0123456789abcdef\"") != NULL);
    free(written);

    PlanJsonError error;
    assert(plan_to_json(plan, NULL, &error) == NULL);
    assert(strstr(error.message, "producer") != NULL);
    assert(plan_to_json(NULL, &producer, &error) == NULL);

    /* The Composer's own stamp. */
    PlanProducer own = composer_plan_producer(test_knowledge());
    assert(strcmp(own.name, "ursatz-composer") == 0 && strcmp(own.version, composer_version()) == 0);
    assert(strlen(own.producer_key) == 16 && strcmp(own.composition_key, composer_composition_key()) == 0 && strlen(own.composition_key) == 16);
    assert(strcmp(own.knowledge_hash, knowledge_snapshot_hash(test_knowledge())) == 0);
    assert(composer_plan_producer(NULL).knowledge_hash[0] == '\0');
    plan_destroy(plan);
}

static bool refuse_every_material(const PitchMaterial *material, void *data) {
    (void)material;
    ++*(int *)data;
    return false;
}

static void test_hooks(void) {
    char *text = read_file(COMPOSER_SNAPSHOT_DIR "/fill.plan.json");
    int asked = 0;
    PlanReadHooks refusing = {refuse_every_material, NULL, &asked};
    Plan *plan = NULL;
    PlanJsonError error;
    assert(plan_from_json(test_knowledge(), text, &refusing, &plan, &error) == PLAN_ERR_FORMAT);
    assert(plan == NULL && asked > 0);
    assert(strcmp(error.message, "the Composer composes in major and natural minor keys only") == 0);
    assert(strstr(error.path, "material") != NULL);
    /* No hooks: every material is supported. */
    assert(plan_from_json(test_knowledge(), text, NULL, &plan, &error) == PLAN_OK);
    plan_destroy(plan);

    /* A format 4 text has no form record: only the Composer's hook gives the plan one, the one of the form its root is named for. */
    cJSON *document = cJSON_Parse(text);
    assert(document != NULL);
    cJSON_ReplaceItemInObject(document, "format_version", cJSON_CreateNumber(4));
    cJSON *root = cJSON_GetObjectItemCaseSensitive(document, "form");
    cJSON *composer = cJSON_GetObjectItemCaseSensitive(root, "composer");
    assert(cJSON_GetObjectItemCaseSensitive(composer, "form") != NULL);
    cJSON_DeleteItemFromObject(composer, "form");
    char *old = cJSON_Print(document);
    cJSON_Delete(document);
    free(text);
    assert(plan_from_json(test_knowledge(), old, NULL, &plan, &error) == PLAN_OK);
    assert(plan_form_record(plan) == NULL);
    plan_destroy(plan);
    assert(plan_from_json(test_knowledge(), old, composer_plan_read_hooks(), &plan, &error) == PLAN_OK);
    assert(plan_form_record(plan) != NULL && strcmp(plan_form_record(plan)->name, "binary") == 0);
    plan_destroy(plan);
    cJSON_free(old);
}

int main(void) {
    test_clone_of_every_stored_plan();
    test_clone_of_a_plan_with_a_removed_subtree();
    test_copy_into();
    test_reopen();
    test_stamp();
    test_hooks();
    test_knowledge_release();
    printf("test_plan_clone: ok\n");
    return 0;
}
