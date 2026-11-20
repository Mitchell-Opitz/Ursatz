#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/json.h>

#include "coordinator/dispatch.h"

/* The commands over projects, items and subjects (R-646). */

static bool require_target(const WorkshopCall *call, UrsatzError *error) {
    if (call->target[0] == '\0') {
        workshop_call_fail(error, URSATZ_ERR_REQUEST, "/target", "the command needs a target");
        return false;
    }
    return true;
}

static UrsatzStatus material_failure(UrsatzError *error, UrsatzStatus status) {
    return workshop_call_fail_service(error, status, "material store failure");
}

static UrsatzStatus empty_reply(WorkshopReply *reply, UrsatzError *error) {
    return workshop_reply_object(reply) ? URSATZ_OK : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

/* --- projects -------------------------------------------------------------------------------------------------------------------- */

WORKSHOP_HANDLER(workshop_c_project_create) {
    bool bad = false;
    const char *name = workshop_param_string(call->params, "name", &bad);
    const char *description = workshop_param_string(call->params, "description", &bad);
    if (bad || !name) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "name is required; name and description are strings");
    }
    cJSON *project = NULL;
    UrsatzStatus result = workshop_material_project_create(name, description, &project);
    return result == URSATZ_OK ? workshop_reply_member(reply, "project", project, error) : material_failure(error, result);
}

WORKSHOP_HANDLER(workshop_c_project_update) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    bool bad = false;
    const char *name = workshop_param_string(call->params, "name", &bad);
    const char *description = workshop_param_string(call->params, "description", &bad);
    if (bad || (!name && !description)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "give name and/or description as strings");
    }
    cJSON *project = NULL;
    UrsatzStatus result = workshop_material_project_update(call->target, name, description, &project);
    return result == URSATZ_OK ? workshop_reply_member(reply, "project", project, error) : material_failure(error, result);
}

WORKSHOP_HANDLER(workshop_c_project_delete) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    UrsatzStatus result = workshop_material_project_delete(call->target);
    return result == URSATZ_OK ? empty_reply(reply, error) : material_failure(error, result);
}

/* --- items ----------------------------------------------------------------------------------------------------------------------- */

/* The item answer: {"item": ..., "analysis_error"?} where the Composer was unavailable. */
static UrsatzStatus item_reply(WorkshopReply *reply, cJSON *item, const char *analysis_error, UrsatzError *error) {
    if (item && analysis_error[0]) {
        cJSON_AddStringToObject(item, "analysis_error", analysis_error);
    }
    return workshop_reply_member(reply, "item", item, error);
}

/* The Composer's refusal of a motif body, or an allocation failure, as the failure of the command. */
static UrsatzStatus analysis_failure(WorkshopMotifAnalysis analysis, const char *refusal, const char *path, UrsatzError *error) {
    return analysis == WORKSHOP_MOTIF_ANALYSIS_REFUSED ? workshop_call_fail(error, URSATZ_ERR_REQUEST, path, "%s", refusal)
                                                       : workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
}

/* A new item of params type, name, project_id, body and note?; a motif's body is analyzed first. */
WORKSHOP_HANDLER(workshop_c_item_create) {
    bool bad = false;
    const char *type = workshop_param_string(call->params, "type", &bad);
    const char *name = workshop_param_string(call->params, "name", &bad);
    const char *project_id = workshop_param_string(call->params, "project_id", &bad);
    const char *note = workshop_param_string(call->params, "note", &bad);
    const cJSON *params_body = cJSON_GetObjectItemCaseSensitive(call->params, "body");
    if (bad || !type || !name || !project_id || !cJSON_IsObject(params_body)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "type, name, project_id and an object body are required; note is a string");
    }
    cJSON *version_body = cJSON_Duplicate(params_body, true);
    if (!version_body) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    if (!workshop_motif_tonic_ok(version_body)) {
        cJSON_Delete(version_body);
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/body", "motifs are written with C as the tonic");
    }
    char analysis_error[WORKSHOP_MOTIF_ANALYSIS_ERROR_SIZE];
    char refusal[512] = "";
    char refusal_path[128] = "";
    WorkshopMotifAnalysis analysis =
        workshop_motif_analyze_body(version_body, NULL, analysis_error, refusal, sizeof(refusal), refusal_path, sizeof(refusal_path));
    if (analysis != WORKSHOP_MOTIF_ANALYSIS_DONE) {
        cJSON_Delete(version_body);
        return analysis_failure(analysis, refusal, refusal_path, error);
    }
    cJSON *item = NULL;
    /* A new motif's or theme's first version is noted "Created" unless the caller gives a note. */
    const char *first_note = workshop_motif_first_note(type, note);
    UrsatzStatus result = workshop_material_item_create_noted(type, name, project_id, version_body, first_note, &item);
    cJSON_Delete(version_body);
    return result == URSATZ_OK ? item_reply(reply, item, analysis_error, error) : material_failure(error, result);
}

/* Renames an item and/or moves it to another project: params name and project_id. */
WORKSHOP_HANDLER(workshop_c_item_update) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    bool bad = false;
    const char *name = workshop_param_string(call->params, "name", &bad);
    const char *project_id = workshop_param_string(call->params, "project_id", &bad);
    if (bad || (!name && !project_id)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "give name and/or project_id as strings");
    }
    /* The project is checked up front, so a bad second field cannot leave the rename applied. */
    cJSON *item = NULL;
    UrsatzStatus result = URSATZ_OK;
    if (project_id) {
        cJSON *project = NULL;
        result = workshop_material_project_get(project_id, &project);
        cJSON_Delete(project);
    }
    if (result == URSATZ_OK && name) {
        result = workshop_material_item_rename(call->target, name, &item);
    }
    if (result == URSATZ_OK && project_id) {
        cJSON_Delete(item);
        item = NULL;
        result = workshop_material_item_move(call->target, project_id, &item);
    }
    if (result != URSATZ_OK) {
        cJSON_Delete(item);
        return material_failure(error, result);
    }
    return workshop_reply_member(reply, "item", item, error);
}

/* Deletes an item and its variants; params force (true, or "1") deletes it although other items use it. A refusal lists them as used_by. */
WORKSHOP_HANDLER(workshop_c_item_delete) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    const cJSON *force = cJSON_GetObjectItemCaseSensitive(call->params, "force");
    bool forced = false;
    if (!common_json_bool(call->params, "force", &forced)) {
        forced = cJSON_IsString(force) && strcmp(force->valuestring, "1") == 0; /* the route's ?force=1 */
    }
    UrsatzStatus result = workshop_material_item_delete(call->target, forced);
    if (result == URSATZ_ERR_CONFLICT) {
        UrsatzStatus failed = material_failure(error, result);
        cJSON *used_by = NULL;
        if (workshop_material_delete_blockers(call->target, &used_by) == URSATZ_OK) {
            workshop_reply_member(reply, "used_by", used_by, NULL);
        }
        return failed;
    }
    return result == URSATZ_OK ? empty_reply(reply, error) : material_failure(error, result);
}

/* A new version of an item: params body and note (every version of a motif or a theme says what changed). */
WORKSHOP_HANDLER(workshop_c_item_save) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    bool bad = false;
    const char *note = workshop_param_string(call->params, "note", &bad);
    const cJSON *params_body = cJSON_GetObjectItemCaseSensitive(call->params, "body");
    if (bad || !cJSON_IsObject(params_body)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "an object body is required; note is a string");
    }
    if (!workshop_motif_version_note_ok(params_body, note)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/note", "give a version note");
    }
    if (!workshop_motif_tonic_ok(params_body)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/body", "motifs are written with C as the tonic");
    }
    cJSON *version_body = cJSON_Duplicate(params_body, true);
    if (!version_body) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    char analysis_error[WORKSHOP_MOTIF_ANALYSIS_ERROR_SIZE];
    char refusal[512] = "";
    char refusal_path[128] = "";
    /* The previous version tells whether proposed segments are to be proposed again (R-312). */
    cJSON *previous_meta = NULL;
    cJSON *previous_body = NULL;
    if (workshop_material_item_get(call->target, &previous_meta, &previous_body) != URSATZ_OK) {
        previous_body = NULL;
    }
    cJSON_Delete(previous_meta);
    WorkshopMotifAnalysis analysis = workshop_motif_analyze_body(version_body, previous_body, analysis_error, refusal, sizeof(refusal),
                                                                 refusal_path, sizeof(refusal_path));
    cJSON_Delete(previous_body);
    if (analysis != WORKSHOP_MOTIF_ANALYSIS_DONE) {
        cJSON_Delete(version_body);
        return analysis_failure(analysis, refusal, refusal_path, error);
    }
    cJSON *item = NULL;
    UrsatzStatus result = workshop_material_item_add_version(call->target, version_body, note, &item);
    cJSON_Delete(version_body);
    return result == URSATZ_OK ? item_reply(reply, item, analysis_error, error) : material_failure(error, result);
}

/* Analyzes the current version of a motif again and saves the result as a new version (versions are never rewritten, R-130). A refusal
 * writes nothing. */
WORKSHOP_HANDLER(workshop_c_item_reanalyze) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *meta = NULL;
    cJSON *version_body = NULL;
    UrsatzStatus result = workshop_material_item_get(call->target, &meta, &version_body);
    if (result != URSATZ_OK) {
        return material_failure(error, result);
    }
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(meta, "type");
    bool motif = cJSON_IsString(type) && strcmp(type->valuestring, "motif") == 0;
    cJSON_Delete(meta);
    if (!motif) {
        cJSON_Delete(version_body);
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/target", "only motifs are analyzed");
    }
    char analysis_error[WORKSHOP_MOTIF_ANALYSIS_ERROR_SIZE];
    char refusal[512] = "";
    char refusal_path[128] = "";
    WorkshopMotifAnalysis analysis =
        workshop_motif_analyze_body(version_body, NULL, analysis_error, refusal, sizeof(refusal), refusal_path, sizeof(refusal_path));
    if (analysis != WORKSHOP_MOTIF_ANALYSIS_DONE) {
        cJSON_Delete(version_body);
        return analysis_failure(analysis, refusal, refusal_path, error);
    }
    cJSON *item = NULL;
    result = workshop_material_item_add_version(call->target, version_body, "Re-analyzed", &item);
    cJSON_Delete(version_body);
    return result == URSATZ_OK ? item_reply(reply, item, analysis_error, error) : material_failure(error, result);
}

/* An item's use of a source moves to the source's current version. params source_id. */
WORKSHOP_HANDLER(workshop_c_item_update_use) {
    const char *source_id = NULL;
    if (!require_target(call, error) || !workshop_param_require_string(call->params, "source_id", &source_id, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *item = NULL;
    UrsatzStatus result = workshop_material_update_use(call->target, source_id, &item);
    return result == URSATZ_OK ? workshop_reply_member(reply, "item", item, error) : material_failure(error, result);
}

/* --- compose jobs ---------------------------------------------------------------------------------------------------------------- */

/* The optional member seed: a whole number from 0 to 4294967295. */
static bool read_seed(const cJSON *params, bool *has_seed, long long *seed, UrsatzError *error) {
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(params, "seed");
    *has_seed = member && !cJSON_IsNull(member);
    if (!*has_seed) {
        return true;
    }
    if (!cJSON_IsNumber(member) || member->valuedouble < 0 || member->valuedouble > 4294967295.0 ||
        member->valuedouble != (double)(long long)member->valuedouble) {
        workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/seed", "seed is a whole number from 0 to 4294967295");
        return false;
    }
    *seed = (long long)member->valuedouble;
    return true;
}

static UrsatzStatus submit_compose(WorkshopReply *reply, WorkshopComposeJob *job, WorkshopComposeOp op, UrsatzError *error) {
    return workshop_reply_job(reply, NULL, workshop_compose_job_kind(op), workshop_compose_job_run, job, workshop_compose_job_free, error);
}

/* Composes the theme or piece into a new version with its files (R-167, R-174): params seed? and update_uses? (every updated use moves to its
 * source's current version first). The reply is the job. */
WORKSHOP_HANDLER(workshop_c_item_compose) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    const cJSON *update_uses = cJSON_GetObjectItemCaseSensitive(call->params, "update_uses");
    bool has_seed = false;
    long long seed = 0;
    if (!read_seed(call->params, &has_seed, &seed, error) || (update_uses && !cJSON_IsBool(update_uses))) {
        if (!error->message[0] || update_uses) {
            workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "seed is a whole number from 0 to 4294967295; update_uses is a boolean");
        }
        return URSATZ_ERR_REQUEST;
    }
    cJSON *meta = NULL;
    cJSON *stored = NULL;
    UrsatzStatus found = workshop_material_item_get(call->target, &meta, &stored);
    if (found != URSATZ_OK) {
        return material_failure(error, found);
    }
    const cJSON *type = cJSON_GetObjectItemCaseSensitive(meta, "type");
    bool piece = cJSON_IsString(type) && strcmp(type->valuestring, "piece") == 0;
    cJSON_Delete(meta);
    cJSON_Delete(stored);
    WorkshopComposeOp op = piece ? WORKSHOP_COMPOSE_PIECE : WORKSHOP_COMPOSE_THEME;
    WorkshopComposeJob *job = workshop_compose_job_new(op, call->target);
    if (!job) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    job->has_seed = has_seed;
    job->seed = seed;
    job->update_uses = cJSON_IsTrue(update_uses);
    return submit_compose(reply, job, op, error);
}

/* The version number params names: a whole number from 1 to 1000000. */
static bool read_version(const cJSON *params, const char *name, int *out, UrsatzError *error) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(params, name);
    if (!cJSON_IsNumber(item) || item->valuedouble < 1 || item->valuedouble > 1000000 || item->valuedouble != (double)(int)item->valuedouble) {
        char path[96];
        snprintf(path, sizeof(path), "/params/%s", name);
        workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, path, "%s not found", strcmp(name, "set") == 0 ? "variations" : "version");
        return false;
    }
    *out = (int)item->valuedouble;
    return true;
}

/* Writes one section of a composed piece version again, as the next version (R-173, R-174): params version, section_id, lane?, seed?. */
WORKSHOP_HANDLER(workshop_c_item_regenerate) {
    int version = 0;
    if (!require_target(call, error) || !read_version(call->params, "version", &version, error)) {
        return error->message[0] ? error->path[0] ? URSATZ_ERR_NOT_FOUND : URSATZ_ERR_REQUEST : URSATZ_ERR_REQUEST;
    }
    const cJSON *section = cJSON_GetObjectItemCaseSensitive(call->params, "section_id");
    const cJSON *lane = cJSON_GetObjectItemCaseSensitive(call->params, "lane");
    long long seed = 0;
    bool has_seed = false;
    if (!cJSON_IsString(section) || !section->valuestring[0] || (lane && !cJSON_IsNull(lane) && !cJSON_IsString(lane)) ||
        !read_seed(call->params, &has_seed, &seed, error)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params",
                                  "section_id is a string; lane is a string; seed is a whole number from 0 to 4294967295");
    }
    WorkshopComposeJob *job = workshop_compose_job_new(WORKSHOP_COMPOSE_PIECE_REGENERATE, call->target);
    if (job) {
        job->version = version;
        job->section = strdup(section->valuestring);
        job->lane = cJSON_IsString(lane) ? strdup(lane->valuestring) : NULL;
        job->has_seed = has_seed;
        job->seed = seed;
    }
    if (!job || !job->section || (cJSON_IsString(lane) && !job->lane)) {
        workshop_compose_job_free(job);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return submit_compose(reply, job, WORKSHOP_COMPOSE_PIECE_REGENERATE, error);
}

/* Makes variations of a section of a composed piece version into a new set: params version, section_id, lane, count (1 to 8), seed?. */
WORKSHOP_HANDLER(workshop_c_item_variations) {
    int version = 0;
    if (!require_target(call, error) || !read_version(call->params, "version", &version, error)) {
        return error->message[0] ? URSATZ_ERR_NOT_FOUND : URSATZ_ERR_REQUEST;
    }
    const cJSON *section = cJSON_GetObjectItemCaseSensitive(call->params, "section_id");
    const cJSON *lane = cJSON_GetObjectItemCaseSensitive(call->params, "lane");
    const cJSON *count = cJSON_GetObjectItemCaseSensitive(call->params, "count");
    long long seed = 0;
    bool has_seed = false;
    if (!cJSON_IsString(section) || !section->valuestring[0] || !cJSON_IsString(lane) || !cJSON_IsNumber(count) || count->valuedouble < 1 ||
        count->valuedouble > 8 || count->valuedouble != (double)(int)count->valuedouble || !read_seed(call->params, &has_seed, &seed, error)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params",
                                  "section_id and lane are strings; count is a whole number from 1 to 8; seed is a whole number from 0 to 4294967295");
    }
    WorkshopComposeJob *job = workshop_compose_job_new(WORKSHOP_COMPOSE_PIECE_VARIATIONS, call->target);
    if (job) {
        job->version = version;
        job->section = strdup(section->valuestring);
        job->lane = strdup(lane->valuestring);
        job->count = (int)count->valuedouble;
        job->has_seed = has_seed;
        job->seed = seed;
    }
    if (!job || !job->section || !job->lane) {
        workshop_compose_job_free(job);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    return submit_compose(reply, job, WORKSHOP_COMPOSE_PIECE_VARIATIONS, error);
}

/* Makes a variation of a set the next version: params version, set, index (from 1). */
WORKSHOP_HANDLER(workshop_c_item_choose) {
    int version = 0;
    int set = 0;
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    if (!read_version(call->params, "version", &version, error) || !read_version(call->params, "set", &set, error)) {
        return workshop_call_fail(error, URSATZ_ERR_NOT_FOUND, "", "variations not found");
    }
    const cJSON *index = cJSON_GetObjectItemCaseSensitive(call->params, "index");
    if (!cJSON_IsNumber(index) || index->valuedouble < 1 || index->valuedouble > 99 || index->valuedouble != (double)(int)index->valuedouble) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/index", "index is a whole number from 1");
    }
    WorkshopComposeJob *job = workshop_compose_job_new(WORKSHOP_COMPOSE_PIECE_CHOOSE, call->target);
    if (!job) {
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    job->version = version;
    job->set = set;
    job->index = (int)index->valuedouble;
    return submit_compose(reply, job, WORKSHOP_COMPOSE_PIECE_CHOOSE, error);
}

/* --- the motif's own commands ---------------------------------------------------------------------------------------------------- */

/* Reruns the stored chain on the source's stored version with a new seed and stores the result as a new version; the reply is the job. */
WORKSHOP_HANDLER(workshop_c_item_reroll) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    WorkshopVariantsJob *job = NULL;
    UrsatzStatus result = workshop_variants_job_new(WORKSHOP_VARIANTS_REROLL, call->target, NULL, &job, error);
    if (result != URSATZ_OK) {
        return result;
    }
    return workshop_reply_job(reply, NULL, workshop_variants_job_kind(job), workshop_variants_job_run, job, workshop_variants_job_free, error);
}

/* A request body in, the item out: the four variant commands. */
static UrsatzStatus variant_command(const WorkshopCall *call, WorkshopReply *reply, UrsatzError *error,
                                    UrsatzStatus (*command)(const char *, const cJSON *, cJSON **, UrsatzError *)) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *item = NULL;
    UrsatzStatus result = command(call->target, call->params, &item, error);
    return result == URSATZ_OK ? workshop_reply_member(reply, "item", item, error) : result;
}

WORKSHOP_HANDLER(workshop_c_item_save_variant) {
    return variant_command(call, reply, error, workshop_variants_save);
}

WORKSHOP_HANDLER(workshop_c_item_flag) {
    return variant_command(call, reply, error, workshop_variants_flag);
}

WORKSHOP_HANDLER(workshop_c_item_promote) {
    return variant_command(call, reply, error, workshop_variants_promote);
}

WORKSHOP_HANDLER(workshop_c_item_link_form) {
    return variant_command(call, reply, error, workshop_variants_link_form);
}

/* Replaces the marks of Try every operation (the file is not a version): params checks. */
WORKSHOP_HANDLER(workshop_c_item_set_op_checks) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    UrsatzError failure = {{0}, {0}};
    cJSON *stored = NULL;
    UrsatzStatus result = workshop_material_op_checks_put(call->target, call->params, &failure, &stored);
    if (result != URSATZ_OK) {
        return failure.message[0] != '\0' ? workshop_call_fail(error, result, failure.path, "%s", failure.message) : material_failure(error, result);
    }
    return workshop_reply_tree(reply, stored, error);
}

/* --- subjects -------------------------------------------------------------------------------------------------------------------- */

static UrsatzStatus subject_failure(UrsatzError *error, UrsatzStatus status) {
    return workshop_call_fail_service(error, status, "subject store failure");
}

WORKSHOP_HANDLER(workshop_c_subject_create) {
    bool bad = false;
    const char *name = workshop_param_string(call->params, "name", &bad);
    const char *kind = workshop_param_string(call->params, "kind", &bad);
    const char *note = workshop_param_string(call->params, "note", &bad);
    if (bad || !name || !kind) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "name and kind are required; name, kind and note are strings");
    }
    cJSON *subject = NULL;
    UrsatzStatus result = workshop_subject_create(name, kind, note, &subject);
    return result == URSATZ_OK ? workshop_reply_member(reply, "subject", subject, error) : subject_failure(error, result);
}

WORKSHOP_HANDLER(workshop_c_subject_update) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    bool bad = false;
    const char *name = workshop_param_string(call->params, "name", &bad);
    const char *kind = workshop_param_string(call->params, "kind", &bad);
    const char *note = workshop_param_string(call->params, "note", &bad);
    if (bad || (!name && !kind && !note)) {
        return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params", "give name, kind and/or note as strings");
    }
    cJSON *subject = NULL;
    UrsatzStatus result = workshop_subject_update(call->target, name, kind, note, &subject);
    return result == URSATZ_OK ? workshop_reply_member(reply, "subject", subject, error) : subject_failure(error, result);
}

WORKSHOP_HANDLER(workshop_c_subject_delete) {
    if (!require_target(call, error)) {
        return URSATZ_ERR_REQUEST;
    }
    UrsatzStatus result = workshop_subject_delete(call->target);
    return result == URSATZ_OK ? empty_reply(reply, error) : subject_failure(error, result);
}

/* The reply of a link or relation: {"subject", "already"?} (already says it was in place; given for the commands that add). */
static UrsatzStatus subject_after(WorkshopReply *reply, cJSON *subject, bool already, bool with_already, UrsatzError *error) {
    cJSON *object = subject ? workshop_reply_object(reply) : NULL;
    if (!object || (with_already && !cJSON_AddBoolToObject(object, "already", already ? 1 : 0))) {
        cJSON_Delete(subject);
        return workshop_call_fail(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    cJSON_AddItemToObject(object, "subject", subject);
    return URSATZ_OK;
}

/* A subject's link to an item, a piece or another subject: params item_id, piece_id or other_id. */
static UrsatzStatus subject_link(const WorkshopCall *call, WorkshopReply *reply, UrsatzError *error, const char *member, bool add,
                                 UrsatzStatus (*link)(const char *, const char *, bool *, cJSON **),
                                 UrsatzStatus (*unlink)(const char *, const char *, cJSON **)) {
    const char *other = NULL;
    if (!require_target(call, error) || !workshop_param_require_string(call->params, member, &other, error)) {
        return URSATZ_ERR_REQUEST;
    }
    bool already = false;
    cJSON *subject = NULL;
    UrsatzStatus result = add ? link(call->target, other, &already, &subject) : unlink(call->target, other, &subject);
    return result == URSATZ_OK ? subject_after(reply, subject, already, add, error) : subject_failure(error, result);
}

WORKSHOP_HANDLER(workshop_c_subject_link_item) {
    const char *other = cJSON_IsString(cJSON_GetObjectItemCaseSensitive(call->params, "item_id"))
                            ? cJSON_GetObjectItemCaseSensitive(call->params, "item_id")->valuestring
                            : NULL;
    if (other) {
        /* A variant belongs to its motif and has no subjects (R-187). An absent item is left to the subject service. */
        cJSON *body = NULL;
        bool variant = workshop_material_item_get(other, NULL, &body) == URSATZ_OK && workshop_material_item_is_variant(body);
        cJSON_Delete(body);
        if (variant) {
            return workshop_call_fail(error, URSATZ_ERR_REQUEST, "/params/item_id", "variants have no subjects; save it as a new motif first");
        }
    }
    return subject_link(call, reply, error, "item_id", true, workshop_subject_link_item, NULL);
}

WORKSHOP_HANDLER(workshop_c_subject_unlink_item) {
    return subject_link(call, reply, error, "item_id", false, NULL, workshop_subject_unlink_item);
}

WORKSHOP_HANDLER(workshop_c_subject_link_piece) {
    return subject_link(call, reply, error, "piece_id", true, workshop_subject_link_piece, NULL);
}

WORKSHOP_HANDLER(workshop_c_subject_unlink_piece) {
    return subject_link(call, reply, error, "piece_id", false, NULL, workshop_subject_unlink_piece);
}

WORKSHOP_HANDLER(workshop_c_subject_relate) {
    return subject_link(call, reply, error, "other_id", true, workshop_subject_relate, NULL);
}

WORKSHOP_HANDLER(workshop_c_subject_unrelate) {
    return subject_link(call, reply, error, "other_id", false, NULL, workshop_subject_unrelate);
}
