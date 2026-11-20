#include "coordinator/dispatch.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base/command_context.h"
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

/* --- the table of commands ------------------------------------------------------------------------------------------------------- */

typedef struct {
    const char *name;
    WorkshopHandler handler;
    bool takes_base_version; /* the command changes an item that has versions: it needs the version the caller read (commit 3) */
} CommandEntry;

static const CommandEntry kCommands[] = {
    {"library.import", workshop_c_library_import, false},
    {"library.update", workshop_c_library_update, false},
    {"library.delete", workshop_c_library_delete, false},
    {"library.reanalyze", workshop_c_library_reanalyze, false},
    {"collection.create", workshop_c_collection_create, false},
    {"collection.update", workshop_c_collection_update, false},
    {"collection.delete", workshop_c_collection_delete, false},
    {"collection.add-pieces", workshop_c_collection_add_pieces, false},
    {"collection.remove-piece", workshop_c_collection_remove_piece, false},
    {"style.build", workshop_c_style_build, false},
    {"style.rebuild", workshop_c_style_rebuild, false},
    {"style.delete", workshop_c_style_delete, false},
    {"links.run", workshop_c_links_run, false},
    {"links.delete", workshop_c_links_delete, false},
    {"project.create", workshop_c_project_create, false},
    {"project.update", workshop_c_project_update, false},
    {"project.delete", workshop_c_project_delete, false},
    {"item.create", workshop_c_item_create, false},
    {"item.update", workshop_c_item_update, true},
    {"item.delete", workshop_c_item_delete, true},
    {"item.save", workshop_c_item_save, true},
    {"item.reanalyze", workshop_c_item_reanalyze, true},
    {"item.compose", workshop_c_item_compose, true},
    {"item.regenerate", workshop_c_item_regenerate, true},
    {"item.variations", workshop_c_item_variations, false},
    {"item.choose", workshop_c_item_choose, true},
    {"item.set-op-checks", workshop_c_item_set_op_checks, false},
    {"item.save-variant", workshop_c_item_save_variant, false},
    {"item.flag", workshop_c_item_flag, true},
    {"item.promote", workshop_c_item_promote, true},
    {"item.link-form", workshop_c_item_link_form, true},
    {"item.reroll", workshop_c_item_reroll, true},
    {"item.update-use", workshop_c_item_update_use, true},
    {"subject.create", workshop_c_subject_create, false},
    {"subject.update", workshop_c_subject_update, false},
    {"subject.delete", workshop_c_subject_delete, false},
    {"subject.link-item", workshop_c_subject_link_item, false},
    {"subject.unlink-item", workshop_c_subject_unlink_item, false},
    {"subject.link-piece", workshop_c_subject_link_piece, false},
    {"subject.unlink-piece", workshop_c_subject_unlink_piece, false},
    {"subject.relate", workshop_c_subject_relate, false},
    {"subject.unrelate", workshop_c_subject_unrelate, false},
    {"job.cancel", workshop_c_job_cancel, false}, /* a runtime control: it changes no file and writes no record */
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

/* --- commands -------------------------------------------------------------------------------------------------------------------- */

/*
 * A command that changes an item that has versions needs the version the caller read (base_version) and is refused with CONFLICT, "changed
 * since you read it" and the current_version in the reply, when it is not the item's current one. An absent item is left to the handler. A
 * job command is checked here, at submit, and again when it writes (items/store.c).
 */
static UrsatzStatus check_base_version(const WorkshopCall *call, WorkshopReply *reply, UrsatzError *error) {
    if (call->target[0] == '\0') {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/target", "the command needs a target");
    }
    cJSON *meta = NULL;
    if (workshop_material_item_get(call->target, &meta, NULL) != URSATZ_OK) {
        return URSATZ_OK; /* an item that is not there is the handler's to report: not found */
    }
    if (call->base_version < 0) {
        cJSON_Delete(meta);
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/base_version", "base_version is required: the version you read");
    }
    const cJSON *current = cJSON_GetObjectItemCaseSensitive(meta, "current_version");
    long current_version = cJSON_IsNumber(current) ? (long)current->valuedouble : 0;
    cJSON_Delete(meta);
    if (current_version == call->base_version) {
        return URSATZ_OK;
    }
    cJSON *object = workshop_reply_object(reply);
    if (object) {
        cJSON_AddNumberToObject(object, "current_version", (double)current_version);
    }
    return workshop_call_fail(error, URSATZ_ERR_CONFLICT, "/base_version", "%s", WORKSHOP_CHANGED_SINCE_READ);
}

UrsatzStatus workshop_command(Workshop *workshop, const cJSON *command, const WorkshopBytes *bytes, WorkshopReply *reply,
                              UrsatzError *error) {
    if (reply) {
        memset(reply, 0, sizeof(*reply));
    }
    if (!workshop || !reply || !cJSON_IsObject(command)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "", "a command is an object");
    }
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(command, "command");
    const cJSON *actor = cJSON_GetObjectItemCaseSensitive(command, "actor");
    const cJSON *target = cJSON_GetObjectItemCaseSensitive(command, "target");
    const cJSON *base = cJSON_GetObjectItemCaseSensitive(command, "base_version");
    const cJSON *params = cJSON_GetObjectItemCaseSensitive(command, "params");
    if (!cJSON_IsString(name) || name->valuestring[0] == '\0') {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/command", "a command has a name");
    }
    if (!cJSON_IsString(actor) || actor->valuestring[0] == '\0' || strlen(actor->valuestring) >= sizeof(((WorkshopCommandContext *)0)->actor)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/actor", "a command has an actor, a text of up to 63 characters");
    }
    if (target && !cJSON_IsNull(target) && (!cJSON_IsString(target) || strlen(target->valuestring) >= 256)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/target", "target is an id or null");
    }
    if (base && !cJSON_IsNull(base) &&
        (!cJSON_IsNumber(base) || base->valuedouble < 0 || base->valuedouble != (double)(long)base->valuedouble)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/base_version", "base_version is a whole number or absent");
    }
    if (params && !cJSON_IsNull(params) && !cJSON_IsObject(params)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "params is an object");
    }
    const CommandEntry *entry = NULL;
    for (size_t i = 0; i < sizeof(kCommands) / sizeof(kCommands[0]); i++) {
        if (strcmp(kCommands[i].name, name->valuestring) == 0) {
            entry = &kCommands[i];
            break;
        }
    }
    if (!entry || strlen(name->valuestring) >= sizeof(((WorkshopCommandContext *)0)->command)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/command", "unknown command %s", name->valuestring);
    }
    cJSON *empty = cJSON_IsObject(params) ? NULL : cJSON_CreateObject();
    if (!cJSON_IsObject(params) && !empty) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    long base_version = cJSON_IsNumber(base) ? (long)base->valuedouble : -1;
    WorkshopCall call = {workshop, cJSON_IsObject(params) ? params : empty, bytes, actor->valuestring,
                         cJSON_IsString(target) ? target->valuestring : "", base_version};
    WorkshopCommandContext context;
    memset(&context, 0, sizeof(context));
    snprintf(context.command, sizeof(context.command), "%s", entry->name);
    snprintf(context.actor, sizeof(context.actor), "%s", actor->valuestring);
    snprintf(context.target, sizeof(context.target), "%s", call.target);
    context.base_version = base_version;
    if (entry->takes_base_version) {
        UrsatzStatus refused = check_base_version(&call, reply, error);
        if (refused != URSATZ_OK) {
            cJSON_Delete(empty);
            return refused;
        }
    }
    workshop_command_context_set(&context);
    workshop_command_context_set_last_seq(0);
    UrsatzStatus status = entry->handler(&call, reply, error);
    workshop_command_context_set(NULL);
    cJSON_Delete(empty);
    if (status == URSATZ_OK && cJSON_IsObject(reply->json)) {
        cJSON_AddNumberToObject(reply->json, "journal_seq", (double)workshop_command_context_last_seq());
    }
    if (status != URSATZ_OK && error && error->message[0] == '\0') {
        workshop_call_fail(error, status, error->path, "the command %s failed", entry->name);
    }
    return status;
}
