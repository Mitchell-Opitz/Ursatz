#define _XOPEN_SOURCE 700

#include "compose_common.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "material_service.h"

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

UrsatzGuiThemeResult ursatz_gui_compose_item(const UrsatzGuiComposeKind *kind, const char *id, bool has_seed,
                                             long long seed, bool update_uses, cJSON **out_item, char *message,
                                             size_t message_size) {
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
    char dir[PATH_BYTES];
    char absolute[PATH_BYTES];
    bool made = false;
    int number = cJSON_GetObjectItemCaseSensitive(meta, "current_version")->valueint + 1;

    if (strcmp(ursatz_gui_compose_text_of(meta, "type"), kind->member) != 0 || !cJSON_IsObject(member)) {
        ursatz_gui_compose_say(message, message_size, kind->not_kind_text);
        result = URSATZ_GUI_THEME_ERR_INVALID;
    } else if (!kind->composable(member, has_brief, message, message_size)) {
        result = URSATZ_GUI_THEME_ERR_INVALID;
    } else if (!ursatz_gui_material_version_dir(id, number, dir, sizeof(dir))) {
        ursatz_gui_compose_say(message, message_size, "the version folder does not fit");
        result = URSATZ_GUI_THEME_ERR_FAILED;
    } else {
        ursatz_gui_theme_remove_folder(dir); /* a folder no version names is left over from a failed run */
        char resolved[PATH_MAX];
        if (mkdir(dir, 0755) != 0 || !realpath(dir, resolved) ||
            snprintf(absolute, sizeof(absolute), "%s", resolved) >= (int)sizeof(absolute)) {
            ursatz_gui_compose_say(message, message_size, "the version folder could not be made");
            result = URSATZ_GUI_THEME_ERR_FAILED;
        }
        made = result == URSATZ_GUI_THEME_OK;
    }

    char style_path[PATH_BYTES + 32];
    snprintf(style_path, sizeof(style_path), "%s/brief.style.json", made ? absolute : "");
    if (result == URSATZ_GUI_THEME_OK && has_brief) {
        char name[256];
        snprintf(name, sizeof(name), "%s brief", ursatz_gui_compose_text_of(meta, "name"));
        result = ursatz_gui_theme_build_brief(brief_json, name, style_path, &brief_summary, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        result = kind->build_request(body, absolute, has_brief ? style_path : NULL, &request, message, message_size);
    }
    if (result == URSATZ_GUI_THEME_OK) {
        char request_path[PATH_BYTES + 32];
        char *text = cJSON_Print(request);
        snprintf(request_path, sizeof(request_path), "%s/request.json", absolute);
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
        result = kind->run(absolute, member, use_seed, chosen, &summary, message, message_size);
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
        /* Another writer may have added a version while the Composer ran; the folder follows its version. */
        int actual = cJSON_GetObjectItemCaseSensitive(item, "current_version")->valueint;
        char target[PATH_BYTES];
        if (actual != number && ursatz_gui_material_version_dir(id, actual, target, sizeof(target))) {
            rename(dir, target);
        }
        *out_item = item;
    } else if (made) {
        ursatz_gui_theme_remove_folder(dir);
    }
    cJSON_Delete(request);
    cJSON_Delete(summary);
    cJSON_Delete(brief_summary);
    cJSON_Delete(new_body);
    cJSON_Delete(meta);
    cJSON_Delete(body);
    return result;
}
