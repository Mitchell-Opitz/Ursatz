#define _XOPEN_SOURCE 700

#include "compose_common.h"
#include "base/last_error.h"
#include "base/knowledge.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <common/files.h>
#include <common/strings.h>
#include <common/json.h>
#include <common/status.h>

#include "items/store.h"

#define PATH_BYTES 800

UrsatzStatus workshop_compose_failure(UrsatzStatus status, const ComposerResponse *response, UrsatzError *error) {
    if (error && response) {
        *error = response->error;
    }
    return status == URSATZ_OK || status == URSATZ_ERR_REQUEST || status == URSATZ_ERR_UNSATISFIABLE ||
                   status == URSATZ_ERR_CANCELLED || status == URSATZ_ERR_ALLOC
               ? workshop_fail_with(status, response ? &response->error : NULL)
               : workshop_fail_with(URSATZ_ERR_INTERNAL, response ? &response->error : NULL);
}

UrsatzStatus workshop_compose_write_plan(const cJSON *plan, const char *path, UrsatzError *error) {
    cJSON *request = cJSON_CreateObject();
    bool ok = request && cJSON_AddItemToObject(request, "plan", cJSON_Duplicate(plan, 1)) &&
              cJSON_AddBoolToObject(request, "plan_file", 1);
    char *text = ok ? cJSON_PrintUnformatted(request) : NULL;
    cJSON_Delete(request);
    ComposerResponse response;
    memset(&response, 0, sizeof(response));
    UrsatzStatus status = text ? composer_api_export(workshop_composer_context_global(), text, NULL, &response) : URSATZ_ERR_ALLOC;
    free(text);
    UrsatzStatus result = URSATZ_OK;
    if (status != URSATZ_OK || !response.plan_json) {
        result = status == URSATZ_OK ? workshop_fail(URSATZ_ERR_INTERNAL, NULL) : workshop_compose_failure(status, &response, error);
        if (status == URSATZ_OK) {
            workshop_error_text(error, "the plan could not be written");
        }
    } else if (!(common_file_write_atomic(path, response.plan_json, strlen(response.plan_json), NULL) == URSATZ_OK)) {
        workshop_error_text(error, "a file of the version could not be written");
        result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
    }
    composer_response_destroy(&response);
    return result;
}

UrsatzStatus workshop_compose_export(const cJSON *plan, double qpm, bool plain_midi, bool midi, bool musicxml,
                                               const cJSON *loop, const UrsatzJob *job, ComposerResponse *response,
                                               UrsatzError *error) {
    cJSON *request = cJSON_CreateObject();
    bool ok = request && cJSON_AddItemToObject(request, "plan", cJSON_Duplicate(plan, 1)) &&
              cJSON_AddBoolToObject(request, "midi", midi) && cJSON_AddBoolToObject(request, "musicxml", musicxml) &&
              cJSON_AddBoolToObject(request, "plain_midi", plain_midi);
    if (ok && loop) {
        ok = cJSON_AddItemToObject(request, "loop", cJSON_Duplicate(loop, 1));
    }
    if (ok && qpm > 0) {
        /* A tempo is a whole number or half-steps like 195/2: the document wrote it with %g. */
        char text[48] = "";
        for (long den = 1; den <= 16 && !text[0]; den++) {
            double scaled = qpm * (double)den;
            if (scaled - (double)(long)(scaled + 0.5) < 1e-6 && (double)(long)(scaled + 0.5) - scaled < 1e-6) {
                snprintf(text, sizeof(text), den == 1 ? "%ld" : "%ld/%ld", (long)(scaled + 0.5), den);
            }
        }
        ok = text[0] && cJSON_AddStringToObject(request, "tempo", text);
    }
    char *printed = ok ? cJSON_PrintUnformatted(request) : NULL;
    cJSON_Delete(request);
    if (!printed) {
        workshop_error_text(error, "the plan could not be rendered");
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    memset(response, 0, sizeof(*response));
    UrsatzStatus status = composer_api_export(workshop_composer_context_global(), printed, job, response);
    free(printed);
    if (status != URSATZ_OK) {
        UrsatzStatus result = workshop_compose_failure(status, response, error);
        composer_response_destroy(response);
        return result;
    }
    return URSATZ_OK;
}

void workshop_compose_rebase(cJSON *tree, const char *from, const char *to) {
    size_t length = strlen(from);
    for (cJSON *child = tree ? tree->child : NULL; child; child = child->next) {
        if (cJSON_IsString(child) && child->valuestring && strncmp(child->valuestring, from, length) == 0) {
            size_t rest = strlen(child->valuestring + length);
            char *replaced = malloc(strlen(to) + rest + 1);
            if (replaced) {
                memcpy(replaced, to, strlen(to));
                memcpy(replaced + strlen(to), child->valuestring + length, rest + 1);
                free(child->valuestring);
                child->valuestring = replaced;
            }
        } else if (cJSON_IsObject(child) || cJSON_IsArray(child)) {
            workshop_compose_rebase(child, from, to);
        }
    }
}

bool workshop_compose_add_files(cJSON *summary, const char *dir, const char *const *names, size_t count) {
    cJSON *files = cJSON_AddArrayToObject(summary, "files");
    for (size_t i = 0; files && i < count; i++) {
        char path[PATH_BYTES + 64];
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        if (!cJSON_AddItemToArray(files, cJSON_CreateString(path))) {
            return false;
        }
    }
    return files != NULL;
}

static unsigned g_stage_serial = 0;

UrsatzStatus workshop_compose_stage_make(const char *id, int number, WorkshopComposeStage *stage, UrsatzError *error) {
    char dir[PATH_BYTES];
    char resolved[PATH_MAX];
    if (!workshop_material_version_dir(id, number, dir, sizeof(dir))) {
        workshop_error_text(error, "the version folder does not fit");
        return workshop_fail(URSATZ_ERR_INTERNAL, NULL);
    }
    unsigned serial = __atomic_add_fetch(&g_stage_serial, 1, __ATOMIC_RELAXED);
    if (snprintf(stage->path, sizeof(stage->path), "%s.stage-%u-%d", dir, serial, (int)getpid()) >= (int)sizeof(stage->path) ||
        mkdir(stage->path, 0755) != 0 || !realpath(stage->path, resolved) ||
        snprintf(stage->absolute, sizeof(stage->absolute), "%s", resolved) >= (int)sizeof(stage->absolute)) {
        workshop_error_text(error, "the version folder could not be made");
        return workshop_fail(URSATZ_ERR_INTERNAL, NULL);
    }
    /* the stage's name without its suffix is the folder the version will have */
    const char *suffix = strstr(stage->absolute, ".stage-");
    size_t keep = suffix ? (size_t)(suffix - stage->absolute) : strlen(stage->absolute);
    snprintf(stage->named, sizeof(stage->named), "%.*s", (int)keep, stage->absolute);
    return URSATZ_OK;
}

void workshop_compose_stage_finish(const char *id, const cJSON *item, const WorkshopComposeStage *stage) {
    /* The version written may have another number than the one planned: another writer may have added a version while
       the Composer ran. */
    int actual = cJSON_GetObjectItemCaseSensitive(item, "current_version")->valueint;
    char target[PATH_BYTES];
    if (workshop_material_version_dir(id, actual, target, sizeof(target))) {
        common_dir_remove(target, NULL); /* a folder no version named is left over from a failed run */
        rename(stage->path, target);
    }
}

void workshop_compose_stage_discard(const WorkshopComposeStage *stage) {
    common_dir_remove(stage->path, NULL);
}

UrsatzStatus workshop_compose_item(const WorkshopComposeKind *kind, const char *id, bool has_seed,
                                             long long seed, bool update_uses, const UrsatzJob *job,
                                             cJSON **out_item, UrsatzError *error) {
    UrsatzStatus result = update_uses ? kind->update_sources(id, error)
                                              : URSATZ_OK;
    if (result != URSATZ_OK) {
        return result;
    }
    cJSON *meta = NULL;
    cJSON *body = NULL;
    UrsatzStatus read = workshop_material_item_get(id, &meta, &body);
    if (read != URSATZ_OK) {
        return read == URSATZ_ERR_NOT_FOUND || read == URSATZ_ERR_REQUEST
                   ? workshop_fail(URSATZ_ERR_NOT_FOUND, NULL)
                   : workshop_fail(URSATZ_ERR_INTERNAL, NULL);
    }
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(body, kind->member);
    const cJSON *brief_json = cJSON_GetObjectItemCaseSensitive(member, "brief");
    bool has_brief = brief_json && !cJSON_IsNull(brief_json);
    cJSON *request = NULL;
    cJSON *summary = NULL;
    cJSON *brief_summary = NULL;
    cJSON *new_body = NULL;
    cJSON *item = NULL;
    WorkshopComposeStage stage;
    bool made = false;
    int number = cJSON_GetObjectItemCaseSensitive(meta, "current_version")->valueint + 1;

    if (strcmp(common_json_string(meta, "type"), kind->member) != 0 || !cJSON_IsObject(member)) {
        workshop_error_text(error, kind->not_kind_text);
        result = workshop_fail(URSATZ_ERR_REQUEST, NULL);
    } else if (!kind->composable(member, has_brief, error)) {
        result = workshop_fail(URSATZ_ERR_REQUEST, NULL);
    } else {
        /* The files are made in a folder of their own: another compose of this item may be running, and the number
           this one will get is only known when its version is written. */
        result = workshop_compose_stage_make(id, number, &stage, error);
        made = result == URSATZ_OK;
    }

    char style_path[PATH_BYTES + 64];
    snprintf(style_path, sizeof(style_path), "%s/brief.style.json", made ? stage.absolute : "");
    if (result == URSATZ_OK && has_brief) {
        char name[256];
        snprintf(name, sizeof(name), "%s brief", common_json_string(meta, "name"));
        result = kind->build_brief(brief_json, name, style_path, &brief_summary, error);
    }
    if (result == URSATZ_OK) {
        result = kind->build_request(body, stage.absolute, has_brief ? style_path : NULL, &request, error);
    }
    char request_path[PATH_BYTES + 64];
    snprintf(request_path, sizeof(request_path), "%s/request.json", made ? stage.absolute : "");
    if (result == URSATZ_OK) {
        char *text = cJSON_Print(request);
        if (!text || !(common_file_write_atomic(request_path, text, strlen(text), NULL) == URSATZ_OK)) {
            workshop_error_text(error, "the request could not be written");
            result = text ? workshop_fail(URSATZ_ERR_INTERNAL, NULL) : workshop_fail(URSATZ_ERR_ALLOC, NULL);
        }
        free(text);
    }
    if (result == URSATZ_OK) {
        const cJSON *body_seed = cJSON_GetObjectItemCaseSensitive(member, "seed");
        bool use_seed = has_seed || cJSON_IsNumber(body_seed);
        long long chosen = has_seed ? seed : use_seed ? (long long)body_seed->valuedouble : 0;
        WorkshopComposeRun run = {stage.absolute, stage.named, request, member, use_seed, chosen, job};
        result = kind->run(&run, &summary, error);
    }
    if (result == URSATZ_OK) {
        /* The files name the version's own folder, not the one they were made in. */
        workshop_compose_rebase(request, stage.absolute, stage.named);
        workshop_compose_rebase(summary, stage.absolute, stage.named);
        char *text = cJSON_Print(request);
        if (!text || !(common_file_write_atomic(request_path, text, strlen(text), NULL) == URSATZ_OK)) {
            workshop_error_text(error, "the request could not be written");
            result = text ? workshop_fail(URSATZ_ERR_INTERNAL, NULL) : workshop_fail(URSATZ_ERR_ALLOC, NULL);
        }
        free(text);
    }
    if (result == URSATZ_OK) {
        if (!has_brief) {
            brief_summary = cJSON_CreateObject();
            if (!brief_summary || !cJSON_AddNumberToObject(brief_summary, "piece_count", 0) ||
                !cJSON_AddArrayToObject(brief_summary, "skipped")) {
                result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
            }
        }
        char note[64];
        if (result == URSATZ_OK) {
            kind->note(summary, note, sizeof(note));
            new_body = kind->body_with_result(body, summary, brief_summary, has_brief);
            summary = NULL;
            brief_summary = NULL;
            if (!new_body) {
                result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
            } else if (workshop_material_item_add_version(id, new_body, note, &item) != URSATZ_OK) {
                workshop_error_text(error, "the version could not be written");
                result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
            }
        }
    }
    if (result == URSATZ_OK) {
        workshop_compose_stage_finish(id, item, &stage);
        *out_item = item;
    } else if (made) {
        workshop_compose_stage_discard(&stage);
    }
    cJSON_Delete(request);
    cJSON_Delete(summary);
    cJSON_Delete(brief_summary);
    cJSON_Delete(new_body);
    cJSON_Delete(meta);
    cJSON_Delete(body);
    return result;
}
