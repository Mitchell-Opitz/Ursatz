#include "coordinator/dispatch.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "coordinator/coordinator.h"

/* --- the table of queries -------------------------------------------------------------------------------------------------------- */

typedef struct {
    const char *name;
    WorkshopHandler handler;
} QueryEntry;

static const QueryEntry kQueries[] = {
    {"library.list", workshop_q_library_list},
    {"library.groups", workshop_q_library_groups},
    {"library.piece", workshop_q_library_piece},
    {"library.document", workshop_q_library_document},
    {"library.export", workshop_q_library_export},
    {"library.musicxml", workshop_q_library_musicxml},
    {"library.meter-defaulted", workshop_q_library_meter_defaulted},
    {"library.playback", workshop_q_library_playback},
    {"collection.list", workshop_q_collection_list},
    {"collection.get", workshop_q_collection_get},
    {"collection.pieces", workshop_q_collection_pieces},
    {"composer.status", workshop_q_composer_status},
    {"composer.vocabulary", workshop_q_composer_vocabulary},
    {"composer.forms", workshop_q_composer_forms},
    {"composer.variant-operations", workshop_q_composer_variant_operations},
    {"composer.render", workshop_q_composer_render},
    {"composer.random-motifs", workshop_q_composer_random_motifs},
    {"composer.import-motif", workshop_q_composer_import_motif},
    {"composer.recognize", workshop_q_composer_recognize},
    {"style.list", workshop_q_style_list},
    {"style.get", workshop_q_style_get},
    {"links.list", workshop_q_links_list},
    {"links.get", workshop_q_links_get},
    {"links.piece", workshop_q_links_piece},
    {"job.get", workshop_q_job_get},
    {"job.list", workshop_q_job_list},
    {"project.list", workshop_q_project_list},
    {"project.get", workshop_q_project_get},
    {"item.list", workshop_q_item_list},
    {"item.get", workshop_q_item_get},
    {"item.version", workshop_q_item_version},
    {"item.export", workshop_q_item_export},
    {"item.score", workshop_q_item_score},
    {"item.playback", workshop_q_item_playback},
    {"item.file", workshop_q_item_file},
    {"item.variation-file", workshop_q_item_variation_file},
    {"item.variation-playback", workshop_q_item_variation_playback},
    {"item.op-checks", workshop_q_item_op_checks},
    {"item.variant-tree", workshop_q_item_variant_tree},
    {"item.validate", workshop_q_item_validate},
    {"item.variants", workshop_q_item_variants},
    {"item.each-op", workshop_q_item_each_op},
    {"subject.list", workshop_q_subject_list},
    {"subject.groups", workshop_q_subject_groups},
    {"subject.get", workshop_q_subject_get},
};

/* --- parameters ------------------------------------------------------------------------------------------------------------------ */

const char *workshop_param_string(const cJSON *params, const char *name, bool *bad) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(params, name);
    if (!item || cJSON_IsNull(item)) {
        return NULL;
    }
    if (!cJSON_IsString(item)) {
        if (bad) {
            *bad = true;
        }
        return NULL;
    }
    return item->valuestring;
}

bool workshop_param_require_string(const cJSON *params, const char *name, const char **out, UrsatzError *error) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(params, name);
    if (!cJSON_IsString(item)) {
        char path[96];
        snprintf(path, sizeof(path), "/params/%s", name);
        workshop_call_fail(error, URSATZ_ERR_REQUEST, path, "%s is required and a string", name);
        return false;
    }
    *out = item->valuestring;
    return true;
}

bool workshop_parse_whole(const char *text, int max_digits, long long *out) {
    size_t length = text ? strlen(text) : 0;
    if (length == 0 || length > (size_t)max_digits) {
        return false;
    }
    long long value = 0;
    for (size_t i = 0; i < length; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
        value = value * 10 + (text[i] - '0');
    }
    *out = value;
    return true;
}

bool workshop_param_int(const cJSON *params, const char *name, long long min, long long max, bool *present, long long *out,
                        UrsatzError *error) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(params, name);
    if (present) {
        *present = item && !cJSON_IsNull(item);
    }
    if (!item || cJSON_IsNull(item)) {
        return true;
    }
    if (!cJSON_IsNumber(item) || item->valuedouble != (double)(long long)item->valuedouble || item->valuedouble < (double)min ||
        item->valuedouble > (double)max) {
        char path[96];
        snprintf(path, sizeof(path), "/params/%s", name);
        workshop_call_fail(error, URSATZ_ERR_REQUEST, path, "%s is a whole number from %lld to %lld", name, min, max);
        return false;
    }
    *out = (long long)item->valuedouble;
    return true;
}

/* --- failures -------------------------------------------------------------------------------------------------------------------- */

UrsatzStatus workshop_call_fail(UrsatzError *error, UrsatzStatus status, const char *path, const char *format, ...) {
    if (error) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(error->message, sizeof(error->message), format, arguments);
        va_end(arguments);
        snprintf(error->path, sizeof(error->path), "%s", path ? path : "");
    }
    return status;
}

UrsatzStatus workshop_call_fail_service(UrsatzError *error, UrsatzStatus status, const char *fallback) {
    const UrsatzError *last = workshop_last_error();
    if (error) {
        snprintf(error->message, sizeof(error->message), "%s", last->message[0] != '\0' ? last->message : fallback);
        snprintf(error->path, sizeof(error->path), "%s", last->path);
    }
    return status;
}

UrsatzStatus workshop_status_of_http(int http_status) {
    switch (http_status) {
        case 400:
        case 413:
            return URSATZ_ERR_REQUEST;
        case 404:
            return URSATZ_ERR_NOT_FOUND;
        case 409:
            return URSATZ_ERR_CONFLICT;
        case 422:
            return URSATZ_ERR_UNSATISFIABLE;
        case 499:
            return URSATZ_ERR_CANCELLED;
        default:
            return URSATZ_ERR_INTERNAL;
    }
}

/* --- replies --------------------------------------------------------------------------------------------------------------------- */

void workshop_reply_destroy(WorkshopReply *reply) {
    if (!reply) {
        return;
    }
    cJSON_Delete(reply->json);
    free(reply->bytes);
    memset(reply, 0, sizeof(*reply));
}

cJSON *workshop_reply_object(WorkshopReply *reply) {
    cJSON *object = cJSON_CreateObject();
    if (object) {
        cJSON_Delete(reply->json);
        reply->json = object;
    }
    return object;
}

UrsatzStatus workshop_reply_member(WorkshopReply *reply, const char *name, cJSON *value, UrsatzError *error) {
    cJSON *object = value ? workshop_reply_object(reply) : NULL;
    if (!object || !cJSON_AddItemToObject(object, name, value)) {
        cJSON_Delete(value);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return URSATZ_OK;
}

UrsatzStatus workshop_reply_tree(WorkshopReply *reply, cJSON *value, UrsatzError *error) {
    if (!value) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    cJSON_Delete(reply->json);
    reply->json = value;
    return URSATZ_OK;
}

UrsatzStatus workshop_reply_text(WorkshopReply *reply, const char *text, size_t length, const char *content_type, UrsatzError *error) {
    if (length == 0) {
        length = strlen(text);
    }
    uint8_t *copy = malloc(length + 1);
    if (!copy) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    memcpy(copy, text, length);
    copy[length] = '\0';
    workshop_reply_take_bytes(reply, copy, length, content_type);
    return URSATZ_OK;
}

void workshop_reply_take_bytes(WorkshopReply *reply, uint8_t *data, size_t size, const char *content_type) {
    free(reply->bytes);
    reply->bytes = data;
    reply->size = size;
    snprintf(reply->content_type, sizeof(reply->content_type), "%s", content_type);
}

UrsatzStatus workshop_reply_job(WorkshopReply *reply, cJSON *object, const char *kind, WorkshopJobFn run, void *input,
                                void (*free_input)(void *), UrsatzError *error) {
    char id[WORKSHOP_JOB_ID_BYTES];
    if (!workshop_job_submit(kind, run, input, free_input, id)) {
        cJSON_Delete(object);
        return workshop_call_fail(error, URSATZ_ERR_INTERNAL, "", "the job queue is not running");
    }
    cJSON *answer = object ? object : cJSON_CreateObject();
    cJSON *job = answer ? cJSON_AddObjectToObject(answer, "job") : NULL;
    if (!job || !cJSON_AddStringToObject(job, "id", id) || !cJSON_AddStringToObject(job, "kind", kind) ||
        !cJSON_AddStringToObject(job, "state", workshop_job_state_name(WORKSHOP_JOB_QUEUED))) {
        cJSON_Delete(answer);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return workshop_reply_tree(reply, answer, error);
}

/* --- queries --------------------------------------------------------------------------------------------------------------------- */

UrsatzStatus workshop_query(Workshop *workshop, const char *name, const cJSON *params, const WorkshopBytes *bytes, WorkshopReply *reply,
                            UrsatzError *error) {
    if (reply) {
        memset(reply, 0, sizeof(*reply));
    }
    if (!workshop || !name || !reply) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/query", "a query needs a workshop, a name and a reply");
    }
    if (params && !cJSON_IsObject(params)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "params is an object");
    }
    for (size_t i = 0; i < sizeof(kQueries) / sizeof(kQueries[0]); i++) {
        if (strcmp(kQueries[i].name, name) != 0) {
            continue;
        }
        cJSON *empty = params ? NULL : cJSON_CreateObject();
        if (!params && !empty) {
            return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
        WorkshopCall call = {workshop, params ? params : empty, bytes, "", "", -1};
        UrsatzStatus status = kQueries[i].handler(&call, reply, error);
        cJSON_Delete(empty);
        if (status != URSATZ_OK && error && error->message[0] == '\0') {
            workshop_call_fail(error, status, error->path, "the query %s failed", name);
        }
        return status;
    }
    return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/query", "unknown query %s", name);
}
