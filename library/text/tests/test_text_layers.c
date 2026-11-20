#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plan/plan.h"
#include "plan/plan_json.h"
#include "reading/reading.h"
#include "reading/reading_json.h"
#include "realization/realization_json.h"
#include "realization/work.h"
#include "text/text_layers.h"
#include "text/text_tree.h"
#include "ursatz_test.h"

/*
 * The layer codecs of Ursatz Text (R-662): each plan, reading and realization fixture is read with its codec, written (the codec's JSON
 * j), printed as text and read back through the codec again: the JSON that comes out is j byte for byte. Knowledge files and entries
 * come back as equal trees. The refusals name the kind or the codec's path.
 */

static KnowledgeSnapshot *snapshot;
static size_t json_bytes, text_bytes;

/* The codec's JSON of a file of the kind (what the layer's own writer gives for what its reader read); NULL with the error printed. */
static char *codec_json(TextKind kind, const char *path) {
    char *source = test_must_read_file(path, NULL);
    char *out = NULL;
    if (kind == TEXT_KIND_PLAN) {
        cJSON *document = cJSON_Parse(source);
        PlanProducer producer;
        Plan *plan = NULL;
        PlanJsonError error = {{0}, {0}, 0};
        text_plan_producer(document, &producer);
        if (CHECKF(plan_from_json(snapshot, source, NULL, &plan, &error) == PLAN_OK, "%s: %s: %s", path, error.path, error.message)) {
            out = plan_to_json(plan, &producer, &error);
            plan_destroy(plan);
        }
        cJSON_Delete(document);
    } else if (kind == TEXT_KIND_READING) {
        Reading *reading = NULL;
        ReadingJsonError error = {{0}, {0}};
        if (CHECKF(reading_from_json(snapshot, source, &reading, &error) == REALIZATION_OK, "%s: %s: %s", path, error.path, error.message)) {
            out = reading_to_json(reading, &error);
            reading_destroy(reading);
        }
    } else {
        Work *work = NULL;
        RealizationJsonError error = {{0}, {0}};
        if (CHECKF(realization_work_from_json(snapshot, source, &work, &error) == REALIZATION_OK, "%s: %s: %s", path, error.path,
                   error.message)) {
            out = realization_work_to_json(work, &error);
            work_destroy(work);
        }
    }
    free(source);
    CHECKF(out != NULL, "%s: the codec wrote nothing", path);
    return out;
}

static void check_layer(TextKind kind, const char *path) {
    char *json = codec_json(kind, path);
    if (!json) {
        return;
    }
    TextError error = {0};
    char *text = text_from_json(json, kind, &error);
    if (CHECKF(text != NULL, "%s: %s", path, error.message)) {
        TextKind found = TEXT_KIND_JSON;
        char *back = text_to_json(snapshot, text, &found, &error);
        if (CHECKF(back != NULL, "%s: line %d: %s", path, error.line, error.message)) {
            CHECKF(found == kind, "%s: kind", path);
            CHECKF(strcmp(json, back) == 0, "%s: the JSON changed", path);
        }
        free(back);
        json_bytes += strlen(json);
        text_bytes += strlen(text);
    }
    free(text);
    free(json);
}

static void check_list(const char *list_path, TextKind kind) {
    char *list = test_must_read_file(list_path, NULL);
    int count = 0;
    for (char *line = strtok(list, "\n"); line; line = strtok(NULL, "\n")) {
        check_layer(kind, line);
        count++;
    }
    free(list);
    printf("text_layers: %d %s files\n", count, text_kind_name(kind));
    CHECK(count > 0);
}

static void check_tree_equal(const char *json, TextKind kind, const char *what) {
    TextError error = {0};
    char *text = text_from_json(json, kind, &error);
    if (!CHECKF(text != NULL, "%s: %s", what, error.message)) {
        return;
    }
    TextKind found = TEXT_KIND_PLAN;
    char *back = text_to_json(snapshot, text, &found, &error);
    if (CHECKF(back != NULL, "%s: %s", what, error.message)) {
        cJSON *a = cJSON_Parse(json);
        cJSON *b = cJSON_Parse(back);
        CHECKF(found == kind, "%s: kind", what);
        CHECKF(a && b && cJSON_Compare(a, b, true), "%s: the tree changed", what);
        char *pa = a ? cJSON_PrintUnformatted(a) : NULL;
        char *pb = b ? cJSON_PrintUnformatted(b) : NULL;
        CHECKF(pa && pb && strcmp(pa, pb) == 0, "%s: the member order changed", what);
        free(pa);
        free(pb);
        cJSON_Delete(a);
        cJSON_Delete(b);
    }
    free(back);
    free(text);
}

static void check_knowledge(void) {
    char *list = test_must_read_file(TEXT_KNOWLEDGE_FILES, NULL);
    int files = 0, entries = 0;
    for (char *line = strtok(list, "\n"); line; line = strtok(NULL, "\n")) {
        char *json = test_must_read_file(line, NULL);
        cJSON *document = cJSON_Parse(json);
        const cJSON *schema = test_json_member(document, "schema");
        bool is_file = cJSON_IsString(schema) && strcmp(schema->valuestring, "ursatz-knowledge") == 0;
        check_tree_equal(json, is_file ? TEXT_KIND_KNOWLEDGE : TEXT_KIND_JSON, line);
        files++;
        const cJSON *list_of_entries = test_json_member(document, "entries");
        const cJSON *entry;
        cJSON_ArrayForEach(entry, list_of_entries) {
            char *entry_json = cJSON_PrintUnformatted(entry);
            check_tree_equal(entry_json, TEXT_KIND_KNOWLEDGE_ENTRY, line);
            free(entry_json);
            entries++;
        }
        cJSON_Delete(document);
        free(json);
    }
    free(list);
    printf("text_layers: %d knowledge files, %d entries\n", files, entries);
    CHECK(files >= 5 && entries >= 20);
}

static void expect_text_error(TextError *error, const char *part) {
    CHECKF(error->line == 0 && strstr(error->message, part) != NULL, "wanted '%s', got line %d '%s'", part, error->line, error->message);
}

static void check_refusals(void) {
    char path[512];
    snprintf(path, sizeof path, "%s/loop.reading.json", TEXT_FIXTURES);
    char *reading = test_must_read_file(path, NULL);
    TextError error = {-1, ""};
    CHECK(text_from_json(reading, TEXT_KIND_PLAN, &error) == NULL);
    expect_text_error(&error, "this is not a plan");
    CHECK(text_from_json(reading, TEXT_KIND_REALIZATION, &error) == NULL);
    expect_text_error(&error, "this is not a realization");
    CHECK(text_from_json("[1]", TEXT_KIND_JSON, &error) == NULL);
    CHECK(text_from_json("{\"id\": 1}", TEXT_KIND_KNOWLEDGE_ENTRY, &error) == NULL);
    expect_text_error(&error, "this is not a knowledge-entry");
    CHECK(text_from_json("{\"a\": ", TEXT_KIND_JSON, &error) == NULL);
    expect_text_error(&error, "does not parse");
    char *as_json = text_from_json(reading, TEXT_KIND_JSON, &error);
    CHECK(as_json != NULL && strncmp(as_json, "ursatz-text 1 json\n", 19) == 0);
    TextKind kind = TEXT_KIND_PLAN;
    char *back = as_json ? text_to_json(snapshot, as_json, &kind, &error) : NULL;
    CHECK(back != NULL && kind == TEXT_KIND_JSON);
    free(back);
    free(as_json);

    /* a plan text with a broken member: the codec's path in the error */
    snprintf(path, sizeof path, "%s/loop.plan.json", TEXT_FIXTURES);
    char *plan = test_must_read_file(path, NULL);
    char *text = text_from_json(plan, TEXT_KIND_PLAN, &error);
    CHECK(text != NULL);
    const char *at = text ? strstr(text, "format_version 6") : NULL;
    CHECK(at != NULL);
    if (at) {
        char *broken = malloc(strlen(text) + 1);
        memcpy(broken, text, strlen(text) + 1);
        broken[at - text + strlen("format_version ")] = '9'; /* a version the codec does not know */
        CHECK(text_to_json(snapshot, broken, &kind, &error) == NULL);
        CHECKF(strncmp(error.message, "format_version: ", 16) == 0, "codec path: %s", error.message);
        CHECK(error.line == 0);
        free(broken);
    }
    TextKind wrong = TEXT_KIND_JSON;
    CHECK(text && text_to_json(snapshot, "ursatz-text 1 reading\nformat ursatz-plan\n", &wrong, &error) == NULL);
    free(text);
    free(plan);
    free(reading);
    CHECK(text_to_json(snapshot, "not a document", &kind, &error) == NULL && error.line == 1);
}

int main(void) {
    char message[256] = "";
    if (knowledge_snapshot_build(NULL, 0, &snapshot, message, sizeof message) != KNOWLEDGE_OK) {
        fprintf(stderr, "snapshot: %s\n", message);
        return 1;
    }
    check_list(TEXT_PLAN_FILES, TEXT_KIND_PLAN);
    char path[512];
    const char *readings[] = {TEXT_READING_MOZART, NULL, NULL};
    char loop_reading[512], bach_reading[512];
    snprintf(loop_reading, sizeof loop_reading, "%s/loop.reading.json", TEXT_FIXTURES);
    snprintf(bach_reading, sizeof bach_reading, "%s/bach-114.reading.json", TEXT_FIXTURES);
    readings[1] = loop_reading;
    readings[2] = bach_reading;
    for (size_t i = 0; i < 3; i++) {
        check_layer(TEXT_KIND_READING, readings[i]);
    }
    const char *realizations[] = {"loop.realization.json", "bach-114.realization.json"};
    for (size_t i = 0; i < 2; i++) {
        snprintf(path, sizeof path, "%s/%s", TEXT_FIXTURES, realizations[i]);
        check_layer(TEXT_KIND_REALIZATION, path);
    }
    check_knowledge();
    check_refusals();
    printf("text_layers: %zu bytes of JSON, %zu of text\n", json_bytes, text_bytes);
    knowledge_snapshot_destroy(snapshot);
    return test_finish("text_layers");
}
