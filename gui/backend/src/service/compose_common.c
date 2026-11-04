#define _XOPEN_SOURCE 700

#include "compose_common.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "material_service.h"
#include "util/fs_util.h"

#define PATH_BYTES 800

void ursatz_gui_compose_say(char *message, size_t size, const char *text) {
    if (message && size > 0) {
        snprintf(message, size, "%s", text);
    }
}

const char *ursatz_gui_compose_text_of(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : "";
}

bool ursatz_gui_compose_attach(cJSON *parent, const char *name, cJSON *child) {
    if (child && cJSON_AddItemToObject(parent, name, child)) {
        return true;
    }
    cJSON_Delete(child);
    return false;
}

bool ursatz_gui_compose_add_copy_or_null(cJSON *dst, const cJSON *src, const char *name) {
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(src, name);
    return ursatz_gui_compose_attach(dst, name, member ? cJSON_Duplicate(member, 1) : cJSON_CreateNull());
}

bool ursatz_gui_compose_add_copy_if(cJSON *dst, const cJSON *src, const char *name) {
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(src, name);
    return !member || cJSON_IsNull(member) || ursatz_gui_compose_attach(dst, name, cJSON_Duplicate(member, 1));
}

UrsatzGuiThemeResult ursatz_gui_compose_failure(ComposerStatus status, const ComposerResponse *response, char *message,
                                                size_t message_size) {
    if (response && response->error.message[0] != '\0') {
        ursatz_gui_compose_say(message, message_size, response->error.message);
    }
    switch (status) {
        case COMPOSER_OK:
            return URSATZ_GUI_THEME_OK;
        case COMPOSER_ERR_REQUEST:
            return URSATZ_GUI_THEME_ERR_INVALID;
        case COMPOSER_ERR_UNSATISFIABLE:
            return URSATZ_GUI_THEME_ERR_UNPROCESSABLE;
        case COMPOSER_ERR_CANCELLED:
            return URSATZ_GUI_THEME_ERR_CANCELLED;
        case COMPOSER_ERR_ALLOC:
            return URSATZ_GUI_THEME_ERR_ALLOC;
        default:
            return URSATZ_GUI_THEME_ERR_FAILED;
    }
}

UrsatzGuiThemeResult ursatz_gui_compose_write_plan(const cJSON *plan, const char *path, char *message,
                                                   size_t message_size) {
    cJSON *request = cJSON_CreateObject();
    bool ok = request && cJSON_AddItemToObject(request, "plan", cJSON_Duplicate(plan, 1)) &&
              cJSON_AddBoolToObject(request, "plan_file", 1);
    char *text = ok ? cJSON_PrintUnformatted(request) : NULL;
    cJSON_Delete(request);
    ComposerResponse response;
    memset(&response, 0, sizeof(response));
    ComposerStatus status = text ? composer_api_export(text, NULL, &response) : COMPOSER_ERR_ALLOC;
    free(text);
    UrsatzGuiThemeResult result = URSATZ_GUI_THEME_OK;
    if (status != COMPOSER_OK || !response.plan_json) {
        result = status == COMPOSER_OK ? URSATZ_GUI_THEME_ERR_FAILED : ursatz_gui_compose_failure(status, &response, message, message_size);
        if (status == COMPOSER_OK) {
            ursatz_gui_compose_say(message, message_size, "the plan could not be written");
        }
    } else if (!ursatz_gui_write_file_atomic(path, response.plan_json, strlen(response.plan_json))) {
        ursatz_gui_compose_say(message, message_size, "a file of the version could not be written");
        result = URSATZ_GUI_THEME_ERR_FAILED;
    }
    composer_response_destroy(&response);
    return result;
}

UrsatzGuiThemeResult ursatz_gui_compose_export(const cJSON *plan, double qpm, bool plain_midi, bool midi, bool musicxml,
                                               const cJSON *loop, const ComposerJob *job, ComposerResponse *response,
                                               char *message, size_t message_size) {
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
        ursatz_gui_compose_say(message, message_size, "the plan could not be rendered");
        return URSATZ_GUI_THEME_ERR_ALLOC;
    }
    memset(response, 0, sizeof(*response));
    ComposerStatus status = composer_api_export(printed, job, response);
    free(printed);
    if (status != COMPOSER_OK) {
        UrsatzGuiThemeResult result = ursatz_gui_compose_failure(status, response, message, message_size);
        composer_response_destroy(response);
        return result;
    }
    return URSATZ_GUI_THEME_OK;
}

void ursatz_gui_compose_rebase(cJSON *tree, const char *from, const char *to) {
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
            ursatz_gui_compose_rebase(child, from, to);
        }
    }
}

bool ursatz_gui_compose_add_files(cJSON *summary, const char *dir, const char *const *names, size_t count) {
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

UrsatzGuiThemeResult ursatz_gui_compose_stage_make(const char *id, int number, UrsatzGuiComposeStage *stage, char *message,
                                                   size_t message_size) {
    char dir[PATH_BYTES];
    char resolved[PATH_MAX];
    if (!ursatz_gui_material_version_dir(id, number, dir, sizeof(dir))) {
        ursatz_gui_compose_say(message, message_size, "the version folder does not fit");
        return URSATZ_GUI_THEME_ERR_FAILED;
    }
    unsigned serial = __atomic_add_fetch(&g_stage_serial, 1, __ATOMIC_RELAXED);
    if (snprintf(stage->path, sizeof(stage->path), "%s.stage-%u-%d", dir, serial, (int)getpid()) >= (int)sizeof(stage->path) ||
        mkdir(stage->path, 0755) != 0 || !realpath(stage->path, resolved) ||
        snprintf(stage->absolute, sizeof(stage->absolute), "%s", resolved) >= (int)sizeof(stage->absolute)) {
        ursatz_gui_compose_say(message, message_size, "the version folder could not be made");
        return URSATZ_GUI_THEME_ERR_FAILED;
    }
    /* the stage's name without its suffix is the folder the version will have */
    const char *suffix = strstr(stage->absolute, ".stage-");
    size_t keep = suffix ? (size_t)(suffix - stage->absolute) : strlen(stage->absolute);
    snprintf(stage->named, sizeof(stage->named), "%.*s", (int)keep, stage->absolute);
    return URSATZ_GUI_THEME_OK;
}

void ursatz_gui_compose_stage_finish(const char *id, const cJSON *item, const UrsatzGuiComposeStage *stage) {
    /* The version written may have another number than the one planned: another writer may have added a version while
       the Composer ran. */
    int actual = cJSON_GetObjectItemCaseSensitive(item, "current_version")->valueint;
    char target[PATH_BYTES];
    if (ursatz_gui_material_version_dir(id, actual, target, sizeof(target))) {
        ursatz_gui_theme_remove_folder(target); /* a folder no version named is left over from a failed run */
        rename(stage->path, target);
    }
}

void ursatz_gui_compose_stage_discard(const UrsatzGuiComposeStage *stage) {
    ursatz_gui_theme_remove_folder(stage->path);
}

UrsatzGuiThemeResult ursatz_gui_compose_item(const UrsatzGuiComposeKind *kind, const char *id, bool has_seed,
                                             long long seed, bool update_uses, const ComposerJob *job,
                                             cJSON **out_item, char *message, size_t message_size) {
    UrsatzGuiThemeResult result = update_uses ? ursatz_gui_theme_update_sources(id, message, message_size)
                                              : URSATZ_GUI_THEME_OK;
    if (result != URSATZ_GUI_THEME_OK) {
        return result;
    }
    cJSON *meta = NULL;
    cJSON *body = NULL;
    UrsatzGuiMaterialResult read = ursatz_gui_material_item_get(id, &meta, &body);
    if (read != URSATZ_GUI_MATERIAL_OK) {
        return read == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND || read == URSATZ_GUI_MATERIAL_ERR_INVALID_ID
                   ? URSATZ_GUI_THEME_ERR_NOT_FOUND
                   : URSATZ_GUI_THEME_ERR_FAILED;
    }
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(body, kind->member);
    const cJSON *brief_json = cJSON_GetObjectItemCaseSensitive(member, "brief");
    bool has_brief = brief_json && !cJSON_IsNull(brief_json);
    cJSON *request = NULL;
    cJSON *summary = NULL;
    cJSON *brief_summary = NULL;
    cJSON *new_body = NULL;
    cJSON *item = NULL;
    UrsatzGuiComposeStage stage;
    bool made = false;
    int number = cJSON_GetObjectItemCaseSensitive(meta, "current_version")->valueint + 1;

    if (strcmp(ursatz_gui_compose_text_of(meta, "type"), kind->member) != 0 || !cJSON_IsObject(member)) {
        ursatz_gui_compose_say(message, message_size, kind->not_kind_text);
        result = URSATZ_GUI_THEME_ERR_INVALID;
    } else if (!kind->composable(member, has_brief, message, message_size)) {
        result = URSATZ_GUI_THEME_ERR_INVALID;
    } else {
        /* The files are made in a folder of their own: another compose of this item may be running, and the number
           this one will get is only known when its version is written. */
        result = ursatz_gui_compose_stage_make(id, number, &stage, message, message_size);
        made = result == URSATZ_GUI_THEME_OK;
    }

    char style_path[PATH_BYTES + 64];
    snprintf(style_path, sizeof(style_path), "%s/brief.style.json", made ? stage.absolute : "");
    if (result == URSATZ_GUI_THEME_OK && has_brief) {
        char name[256];
        snprintf(name, sizeof(name), "%s brief", ursatz_gui_compose_text_of(meta, "name"));
        result = ursatz_gui_theme_build_brief(brief_json, name, style_path, &brief_summary, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        result = kind->build_request(body, stage.absolute, has_brief ? style_path : NULL, &request, message, message_size);
    }
    char request_path[PATH_BYTES + 64];
    snprintf(request_path, sizeof(request_path), "%s/request.json", made ? stage.absolute : "");
    if (result == URSATZ_GUI_THEME_OK) {
        char *text = cJSON_Print(request);
        if (!text || !ursatz_gui_theme_write_text(request_path, text)) {
            ursatz_gui_compose_say(message, message_size, "the request could not be written");
            result = text ? URSATZ_GUI_THEME_ERR_FAILED : URSATZ_GUI_THEME_ERR_ALLOC;
        }
        free(text);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        const cJSON *body_seed = cJSON_GetObjectItemCaseSensitive(member, "seed");
        bool use_seed = has_seed || cJSON_IsNumber(body_seed);
        long long chosen = has_seed ? seed : use_seed ? (long long)body_seed->valuedouble : 0;
        UrsatzGuiComposeRun run = {stage.absolute, stage.named, request, member, use_seed, chosen, job};
        result = kind->run(&run, &summary, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        /* The files name the version's own folder, not the one they were made in. */
        ursatz_gui_compose_rebase(request, stage.absolute, stage.named);
        ursatz_gui_compose_rebase(summary, stage.absolute, stage.named);
        char *text = cJSON_Print(request);
        if (!text || !ursatz_gui_theme_write_text(request_path, text)) {
            ursatz_gui_compose_say(message, message_size, "the request could not be written");
            result = text ? URSATZ_GUI_THEME_ERR_FAILED : URSATZ_GUI_THEME_ERR_ALLOC;
        }
        free(text);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        if (!has_brief) {
            brief_summary = cJSON_CreateObject();
            if (!brief_summary || !cJSON_AddNumberToObject(brief_summary, "piece_count", 0) ||
                !cJSON_AddArrayToObject(brief_summary, "skipped")) {
                result = URSATZ_GUI_THEME_ERR_ALLOC;
            }
        }
        char note[64];
        if (result == URSATZ_GUI_THEME_OK) {
            kind->note(summary, note, sizeof(note));
            new_body = kind->body_with_result(body, summary, brief_summary, has_brief);
            summary = NULL;
            brief_summary = NULL;
            if (!new_body) {
                result = URSATZ_GUI_THEME_ERR_ALLOC;
            } else if (ursatz_gui_material_item_add_version(id, new_body, note, &item) != URSATZ_GUI_MATERIAL_OK) {
                ursatz_gui_compose_say(message, message_size, "the version could not be written");
                result = URSATZ_GUI_THEME_ERR_FAILED;
            }
        }
    }
    if (result == URSATZ_GUI_THEME_OK) {
        ursatz_gui_compose_stage_finish(id, item, &stage);
        *out_item = item;
    } else if (made) {
        ursatz_gui_compose_stage_discard(&stage);
    }
    cJSON_Delete(request);
    cJSON_Delete(summary);
    cJSON_Delete(brief_summary);
    cJSON_Delete(new_body);
    cJSON_Delete(meta);
    cJSON_Delete(body);
    return result;
}
