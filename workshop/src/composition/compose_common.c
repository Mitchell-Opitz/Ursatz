#define _XOPEN_SOURCE 700

#include "compose_common.h"
#include "base/last_error.h"
#include "base/knowledge.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <common/files.h>
#include <common/strings.h>
#include <common/json.h>
#include <common/status.h>

#include "items/store.h"
#include "items/formats.h"
#include "items/layout.h"
#include "request_resolve.h"
#include "settings/settings_keys.h"
#include "settings/settings_resolve.h"

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

UrsatzStatus workshop_compose_write_layers(const char *stage, cJSON *document, UrsatzError *error) {
    cJSON *realization = cJSON_DetachItemFromObjectCaseSensitive(document, "realization");
    cJSON *reading = cJSON_DetachItemFromObjectCaseSensitive(document, "reading");
    UrsatzStatus result = URSATZ_OK;
    if (cJSON_IsObject(realization) && cJSON_IsObject(reading)) {
        char path[PATH_BYTES + 64];
        char folder[PATH_BYTES + 64];
        char *realization_text = cJSON_Print(realization);
        char *reading_text = cJSON_Print(reading);
        snprintf(folder, sizeof(folder), "%s/readings", stage);
        bool ok = realization_text && reading_text && snprintf(path, sizeof(path), "%s/realization.json", stage) < (int)sizeof(path) &&
                  common_file_write_atomic(path, realization_text, strlen(realization_text), NULL) == URSATZ_OK &&
                  common_dir_make(folder, NULL) == URSATZ_OK && snprintf(path, sizeof(path), "%s/composer.json", folder) < (int)sizeof(path) &&
                  common_file_write_atomic(path, reading_text, strlen(reading_text), NULL) == URSATZ_OK;
        if (!ok) {
            workshop_error_text(error, "the layers of the version could not be written");
            result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
        }
        free(realization_text);
        free(reading_text);
    }
    cJSON_Delete(realization);
    cJSON_Delete(reading);
    return result;
}

void workshop_compose_stage_discard(const WorkshopComposeStage *stage) {
    common_dir_remove(stage->path, NULL);
}

/* The test hook (P63 B15): when URSATZ_DUMP_REQUESTS names a folder, each request a compose builds is written there with the
   body it was built from, as <kind>-<hash>.json {kind, body, request}; the folder of the version's files reads "@STAGE@" and
   the data folder "@DATA@", so the file does not depend on where the test ran. */
static void dump_request(const WorkshopComposeKind *kind, const cJSON *body, const cJSON *request, const char *stage) {
    const char *folder = getenv("URSATZ_DUMP_REQUESTS");
    if (!folder || folder[0] == '\0') {
        return;
    }
    cJSON *copy = cJSON_Duplicate(request, 1);
    cJSON *record = cJSON_CreateObject();
    const char *items = strstr(stage, "/items/");
    if (copy && record) {
        workshop_compose_rebase(copy, stage, "@STAGE@");
        if (items) {
            char data[PATH_BYTES];
            snprintf(data, sizeof(data), "%.*s", (int)(items - stage), stage);
            workshop_compose_rebase(copy, data, "@DATA@");
        }
        cJSON_AddStringToObject(record, "kind", kind->member);
        cJSON_AddItemToObject(record, "body", cJSON_Duplicate(body, 1));
        cJSON_AddItemToObject(record, "request", copy);
        copy = NULL;
        char *text = cJSON_Print(record);
        if (text) {
            unsigned long hash = 5381;
            for (const char *p = text; *p; p++) {
                hash = hash * 33 + (unsigned char)*p;
            }
            char path[PATH_BYTES + 64];
            snprintf(path, sizeof(path), "%s/%s-%08lx.json", folder, kind->member, hash & 0xffffffffUL);
            common_file_write_atomic(path, text, strlen(text), NULL);
        }
        free(text);
    }
    cJSON_Delete(copy);
    cJSON_Delete(record);
}

/* Merges src into dst: an object member merges into the object of the same name, any other member replaces. */
static bool merge_document(cJSON *dst, const cJSON *src) {
    const cJSON *member;
    cJSON_ArrayForEach(member, src) {
        cJSON *held = cJSON_GetObjectItemCaseSensitive(dst, member->string);
        if (cJSON_IsObject(member) && cJSON_IsObject(held)) {
            if (!merge_document(held, member)) {
                return false;
            }
            continue;
        }
        cJSON *copy = cJSON_Duplicate(member, true);
        if (!copy) {
            return false;
        }
        if (held) {
            cJSON_ReplaceItemInObjectCaseSensitive(dst, member->string, copy);
        } else if (!cJSON_AddItemToObject(dst, member->string, copy)) {
            cJSON_Delete(copy);
            return false;
        }
    }
    return true;
}

/*
 * The settings of a theme's compose (R-671): resolved with the item's project, the item, the style its brief names alone, and the call's own
 * scope, over the body request's own preferences. A document becomes the request's `preferences`; the snapshot is written to the stage when
 * the resolution says anything beyond the defaults.
 */
static UrsatzStatus resolve_theme_settings(const cJSON *meta, const char *id, const cJSON *body, const WorkshopComposeSettings *settings, bool explore,
                                           long long explore_seed, cJSON *request, const char *stage, UrsatzError *error) {
    if (workshop_settings_key_count() == 0) {
        return URSATZ_OK; /* a caller that never opened a workshop (a service test) has no settings to resolve */
    }
    cJSON *values = cJSON_CreateObject();
    const cJSON *own = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(body, "request"), "preferences");
    if (!values || (cJSON_IsObject(own) && !merge_document(values, own)) ||
        (settings && cJSON_IsObject(settings->settings) && !merge_document(values, settings->settings))) {
        cJSON_Delete(values);
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    const cJSON *styles = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(body, "brief"), "style_ids");
    WorkshopSettingsRequest resolve;
    memset(&resolve, 0, sizeof(resolve));
    resolve.project_id = common_json_string(meta, "project_id");
    resolve.item_id = id;
    resolve.style_id = cJSON_IsArray(styles) && cJSON_GetArraySize(styles) == 1 && cJSON_IsString(cJSON_GetArrayItem(styles, 0))
                           ? cJSON_GetArrayItem(styles, 0)->valuestring
                           : NULL;
    resolve.taste = settings ? settings->taste : NULL;
    resolve.values = cJSON_GetArraySize(values) > 0 ? values : NULL;
    resolve.overrides = settings ? settings->overrides : NULL;
    resolve.off = settings ? settings->off : NULL;
    resolve.kind = "theme";
    resolve.explore = explore;
    resolve.explore_seed = (unsigned long long)explore_seed;
    resolve.explore_distance = settings ? settings->distance : 0;
    cJSON *answer = NULL;
    UrsatzError failure = {{0}, {0}};
    UrsatzStatus status = workshop_settings_resolve_request(&resolve, false, &answer, &failure);
    cJSON_Delete(values);
    if (status != URSATZ_OK) {
        if (error) {
            *error = failure;
        }
        return workshop_fail(status == URSATZ_ERR_ALLOC ? status : URSATZ_ERR_REQUEST, NULL);
    }
    const cJSON *document = cJSON_GetObjectItemCaseSensitive(answer, "document");
    const cJSON *moves = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(answer, "explore"), "moves");
    bool says_something = cJSON_IsObject(document) || cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(answer, "overrides")) > 0 ||
                          cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(answer, "off")) > 0 || cJSON_GetArraySize(moves) > 0;
    if (cJSON_IsObject(document)) {
        cJSON *copy = cJSON_Duplicate(document, true);
        cJSON_DeleteItemFromObjectCaseSensitive(request, "preferences");
        if (!copy || !cJSON_AddItemToObject(request, "preferences", copy)) {
            cJSON_Delete(copy);
            cJSON_Delete(answer);
            return workshop_fail(URSATZ_ERR_ALLOC, NULL);
        }
    }
    if (says_something) {
        /* the snapshot: what the compose resolved, without the values the defaults hold */
        cJSON *snapshot = cJSON_CreateObject();
        const char *profile = common_json_string(answer, "taste");
        bool ok = snapshot && cJSON_AddStringToObject(snapshot, "taste", profile) &&
                  cJSON_AddItemToObject(snapshot, "off", cJSON_DetachItemFromObjectCaseSensitive(answer, "off")) &&
                  cJSON_AddItemToObject(snapshot, "values", cJSON_DetachItemFromObjectCaseSensitive(answer, "values")) &&
                  cJSON_AddItemToObject(snapshot, "overrides", cJSON_DetachItemFromObjectCaseSensitive(answer, "overrides")) &&
                  cJSON_AddItemToObject(snapshot, "explore",
                                        cJSON_GetObjectItemCaseSensitive(answer, "explore") ? cJSON_DetachItemFromObjectCaseSensitive(answer, "explore") : cJSON_CreateNull()) &&
                  cJSON_AddItemToObject(snapshot, "document", cJSON_IsObject(document) ? cJSON_DetachItemFromObjectCaseSensitive(answer, "document") : cJSON_CreateNull());
        char path[PATH_BYTES + 64];
        snprintf(path, sizeof(path), "%s/%s", stage, WORKSHOP_FILE_SETTINGS);
        UrsatzError written = {{0}, {0}};
        ok = ok && workshop_format_write(path, WORKSHOP_FORMAT_SETTINGS_SNAPSHOT, snapshot, &written) == URSATZ_OK;
        cJSON_Delete(snapshot);
        if (!ok) {
            cJSON_Delete(answer);
            workshop_error_text(error, "the settings could not be written");
            return workshop_fail(URSATZ_ERR_INTERNAL, NULL);
        }
    }
    cJSON_Delete(answer);
    return URSATZ_OK;
}

UrsatzStatus workshop_compose_item(const WorkshopComposeKind *kind, const char *id, bool has_seed,
                                             long long seed, bool update_uses, const WorkshopComposeSettings *settings,
                                             const UrsatzJob *job, cJSON **out_item, UrsatzError *error) {
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
    const cJSON *brief_json = cJSON_GetObjectItemCaseSensitive(body, "brief");
    bool has_brief = brief_json && !cJSON_IsNull(brief_json);
    cJSON *request = NULL;
    cJSON *summary = NULL;
    cJSON *brief_summary = NULL;
    cJSON *new_body = NULL;
    cJSON *item = NULL;
    WorkshopComposeStage stage;
    bool made = false;
    int number = cJSON_GetObjectItemCaseSensitive(meta, "current_version")->valueint + 1;

    if (strcmp(common_json_string(meta, "type"), kind->member) != 0 || !cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(body, "request"))) {
        workshop_error_text(error, kind->not_kind_text);
        result = workshop_fail(URSATZ_ERR_REQUEST, NULL);
    } else if (!kind->composable(body, error)) {
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
        result = workshop_request_resolve(kind->member, body, stage.absolute, has_brief ? style_path : NULL, &request, error);
        if (result == URSATZ_OK && strcmp(kind->member, "theme") == 0) {
            /* the settings: resolved before the request is dumped and written, so request.json records them (R-671) */
            const cJSON *body_seed_member = cJSON_GetObjectItemCaseSensitive(body, "seed");
            if (settings && settings->explore && !has_seed && !cJSON_IsNumber(body_seed_member)) {
                has_seed = true;
                seed = (long long)((unsigned long)time(NULL) % 4294967296UL);
            }
            long long explore_seed = has_seed ? seed : cJSON_IsNumber(body_seed_member) ? (long long)body_seed_member->valuedouble : 0;
            result = resolve_theme_settings(meta, id, body, settings, settings && settings->explore, explore_seed, request, stage.absolute, error);
        }
        if (result == URSATZ_OK) {
            dump_request(kind, body, request, stage.absolute);
        }
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
        const cJSON *body_seed = cJSON_GetObjectItemCaseSensitive(body, "seed");
        bool use_seed = has_seed || cJSON_IsNumber(body_seed);
        long long chosen = has_seed ? seed : use_seed ? (long long)body_seed->valuedouble : 0;
        WorkshopComposeRun run = {stage.absolute, stage.named, request, body, use_seed, chosen, job};
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
            } else {
                UrsatzStatus written = workshop_material_item_add_version_staged(id, new_body, note, stage.path, &item);
                if (written == URSATZ_ERR_CONFLICT) {
                    /* the item changed since the command was made (R-646): the refusal is the job's answer */
                    const UrsatzError *refusal = workshop_last_error();
                    if (error) {
                        *error = *refusal;
                    }
                    result = workshop_fail_with(URSATZ_ERR_CONFLICT, refusal);
                } else if (written != URSATZ_OK) {
                    workshop_error_text(error, "the version could not be written");
                    result = workshop_fail(URSATZ_ERR_INTERNAL, NULL);
                }
            }
        }
    }
    if (result == URSATZ_OK) {
        *out_item = item; /* the stage is the version's folder now */
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
