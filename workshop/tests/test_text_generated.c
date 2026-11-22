#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <plan/plan_json.h>
#include <reading/reading_json.h>
#include <realization/realization_json.h>
#include <text/text_layers.h>
#include <ursatz_workshop/workshop_api.h>

#include "base/knowledge.h"
#include "ursatz_test.h"

/*
 * Ursatz Text over generated layers (R-662, slow): the plan, the realization and the reading of a composed piece of every built-in form but the
 * game loop (seed 1, C major) and of the structure-only loop request, and of five analyzed pieces, each read with its codec, printed as text and
 * read back: the codec's JSON that comes out is the one that went in, byte for byte (text_layers does the same for the fixtures).
 *
 * With the argument "smoke" (the fast set) it composes only the form `binary`, runs the loop request and analyzes only Bach 114.mid:
 * nine layers.
 */

#ifndef TEXT_FORMS
#error "TEXT_FORMS must name composer/forms"
#endif
#ifndef TEXT_LOOP_REQUEST
#error "TEXT_LOOP_REQUEST must name composer/tests/fixtures/piece/structure-only.json"
#endif
#ifndef TEXT_CORPUS
#error "TEXT_CORPUS must name tests/fixtures/framework-v1-corpus"
#endif

static const KnowledgeSnapshot *snapshot;
static int round_trips;

/* The codec's JSON of a layer text, or NULL after a failed check. */
static char *codec_json(TextKind kind, const char *source, const char *what) {
    char *out = NULL;
    if (kind == TEXT_KIND_PLAN) {
        cJSON *document = cJSON_Parse(source);
        PlanProducer producer;
        Plan *plan = NULL;
        PlanJsonError error = {{0}, {0}, 0};
        text_plan_producer(document, &producer);
        if (CHECKF(plan_from_json(snapshot, source, NULL, &plan, &error) == PLAN_OK, "%s plan: %s: %s", what, error.path, error.message)) {
            out = plan_to_json(plan, &producer, &error);
            plan_destroy(plan);
        }
        cJSON_Delete(document);
    } else if (kind == TEXT_KIND_READING) {
        Reading *reading = NULL;
        ReadingJsonError error = {{0}, {0}};
        if (CHECKF(reading_from_json(snapshot, source, &reading, &error) == REALIZATION_OK, "%s reading: %s: %s", what, error.path, error.message)) {
            out = reading_to_json(reading, &error);
            reading_destroy(reading);
        }
    } else {
        Work *work = NULL;
        RealizationJsonError error = {{0}, {0}};
        if (CHECKF(realization_work_from_json(snapshot, source, &work, &error) == REALIZATION_OK, "%s realization: %s: %s", what, error.path, error.message)) {
            out = realization_work_to_json(work, &error);
            work_destroy(work);
        }
    }
    return out;
}

static void round_trip(TextKind kind, const char *source, const char *what) {
    if (!CHECKF(source != NULL, "%s: no %s", what, text_kind_name(kind))) {
        return;
    }
    char *json = codec_json(kind, source, what);
    if (!json) {
        return;
    }
    TextError error = {0, ""};
    char *text = text_from_json(json, kind, &error);
    if (CHECKF(text != NULL, "%s %s: %s", what, text_kind_name(kind), error.message)) {
        TextKind found = TEXT_KIND_JSON;
        char *back = text_to_json(snapshot, text, &found, &error);
        if (CHECKF(back != NULL, "%s %s: line %d: %s", what, text_kind_name(kind), error.line, error.message)) {
            CHECKF(found == kind && strcmp(json, back) == 0, "%s %s: the JSON changed", what, text_kind_name(kind));
            round_trips++;
        }
        free(back);
    }
    free(text);
    free(json);
}

static char *member_text(const cJSON *object, const char *name) {
    const cJSON *m = cJSON_GetObjectItemCaseSensitive(object, name);
    if (cJSON_IsString(m)) {
        return strdup(m->valuestring);
    }
    return cJSON_IsObject(m) ? cJSON_PrintUnformatted(m) : NULL;
}

/* The three layers of a reply (a composed piece: the document's realization and reading, and plan_json; an analysis: realization, reading and plan). */
static void check_reply(const WorkshopReply *reply, bool composed, const char *what) {
    cJSON *document = NULL;
    const cJSON *holder = reply->json;
    if (composed) {
        const cJSON *json = cJSON_GetObjectItemCaseSensitive(reply->json, "json");
        document = cJSON_IsString(json) ? cJSON_Parse(json->valuestring) : NULL;
        holder = document;
    }
    char *plan = member_text(composed ? reply->json : holder, composed ? "plan_json" : "plan");
    char *realization = member_text(holder, "realization");
    char *reading = member_text(holder, "reading");
    round_trip(TEXT_KIND_PLAN, plan, what);
    round_trip(TEXT_KIND_REALIZATION, realization, what);
    round_trip(TEXT_KIND_READING, reading, what);
    free(plan);
    free(realization);
    free(reading);
    cJSON_Delete(document);
}

static int name_cmp(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

int main(int argc, char **argv) {
    bool smoke = argc > 1 && strcmp(argv[1], "smoke") == 0;
    UrsatzError error;
    Workshop *workshop = NULL;
    if (workshop_tools_open(&workshop, &error) != URSATZ_OK) {
        fprintf(stderr, "cannot open the tools workshop: %s\n", error.message);
        return 1;
    }
    snapshot = workshop_knowledge_snapshot();
    CHECK(snapshot != NULL);

    /* every form but the game loop */
    char *names[64];
    size_t count = 0;
    DIR *dir = opendir(TEXT_FORMS);
    struct dirent *entry;
    while (dir && (entry = readdir(dir)) != NULL && count < 64) {
        size_t length = strlen(entry->d_name);
        if (length > 5 && strcmp(entry->d_name + length - 5, ".json") == 0 && strcmp(entry->d_name, "game-loop.json") != 0 &&
            (!smoke || strcmp(entry->d_name, "binary.json") == 0)) {
            names[count] = strndup(entry->d_name, length - 5);
            count++;
        }
    }
    if (dir) {
        closedir(dir);
    }
    qsort(names, count, sizeof names[0], name_cmp);
    CHECKF(smoke ? count == 1 : count >= 15, "only %zu forms", count);
    for (size_t i = 0; i < count; i++) {
        const char *n = names[i];
        const char *meter = (strcmp(n, "minuet") == 0 || strcmp(n, "minuet-and-trio") == 0 || strcmp(n, "scherzo") == 0 ||
                             strcmp(n, "scherzo-and-trio") == 0 || strcmp(n, "trio") == 0)
                                ? "3/4"
                                : "4/4";
        char request[512];
        snprintf(request, sizeof request,
                 "{\"entry\":\"piece\",\"request\":{\"request\":{\"schema\":\"ursatz-piece-request\",\"schema_version\":2,\"form\":\"%s\","
                 "\"key\":\"C\",\"material\":\"major\",\"meter\":\"%s\"},\"seed\":\"1\",\"layers\":true}}",
                 n, meter);
        cJSON *params = cJSON_Parse(request);
        WorkshopReply reply;
        memset(&error, 0, sizeof error);
        UrsatzStatus status = workshop_tool(workshop, "composer", params, NULL, &reply, &error);
        cJSON_Delete(params);
        if (CHECKF(status == URSATZ_OK, "%s: composing: %s", n, error.message)) {
            check_reply(&reply, true, n);
        }
        workshop_reply_destroy(&reply);
        free(names[i]);
    }

    /* the structure-only loop request */
    char *loop = test_must_read_file(TEXT_LOOP_REQUEST, NULL);
    cJSON *request = cJSON_CreateObject();
    cJSON_AddItemToObject(request, "request", cJSON_Parse(loop));
    free(loop);
    cJSON_AddStringToObject(request, "seed", "1");
    cJSON_AddTrueToObject(request, "layers");
    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "entry", "piece");
    cJSON_AddItemToObject(params, "request", request);
    WorkshopReply reply;
    memset(&error, 0, sizeof error);
    UrsatzStatus status = workshop_tool(workshop, "composer", params, NULL, &reply, &error);
    cJSON_Delete(params);
    if (CHECKF(status == URSATZ_OK, "loop: %s", error.message)) {
        check_reply(&reply, true, "structure-only");
    }
    workshop_reply_destroy(&reply);

    /* the five analyzed pieces of the text export goldens */
    static const char *const PIECES[] = {"Bach 114.mid", "Mozart 545.mid", "Clementi 36.mid", "Field 1.mid", "Field 5.mid"};
    for (size_t i = 0; i < (smoke ? 1 : sizeof PIECES / sizeof PIECES[0]); i++) {
        char path[600];
        snprintf(path, sizeof path, "%s/%s", TEXT_CORPUS, PIECES[i]);
        params = cJSON_CreateObject();
        cJSON_AddStringToObject(params, "path", path);
        cJSON_AddTrueToObject(params, "layers");
        cJSON_AddTrueToObject(params, "model_plan");
        memset(&error, 0, sizeof error);
        status = workshop_tool(workshop, "analyze", params, NULL, &reply, &error);
        cJSON_Delete(params);
        if (CHECKF(status == URSATZ_OK, "%s: analyzing: %s", PIECES[i], error.message)) {
            check_reply(&reply, false, PIECES[i]);
        }
        workshop_reply_destroy(&reply);
    }
    printf("workshop_text_generated: %d layers round-tripped\n", round_trips);
    /* the forms, the loop request and the analyzed pieces, three layers each */
    CHECKF(round_trips >= 3 * (int)(count + (smoke ? 2 : 6)), "only %d round trips", round_trips);
    workshop_close(workshop);
    return test_finish(smoke ? "workshop_text_generated smoke" : "workshop_text_generated");
}
