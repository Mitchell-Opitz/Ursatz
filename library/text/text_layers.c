#include "text/text_layers.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <common/strings.h>

#include "plan/plan.h"
#include "reading/reading.h"
#include "reading/reading_json.h"
#include "realization/realization_json.h"
#include "realization/work.h"
#include "text/text_tree.h"

static void fail(TextError *error, const char *format, ...) {
    if (!error) {
        return;
    }
    error->line = 0;
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof error->message, format, args);
    va_end(args);
}

static void fail_at(TextError *error, const char *path, const char *message) {
    if (!error) {
        return;
    }
    error->line = 0;
    if (path && path[0]) {
        snprintf(error->message, sizeof error->message, "%s: %s", path, message);
    } else {
        snprintf(error->message, sizeof error->message, "%s", message);
    }
}

static bool member_is(const cJSON *document, const char *name, const char *value) {
    const cJSON *m = cJSON_GetObjectItemCaseSensitive(document, name);
    return cJSON_IsString(m) && m->valuestring && strcmp(m->valuestring, value) == 0;
}

static bool is_kind(const cJSON *document, TextKind kind) {
    switch (kind) {
    case TEXT_KIND_JSON:
        return cJSON_IsObject(document);
    case TEXT_KIND_PLAN:
        return member_is(document, "format", "ursatz-plan") || member_is(document, "format", "ursatz-composer-plan");
    case TEXT_KIND_READING:
        return member_is(document, "format", "ursatz-reading");
    case TEXT_KIND_REALIZATION:
        return member_is(document, "format", "ursatz-realization");
    case TEXT_KIND_KNOWLEDGE:
        return member_is(document, "schema", "ursatz-knowledge");
    case TEXT_KIND_KNOWLEDGE_ENTRY: {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(document, "id");
        const cJSON *entry_kind = cJSON_GetObjectItemCaseSensitive(document, "kind");
        return cJSON_IsString(id) && cJSON_IsString(entry_kind);
    }
    default:
        return false;
    }
}

char *text_from_json(const char *json, TextKind kind, TextError *error) {
    cJSON *document = json ? cJSON_Parse(json) : NULL;
    if (!document) {
        fail(error, "the JSON does not parse");
        return NULL;
    }
    if (!is_kind(document, kind)) {
        fail(error, "this is not a %s", text_kind_name(kind));
        cJSON_Delete(document);
        return NULL;
    }
    char *text = text_tree_print(document, kind);
    cJSON_Delete(document);
    if (!text) {
        fail(error, "out of memory");
    }
    return text;
}

static void copy_member(const cJSON *metadata, const char *name, char *out, size_t size) {
    const cJSON *m = metadata ? cJSON_GetObjectItemCaseSensitive(metadata, name) : NULL;
    out[0] = '\0';
    if (cJSON_IsString(m) && m->valuestring) {
        snprintf(out, size, "%s", m->valuestring);
    }
}

bool text_plan_producer(const cJSON *document, PlanProducer *out) {
    if (!cJSON_IsObject(document) || !out) {
        return false;
    }
    const cJSON *metadata = cJSON_GetObjectItemCaseSensitive(document, "metadata");
    if (!cJSON_IsObject(metadata)) {
        metadata = NULL;
    }
    copy_member(metadata, "producer", out->name, sizeof out->name);
    copy_member(metadata, "producer_version", out->version, sizeof out->version);
    copy_member(metadata, "producer_key", out->producer_key, sizeof out->producer_key);
    copy_member(metadata, "composition_key", out->composition_key, sizeof out->composition_key);
    copy_member(metadata, "knowledge_hash", out->knowledge_hash, sizeof out->knowledge_hash);
    return true;
}

static char *plan_through_codec(const KnowledgeSnapshot *knowledge, const cJSON *document, const char *json, TextError *error) {
    PlanProducer producer;
    text_plan_producer(document, &producer);
    Plan *plan = NULL;
    PlanJsonError perror = {0};
    if (plan_from_json(knowledge, json, NULL, &plan, &perror) != PLAN_OK) {
        fail_at(error, perror.path, perror.message[0] ? perror.message : "this is not a plan");
        return NULL;
    }
    PlanJsonError werror = {0};
    char *out = plan_to_json(plan, &producer, &werror);
    plan_destroy(plan);
    if (!out) {
        fail_at(error, werror.path, werror.message[0] ? werror.message : "the plan cannot be written");
    }
    return out;
}

static char *reading_through_codec(const KnowledgeSnapshot *knowledge, const char *json, TextError *error) {
    Reading *reading = NULL;
    ReadingJsonError rerror = {0};
    if (reading_from_json(knowledge, json, &reading, &rerror) != REALIZATION_OK) {
        fail_at(error, rerror.path, rerror.message[0] ? rerror.message : "this is not a reading");
        return NULL;
    }
    ReadingJsonError werror = {0};
    char *out = reading_to_json(reading, &werror);
    reading_destroy(reading);
    if (!out) {
        fail_at(error, werror.path, werror.message[0] ? werror.message : "the reading cannot be written");
    }
    return out;
}

static char *realization_through_codec(const KnowledgeSnapshot *knowledge, const char *json, TextError *error) {
    Work *work = NULL;
    RealizationJsonError rerror = {0};
    if (realization_work_from_json(knowledge, json, &work, &rerror) != REALIZATION_OK) {
        fail_at(error, rerror.path, rerror.message[0] ? rerror.message : "this is not a realization");
        return NULL;
    }
    RealizationJsonError werror = {0};
    char *out = realization_work_to_json(work, &werror);
    work_destroy(work);
    if (!out) {
        fail_at(error, werror.path, werror.message[0] ? werror.message : "the realization cannot be written");
    }
    return out;
}

char *text_to_json(const KnowledgeSnapshot *knowledge, const char *text, TextKind *kind, TextError *error) {
    TextKind found = TEXT_KIND_JSON;
    cJSON *tree = text_tree_parse(text, &found, error);
    if (!tree) {
        return NULL;
    }
    if (kind) {
        *kind = found;
    }
    char *out = NULL;
    if (found == TEXT_KIND_PLAN || found == TEXT_KIND_READING || found == TEXT_KIND_REALIZATION) {
        char *json = cJSON_PrintUnformatted(tree);
        if (!json) {
            fail(error, "out of memory");
        } else if (found == TEXT_KIND_PLAN) {
            out = plan_through_codec(knowledge, tree, json, error);
        } else if (found == TEXT_KIND_READING) {
            out = reading_through_codec(knowledge, json, error);
        } else {
            out = realization_through_codec(knowledge, json, error);
        }
        free(json);
    } else {
        out = cJSON_Print(tree);
        if (!out) {
            fail(error, "out of memory");
        }
    }
    cJSON_Delete(tree);
    return out;
}
