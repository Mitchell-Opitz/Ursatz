#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "coordinator/dispatch.h"
#include "items/views.h"

/* The project and item queries (R-646): the motifs, themes and pieces a person composes, their versions and files. */

#define VERSION_MAX 1000000

static UrsatzStatus material_failure(UrsatzError *error, UrsatzStatus status) {
    return workshop_call_fail_service(error, status, "material store failure");
}

/* A version number from a parameter given as text or as a whole number, 1 to max; false when it is not one. */
static bool read_version(const cJSON *params, const char *name, long long max, long long *out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(params, name);
    if (cJSON_IsNumber(item)) {
        if (item->valuedouble != (double)(long long)item->valuedouble || item->valuedouble < 1 || item->valuedouble > (double)max) {
            return false;
        }
        *out = (long long)item->valuedouble;
        return true;
    }
    return cJSON_IsString(item) && workshop_parse_whole(item->valuestring, 9, out) && *out >= 1 && *out <= max;
}

static const char *text_param(const cJSON *params, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(params, name);
    return cJSON_IsString(item) && item->valuestring[0] != '\0' ? item->valuestring : NULL;
}

WORKSHOP_HANDLER(workshop_q_project_list) {
    (void)call;
    cJSON *projects = NULL;
    UrsatzStatus result = workshop_material_project_list(&projects);
    return result == URSATZ_OK ? workshop_reply_member(reply, "projects", projects, error) : material_failure(error, result);
}

WORKSHOP_HANDLER(workshop_q_project_get) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *project = NULL;
    UrsatzStatus result = workshop_material_project_get(id, &project);
    return result == URSATZ_OK ? workshop_reply_member(reply, "project", project, error) : material_failure(error, result);
}

/* The items of a project and/or a type: params project and type, both optional. */
WORKSHOP_HANDLER(workshop_q_item_list) {
    cJSON *items = NULL;
    UrsatzStatus result = workshop_material_item_list(text_param(call->params, "project"), text_param(call->params, "type"), &items);
    return result == URSATZ_OK ? workshop_reply_member(reply, "items", items, error) : material_failure(error, result);
}

/* {item, body (the current version's), layers?, uses_status, used_by, subjects}. */
WORKSHOP_HANDLER(workshop_q_item_get) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *meta = NULL;
    cJSON *body = NULL;
    cJSON *uses_status = NULL;
    cJSON *used_by = NULL;
    cJSON *subjects = NULL;
    UrsatzStatus result = workshop_material_item_get(id, &meta, &body);
    if (result == URSATZ_OK) {
        result = workshop_material_uses_status(id, &uses_status);
    }
    if (result == URSATZ_OK) {
        result = workshop_material_used_by(id, &used_by);
    }
    if (result == URSATZ_OK) {
        subjects = workshop_subjects_for_item(id);
    }
    cJSON *object = result == URSATZ_OK && subjects ? workshop_reply_object(reply) : NULL;
    if (!object || !cJSON_AddItemToObject(object, "item", meta)) {
        cJSON_Delete(meta);
        cJSON_Delete(body);
        cJSON_Delete(uses_status);
        cJSON_Delete(used_by);
        cJSON_Delete(subjects);
        return result == URSATZ_OK ? workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory") : material_failure(error, result);
    }
    cJSON_AddItemToObject(object, "body", body);
    const cJSON *current = cJSON_GetObjectItemCaseSensitive(meta, "current_version");
    cJSON *layers = NULL;
    if (workshop_material_version_layers(id, cJSON_IsNumber(current) ? (int)current->valuedouble : 0, &layers) == URSATZ_OK) {
        cJSON_AddItemToObject(object, "layers", layers);
    }
    cJSON_AddItemToObject(object, "uses_status", uses_status);
    cJSON_AddItemToObject(object, "used_by", used_by);
    cJSON_AddItemToObject(object, "subjects", subjects);
    return URSATZ_OK;
}

/* {number, body, actor, lineage, layers?}: params id and version; actor and lineage are the version envelope's. */
WORKSHOP_HANDLER(workshop_q_item_version) {
    const char *id = NULL;
    long long number = 0;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!read_version(call->params, "version", VERSION_MAX, &number)) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "", "version not found");
    }
    cJSON *version_body = NULL;
    UrsatzStatus result = workshop_material_item_get_version(id, (int)number, &version_body);
    if (result != URSATZ_OK) {
        return material_failure(error, result);
    }
    cJSON *object = workshop_reply_object(reply);
    if (!object || !cJSON_AddNumberToObject(object, "number", (double)number)) {
        cJSON_Delete(version_body);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    cJSON_AddItemToObject(object, "body", version_body);
    cJSON *envelope = NULL;
    if (workshop_material_version_envelope(id, (int)number, &envelope) == URSATZ_OK) {
        const cJSON *lineage = cJSON_GetObjectItemCaseSensitive(envelope, "lineage");
        cJSON_AddStringToObject(object, "actor", cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(envelope, "actor")));
        cJSON_AddItemToObject(object, "lineage", lineage ? cJSON_Duplicate(lineage, true) : cJSON_CreateNull());
        cJSON_Delete(envelope);
    }
    cJSON *layers = NULL;
    if (workshop_material_version_layers(id, (int)number, &layers) == URSATZ_OK) {
        cJSON_AddItemToObject(object, "layers", layers);
    }
    return URSATZ_OK;
}

/* A view of a version of an item (items/views.h): params id, version (the current one when absent), view (default summary), measures ({first,
 * last}) and section. "json" answers what item.version answers; the others {id, version, view, text}. */
WORKSHOP_HANDLER(workshop_q_item_view) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    long long number = 0;
    const cJSON *version_item = cJSON_GetObjectItemCaseSensitive(call->params, "version");
    if (version_item && !cJSON_IsNull(version_item) && !read_version(call->params, "version", VERSION_MAX, &number)) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "/params/version", "version not found");
    }
    bool bad = false;
    const char *view = workshop_param_string(call->params, "view", &bad);
    if (bad) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/view", "view is a string");
    }
    view = view && view[0] ? view : "summary";
    TextViewLimits limits;
    UrsatzStatus status = workshop_view_limits(call->params, &limits, error);
    if (status != URSATZ_OK) {
        return status;
    }
    if (strcmp(view, "json") == 0) {
        if (number == 0) {
            cJSON *meta = NULL;
            if (workshop_material_item_get(id, &meta, NULL) != URSATZ_OK) {
                return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "/params/id", "item not found");
            }
            const cJSON *current = cJSON_GetObjectItemCaseSensitive(meta, "current_version");
            number = cJSON_IsNumber(current) ? (long long)current->valuedouble : 1;
            cJSON_Delete(meta);
        }
        cJSON *own = cJSON_CreateObject();
        if (!own || !cJSON_AddStringToObject(own, "id", id) || !cJSON_AddNumberToObject(own, "version", (double)number)) {
            cJSON_Delete(own);
            return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
        WorkshopCall again = *call;
        again.params = own;
        status = workshop_q_item_version(&again, reply, error);
        cJSON_Delete(own);
        return status;
    }
    char *text = NULL;
    status = workshop_view_item(id, (int)number, view, &limits, &text, error);
    if (status != URSATZ_OK) {
        return status;
    }
    cJSON *object = workshop_reply_object(reply);
    bool ok = object && cJSON_AddStringToObject(object, "id", id) && cJSON_AddStringToObject(object, "view", view) &&
              cJSON_AddStringToObject(object, "text", text);
    free(text);
    return ok ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

/* The Analyzer's text export of a motif, theme or piece (text/plain): params id and version (the current one when absent). */
WORKSHOP_HANDLER(workshop_q_item_export) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    long long version = 0;
    if (text_param(call->params, "version") || cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(call->params, "version"))) {
        if (!read_version(call->params, "version", 999999, &version)) {
            return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/version", "version must be a whole number from 1");
        }
    }
    char *text = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus result = workshop_material_export(id, (int)version, &text, &failure);
    if (result == URSATZ_ERR_NOT_FOUND) {
        return workshop_call_fail(error, result, "", "item or version not found");
    }
    if (result == URSATZ_ERR_REQUEST) {
        return workshop_call_fail(error, result, "", "%s", failure.message[0] ? failure.message : "composer could not render the motif");
    }
    if (result != URSATZ_OK) {
        return workshop_call_fail(error, result == URSATZ_ERR_ALLOC ? result : URSATZ_ERR_INTERNAL, "", "%s",
                                  result == URSATZ_ERR_ALLOC ? "out of memory" : "failed to export the item");
    }
    UrsatzStatus replied = workshop_reply_text(reply, text, 0, "text/plain; charset=utf-8", error);
    free(text);
    return replied;
}

/* The score (MusicXML) or the playback of a version, with key for the key to sound it in (default: the motif's own); with file, the
 * playback of a theme or piece version's own MIDI file. */
static UrsatzStatus version_render(const WorkshopCall *call, WorkshopReply *reply, UrsatzError *error, bool playback_only) {
    const char *id = NULL;
    long long number = 0;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!read_version(call->params, "version", VERSION_MAX, &number)) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "", "version not found");
    }
    const char *file = playback_only ? text_param(call->params, "file") : NULL;
    const char *render_key = text_param(call->params, "key");
    char *musicxml = NULL;
    cJSON *playback = NULL;
    int status = 200;
    UrsatzStatus result = workshop_motif_version_render(id, (int)number, render_key, playback_only, file, &musicxml, &playback, &status, error);
    if (result != URSATZ_OK) {
        return workshop_status_of_http(status);
    }
    if (playback) {
        return workshop_reply_tree(reply, playback, error);
    }
    UrsatzStatus replied = workshop_reply_text(reply, musicxml, 0, "application/xml", error);
    free(musicxml);
    return replied;
}

WORKSHOP_HANDLER(workshop_q_item_score) {
    return version_render(call, reply, error, false);
}

WORKSHOP_HANDLER(workshop_q_item_playback) {
    return version_render(call, reply, error, true);
}

/* One of a theme or piece version's files: params id, version and name. */
WORKSHOP_HANDLER(workshop_q_item_file) {
    const char *id = NULL;
    const char *name = NULL;
    long long number = 0;
    if (!workshop_param_require_string(call->params, "id", &id, error) ||
        !workshop_param_require_string(call->params, "name", &name, error)) {
        return URSATZ_ERR_REQUEST;
    }
    const char *content_type = workshop_material_version_file_type(name);
    if (!read_version(call->params, "version", VERSION_MAX, &number) || !content_type) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "", "file not found");
    }
    unsigned char *data = NULL;
    size_t size = 0;
    UrsatzStatus result = workshop_material_version_file(id, (int)number, name, &data, &size);
    if (result != URSATZ_OK) {
        return result == URSATZ_ERR_NOT_FOUND ? workshop_call_fail(error, result, "", "file not found") : material_failure(error, result);
    }
    workshop_reply_take_bytes(reply, data, size, content_type);
    snprintf(reply->filename, sizeof(reply->filename), "%s", name);
    return URSATZ_OK;
}

/* A file of a variations set of a piece version: params id, version, set and name. */
WORKSHOP_HANDLER(workshop_q_item_variation_file) {
    const char *id = NULL;
    const char *name = NULL;
    long long version = 0;
    long long set = 0;
    unsigned char *data = NULL;
    size_t size = 0;
    const char *type = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error) ||
        !workshop_param_require_string(call->params, "name", &name, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!read_version(call->params, "version", VERSION_MAX, &version) || !read_version(call->params, "set", VERSION_MAX, &set) ||
        workshop_piece_variation_file(id, (int)version, (int)set, name, &data, &size, &type) != URSATZ_OK) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "", "file not found");
    }
    workshop_reply_take_bytes(reply, data, size, type);
    snprintf(reply->filename, sizeof(reply->filename), "%s", name);
    return URSATZ_OK;
}

/* The playback of a variation's MIDI file (variation-<i>.mid): params id, version, set and name. */
WORKSHOP_HANDLER(workshop_q_item_variation_playback) {
    const char *id = NULL;
    const char *name = NULL;
    long long version = 0;
    long long set = 0;
    char path[640];
    if (!workshop_param_require_string(call->params, "id", &id, error) ||
        !workshop_param_require_string(call->params, "name", &name, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!read_version(call->params, "version", VERSION_MAX, &version) || !read_version(call->params, "set", VERSION_MAX, &set) ||
        workshop_piece_variation_midi_path(id, (int)version, (int)set, name, path, sizeof(path)) != URSATZ_OK) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "", "file not found");
    }
    char *json = NULL;
    UrsatzStatus built = workshop_playback_of_midi(path, &json);
    cJSON *playback = built == URSATZ_OK ? cJSON_Parse(json) : NULL;
    free(json);
    if (!playback) {
        return workshop_call_fail(error, built == URSATZ_ERR_ALLOC ? built : URSATZ_ERR_INTERNAL, "", "%s",
                                  built == URSATZ_ERR_ALLOC ? "out of memory" : "could not build the playback from the MIDI file");
    }
    return workshop_reply_tree(reply, playback, error);
}

/* The marks of a motif from Try every operation: {"checks": {...}} (R-190). */
WORKSHOP_HANDLER(workshop_q_item_op_checks) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *checks = NULL;
    UrsatzStatus result = workshop_material_op_checks_get(id, &checks);
    return result == URSATZ_OK ? workshop_reply_tree(reply, checks, error) : material_failure(error, result);
}

/* The item and, recursively, every motif derived from it. */
WORKSHOP_HANDLER(workshop_q_item_variant_tree) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *tree = NULL;
    UrsatzStatus result = workshop_variants_tree(id, &tree, error);
    return result == URSATZ_OK ? workshop_reply_member(reply, "tree", tree, error) : result;
}

/* Candidates from the item's current degrees; nothing is saved. params id and the request members (chain, count, seed, functions,
 * distance); the reply is the job. */
static UrsatzStatus variants_job(const WorkshopCall *call, WorkshopReply *reply, UrsatzError *error, WorkshopVariantsOp op) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *body = cJSON_Duplicate(call->params, true); /* the job takes it */
    WorkshopVariantsJob *job = NULL;
    UrsatzStatus result = body ? workshop_variants_job_new(op, id, body, &job, error) : URSATZ_ERR_ALLOC;
    if (result != URSATZ_OK) {
        return result == URSATZ_ERR_ALLOC ? workshop_call_fail(error, result, "", "out of memory") : result;
    }
    return workshop_reply_job(reply, NULL, workshop_variants_job_kind(job), workshop_variants_job_run, job, workshop_variants_job_free, error);
}

WORKSHOP_HANDLER(workshop_q_item_variants) {
    return variants_job(call, reply, error, WORKSHOP_VARIANTS_VARIANTS);
}

WORKSHOP_HANDLER(workshop_q_item_each_op) {
    return variants_job(call, reply, error, WORKSHOP_VARIANTS_EACH_OP);
}

/* {"complete": false, "message"}: the body is not composable yet, which is no refusal. */
static UrsatzStatus incomplete(WorkshopReply *reply, const char *message, UrsatzError *error) {
    cJSON *object = workshop_reply_object(reply);
    if (!object || !cJSON_AddBoolToObject(object, "complete", 0) || !cJSON_AddStringToObject(object, "message", message ? message : "")) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return URSATZ_OK;
}

/*
 * params type ("motif", "theme" or "piece") and body: the Composer reads the request the body would compose (or analyze) with and
 * composes nothing (R-276). The reply is {"complete": true, "request"} with the Composer's defaults filled, {"complete": false,
 * "message"} when the body is not composable yet, or the refusal with the request member at fault as its path.
 */
WORKSHOP_HANDLER(workshop_q_item_validate) {
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(call->params, "type");
    const cJSON *body = cJSON_GetObjectItemCaseSensitive(call->params, "body");
    bool is_motif = cJSON_IsString(type) && strcmp(type->valuestring, "motif") == 0;
    bool is_theme = cJSON_IsString(type) && strcmp(type->valuestring, "theme") == 0;
    bool is_piece = cJSON_IsString(type) && strcmp(type->valuestring, "piece") == 0;
    if (!(is_motif || is_theme || is_piece) || !cJSON_IsObject(body)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "type is motif, theme or piece and body an object");
    }
    cJSON *request = NULL;
    UrsatzError failure = {{0}, {0}};
    bool complete = false;
    UrsatzStatus built = URSATZ_OK;
    if (is_motif) {
        const cJSON *motif = cJSON_GetObjectItemCaseSensitive(body, "motif");
        const cJSON *form = cJSON_GetObjectItemCaseSensitive(motif, "form");
        const cJSON *notes = cJSON_GetObjectItemCaseSensitive(motif, "notes");
        bool draft = !cJSON_IsObject(motif) || !cJSON_IsString(form) || !cJSON_IsString(notes) || notes->valuestring[0] == '\0' ||
                     (strcmp(form->valuestring, "full") != 0 && strcmp(form->valuestring, "rhythm") != 0 &&
                      strcmp(form->valuestring, "contour") != 0);
        request = draft ? NULL : workshop_motif_request(motif);
        complete = request != NULL;
        if (!draft && !request) {
            workshop_error_printf(&failure, "the motif needs notes, meter and pickup");
        }
    } else {
        /* A form not saved yet names its motifs and themes by reference; the uses a save would make are made here. */
        cJSON *filled = NULL;
        UrsatzStatus uses = workshop_material_body_with_uses(body, &filled);
        if (uses == URSATZ_ERR_ALLOC) {
            built = URSATZ_ERR_ALLOC;
        } else if (uses != URSATZ_OK) {
            workshop_error_printf(&failure, "a motif or theme the body names cannot be used yet");
        } else if (is_theme) {
            built = workshop_theme_validate(filled, &complete, &request, &failure);
        } else {
            built = workshop_piece_validate(filled, &complete, &request, &failure);
        }
        cJSON_Delete(filled);
    }
    if (built != URSATZ_OK) {
        cJSON_Delete(request);
        return workshop_call_fail(error, built == URSATZ_ERR_REQUEST ? built : (built == URSATZ_ERR_ALLOC ? built : URSATZ_ERR_INTERNAL), "",
                                  "%s", built == URSATZ_ERR_ALLOC ? "out of memory"
                                        : failure.message[0] ? failure.message : "the request could not be built");
    }
    if (!complete) {
        return incomplete(reply, failure.message, error);
    }
    WorkshopComposerReply composer;
    bool called = workshop_motif_validate(is_motif ? "motive" : type->valuestring, request, &composer);
    cJSON_Delete(request);
    if (!called) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    cJSON *document = composer.status == URSATZ_OK && composer.json ? cJSON_Parse(composer.json) : NULL;
    cJSON *filled = document ? cJSON_DetachItemFromObjectCaseSensitive(document, "request") : NULL;
    cJSON_Delete(document);
    if (!filled) {
        UrsatzStatus status = composer.status != URSATZ_OK ? composer.status : URSATZ_ERR_INTERNAL;
        workshop_call_fail(error, status, composer.path, "%s", composer.message[0] ? composer.message : "composer could not check the request");
        workshop_composer_reply_destroy(&composer);
        return status;
    }
    workshop_composer_reply_destroy(&composer);
    cJSON *answer = workshop_reply_object(reply);
    if (!answer || !cJSON_AddBoolToObject(answer, "complete", 1) || !cJSON_AddItemToObject(answer, "request", filled)) {
        cJSON_Delete(filled);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return URSATZ_OK;
}

/* --- subjects ---------------------------------------------------------------------------------------------------------------- */

static UrsatzStatus subject_failure(UrsatzError *error, UrsatzStatus status) {
    return workshop_call_fail_service(error, status, "subject store failure");
}

WORKSHOP_HANDLER(workshop_q_subject_list) {
    (void)call;
    cJSON *subjects = NULL;
    UrsatzStatus result = workshop_subject_list(&subjects);
    return result == URSATZ_OK ? workshop_reply_member(reply, "subjects", subjects, error) : subject_failure(error, result);
}

WORKSHOP_HANDLER(workshop_q_subject_groups) {
    (void)call;
    cJSON *groups = NULL;
    UrsatzStatus result = workshop_subject_groups(&groups);
    return result == URSATZ_OK ? workshop_reply_member(reply, "groups", groups, error) : subject_failure(error, result);
}

WORKSHOP_HANDLER(workshop_q_subject_get) {
    const char *id = NULL;
    if (!workshop_param_require_string(call->params, "id", &id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *subject = NULL;
    UrsatzStatus result = workshop_subject_get(id, &subject);
    return result == URSATZ_OK ? workshop_reply_member(reply, "subject", subject, error) : subject_failure(error, result);
}
