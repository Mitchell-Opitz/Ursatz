#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "coordinator/dispatch.h"

/* The commands over the Library, collections, styles, motive links and jobs (R-646). Each is one change of the data folder, journaled
 * under its name; the long ones answer with the job that makes it. */

/* The target of a command, or the failure naming /target. */
static bool require_target(const WorkshopCall *call, UrsatzError *error) {
    if (call->target[0] == '\0') {
        workshop_call_fail(error, URSATZ_ERR_REQUEST, "/target", "the command needs a target");
        return false;
    }
    return true;
}

/* --- the Library ----------------------------------------------------------------------------------------------------------------- */

/* Imports the file in bytes (a MIDI, MusicXML or compressed MusicXML file) as a Library piece; params filename. The reply is the job. */
WORKSHOP_HANDLER(workshop_c_library_import) {
    if (!call->bytes || !call->bytes->data) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/bytes", "failed to read request body");
    }
    bool bad = false;
    const char *filename = workshop_param_string(call->params, "filename", &bad);
    if (bad) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/filename", "filename is a string");
    }
    WorkshopAnalysisJob *job = workshop_analysis_job_import(call->bytes->data, call->bytes->size, filename ? filename : "");
    if (!job) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return workshop_reply_job(reply, NULL, workshop_analysis_job_kind(job), workshop_analysis_job_run, job, workshop_analysis_job_free, error);
}

/* Changes a piece's filename, composer, source, tags or notation (params as workshop_library_patch reads them); the reply is the piece. */
WORKSHOP_HANDLER(workshop_c_library_update) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *piece = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus result = workshop_library_patch(WORKSHOP_DIR_PIECES, call->target, call->params, &piece, &failure);
    switch (result) {
        case URSATZ_OK:
            return workshop_reply_member(reply, "piece", piece, error);
        case URSATZ_ERR_NOT_FOUND:
            return workshop_call_fail(error, result, "", "piece not found");
        case URSATZ_ERR_REQUEST:
            return workshop_call_fail(error, result, failure.path, "%s", strcmp(failure.path, "filename") == 0 ? "invalid filename" : failure.message);
        case URSATZ_ERR_UNSATISFIABLE:
            return workshop_call_fail(error, result, "", "nothing to update");
        default:
            return workshop_call_fail(error, result, "", "failed to update piece");
    }
}

WORKSHOP_HANDLER(workshop_c_library_delete) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    UrsatzStatus result = workshop_library_delete(WORKSHOP_DIR_PIECES, call->target);
    if (result != URSATZ_OK) {
        return workshop_call_fail(error, result, "", result == URSATZ_ERR_NOT_FOUND ? "piece not found" : "failed to delete piece");
    }
    return workshop_reply_object(reply) ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

/* The Analyzer again on a piece's stored source; the reply is the job. */
WORKSHOP_HANDLER(workshop_c_library_reanalyze) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    WorkshopAnalysisJob *job = workshop_analysis_job_reanalyze(call->target);
    if (!job) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return workshop_reply_job(reply, NULL, workshop_analysis_job_kind(job), workshop_analysis_job_run, job, workshop_analysis_job_free, error);
}

/* --- collections ----------------------------------------------------------------------------------------------------------------- */

static UrsatzStatus collection_failure(UrsatzError *error, UrsatzStatus status) {
    return workshop_call_fail_service(error, status, "collection operation failed");
}

/* The reply is the collection as stored: id, name, description, piece_count, updated_at, so an answer shows the normalized values. */
static UrsatzStatus collection_summary_reply(WorkshopReply *reply, const char *id, UrsatzError *error) {
    WorkshopCollection *collection = NULL;
    UrsatzStatus result = workshop_collection_get(id, &collection);
    if (result != URSATZ_OK) {
        return collection_failure(error, result);
    }
    cJSON *object = workshop_reply_object(reply);
    bool built = object && cJSON_AddStringToObject(object, "id", collection->id) && cJSON_AddStringToObject(object, "name", collection->name) &&
                 cJSON_AddStringToObject(object, "description", collection->description) &&
                 cJSON_AddNumberToObject(object, "piece_count", (double)collection->piece_count) &&
                 cJSON_AddStringToObject(object, "updated_at", collection->updated_at);
    workshop_collection_destroy(collection);
    return built ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

/* A collection of params {name, description?, id?}. */
WORKSHOP_HANDLER(workshop_c_collection_create) {
    char *id = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus result = workshop_collection_create_request(call->params, &id, &failure);
    if (result != URSATZ_OK) {
        return workshop_call_fail(error, result, failure.path, "%s", failure.message);
    }
    result = collection_summary_reply(reply, id, error);
    free(id);
    return result;
}

WORKSHOP_HANDLER(workshop_c_collection_update) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!workshop_collection_is_valid_id(call->target)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/target", "invalid id");
    }
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(call->params, "name");
    const cJSON *description = cJSON_GetObjectItemCaseSensitive(call->params, "description");
    if (!name && !description) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "nothing to update");
    }
    if (name && !cJSON_IsString(name)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/name", "invalid name");
    }
    if (description && !cJSON_IsString(description)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/description", "invalid description");
    }
    UrsatzStatus result = workshop_collection_update(call->target, name ? name->valuestring : NULL, description ? description->valuestring : NULL);
    return result == URSATZ_OK ? collection_summary_reply(reply, call->target, error) : collection_failure(error, result);
}

WORKSHOP_HANDLER(workshop_c_collection_delete) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!workshop_collection_is_valid_id(call->target)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/target", "invalid id");
    }
    UrsatzStatus result = workshop_collection_delete(call->target);
    if (result != URSATZ_OK) {
        return collection_failure(error, result);
    }
    return workshop_reply_object(reply) ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

/* The pieces of params piece_id or piece_ids join the collection; one already in it is counted, not an error: {"added", "already"}. */
WORKSHOP_HANDLER(workshop_c_collection_add_pieces) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!workshop_collection_is_valid_id(call->target)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/target", "invalid id");
    }
    size_t added = 0;
    size_t already = 0;
    int http_status = 200;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus result = workshop_collection_add_request(call->target, call->params, &added, &already, &http_status, &failure);
    if (result != URSATZ_OK) {
        return workshop_call_fail(error, result, failure.path, "%s", failure.message);
    }
    cJSON *object = workshop_reply_object(reply);
    if (!object || !cJSON_AddNumberToObject(object, "added", (double)added) || !cJSON_AddNumberToObject(object, "already", (double)already)) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return URSATZ_OK;
}

/* params piece_id leaves the collection. */
WORKSHOP_HANDLER(workshop_c_collection_remove_piece) {
    const char *piece_id = NULL;
    if (!require_target(call, error) || !workshop_param_require_string(call->params, "piece_id", &piece_id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!workshop_collection_is_valid_id(call->target)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/target", "invalid id");
    }
    if (!workshop_collection_is_valid_id(piece_id)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/piece_id", "invalid piece_id");
    }
    UrsatzStatus result = workshop_collection_remove_piece(call->target, piece_id);
    if (result != URSATZ_OK) {
        return collection_failure(error, result);
    }
    return workshop_reply_object(reply) ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

/* --- styles ---------------------------------------------------------------------------------------------------------------------- */

static UrsatzStatus style_failure(UrsatzError *error, UrsatzStatus status) {
    return workshop_call_fail_service(error, status, "style operation failed");
}

/* A style built from the pieces of params (the request workshop_style_build_request_parse reads); a "style" job, and a second build waits. The
 * request is read here, so a bad one is refused at once. */
WORKSHOP_HANDLER(workshop_c_style_build) {
    cJSON *body = cJSON_Duplicate(call->params, true); /* the job takes it */
    if (!body) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    WorkshopStyleRequest request;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus parsed = workshop_style_build_request_parse(body, &request, &failure);
    if (parsed != URSATZ_OK) {
        cJSON_Delete(body);
        return workshop_call_fail(error, parsed, failure.path, "%s", failure.message);
    }
    WorkshopAnalysisJob *job = workshop_analysis_job_style_build(&request, body);
    if (!job) {
        workshop_style_request_free(&request);
        cJSON_Delete(body);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return workshop_reply_job(reply, NULL, workshop_analysis_job_kind(job), workshop_analysis_job_run, job, workshop_analysis_job_free, error);
}

WORKSHOP_HANDLER(workshop_c_style_rebuild) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!workshop_collection_is_valid_id(call->target)) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "", "style not found");
    }
    WorkshopAnalysisJob *job = workshop_analysis_job_style_rebuild(call->target);
    if (!job) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return workshop_reply_job(reply, NULL, workshop_analysis_job_kind(job), workshop_analysis_job_run, job, workshop_analysis_job_free, error);
}

WORKSHOP_HANDLER(workshop_c_style_delete) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!workshop_collection_is_valid_id(call->target)) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "", "style not found");
    }
    UrsatzStatus result = workshop_style_delete(call->target);
    if (result != URSATZ_OK) {
        return style_failure(error, result);
    }
    return workshop_reply_object(reply) ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

/* --- motive links ---------------------------------------------------------------------------------------------------------------- */

static UrsatzStatus link_failure(UrsatzError *error, UrsatzStatus status) {
    return workshop_call_fail_service(error, status, "link operation failed");
}

/* A run over the pieces of params scope; the reply is {"run", "job"}: the run is queued behind an earlier one. */
WORKSHOP_HANDLER(workshop_c_links_run) {
    WorkshopLinkScope scope;
    if (!workshop_link_scope_parse(cJSON_GetObjectItemCaseSensitive(call->params, "scope"), &scope)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/scope", "invalid scope");
    }
    char *id = NULL;
    char job_id[WORKSHOP_JOB_ID_BYTES];
    UrsatzStatus result = workshop_link_run_start(&scope, &id, job_id);
    if (result != URSATZ_OK) {
        return link_failure(error, result);
    }
    cJSON *run = NULL;
    result = workshop_link_run_get(id, &run);
    free(id);
    if (result != URSATZ_OK) {
        return link_failure(error, result);
    }
    cJSON *object = workshop_reply_object(reply);
    cJSON *job = object ? cJSON_AddObjectToObject(object, "job") : NULL;
    if (!job || !cJSON_AddItemToObject(object, "run", run) || !cJSON_AddStringToObject(job, "id", job_id) ||
        !cJSON_AddStringToObject(job, "kind", "links") ||
        !cJSON_AddStringToObject(job, "state", workshop_job_state_name(WORKSHOP_JOB_QUEUED))) {
        if (!object) {
            cJSON_Delete(run);
        }
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return URSATZ_OK;
}

/* A run and its files leave the data folder; the run's job is cancelled first. */
WORKSHOP_HANDLER(workshop_c_links_delete) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!workshop_collection_is_valid_id(call->target)) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "", "run not found");
    }
    UrsatzStatus result = workshop_link_run_delete(call->target);
    if (result != URSATZ_OK) {
        return result == URSATZ_ERR_NOT_FOUND ? workshop_call_fail(error, result, "", "run not found") : link_failure(error, result);
    }
    return workshop_reply_object(reply) ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

/* --- jobs ------------------------------------------------------------------------------------------------------------------------ */

/* Asks the job to stop: a queued one is cancelled at once, a running one at the Composer's next report. A runtime control, not journaled. */
WORKSHOP_HANDLER(workshop_c_job_cancel) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (strlen(call->target) >= WORKSHOP_JOB_ID_BYTES || !workshop_job_cancel(call->target)) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "", "job not found");
    }
    return workshop_reply_object(reply) ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}
