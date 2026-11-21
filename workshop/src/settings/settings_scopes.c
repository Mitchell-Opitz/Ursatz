#include "settings/settings_scopes.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/files.h>
#include <common/json.h>
#include <common/strings.h>
#include <ursatz_workshop/workshop_api.h>

#include "items/formats.h"
#include "items/layout.h"
#include "items/store.h"

static UrsatzStatus refuse(UrsatzError *error, UrsatzStatus status, const char *path, const char *format, ...)
    __attribute__((format(printf, 4, 5)));

static UrsatzStatus refuse(UrsatzError *error, UrsatzStatus status, const char *path, const char *format, ...) {
    if (error) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(error->message, sizeof error->message, format, arguments);
        va_end(arguments);
        snprintf(error->path, sizeof error->path, "%s", path);
    }
    return status;
}

bool workshop_scope_parse(const char *scope, WorkshopScopeKind *kind, char *id, size_t size) {
    static const struct {
        const char *prefix;
        WorkshopScopeKind kind;
    } kinds[] = {{"taste:", WORKSHOP_SCOPE_TASTE}, {"style:", WORKSHOP_SCOPE_STYLE}, {"project:", WORKSHOP_SCOPE_PROJECT}, {"item:", WORKSHOP_SCOPE_ITEM}};
    for (size_t i = 0; scope && i < sizeof(kinds) / sizeof(kinds[0]); i++) {
        size_t length = strlen(kinds[i].prefix);
        if (strncmp(scope, kinds[i].prefix, length) == 0) {
            if (!workshop_layout_id_ok(scope + length) || strlen(scope + length) >= size) {
                return false;
            }
            *kind = kinds[i].kind;
            snprintf(id, size, "%s", scope + length);
            return true;
        }
    }
    return false;
}

UrsatzStatus workshop_scope_path(const char *scope, char *path, size_t size, UrsatzError *error) {
    WorkshopScopeKind kind;
    char id[80];
    if (!workshop_scope_parse(scope, &kind, id, sizeof(id))) {
        return refuse(error, URSATZ_ERR_REQUEST, "/params/scope", "a scope is taste:<name>, style:<id>, project:<id> or item:<id>");
    }
    bool ok = false;
    if (kind == WORKSHOP_SCOPE_TASTE) {
        ok = workshop_layout_taste_file(id, path, size);
    } else if (kind == WORKSHOP_SCOPE_STYLE) {
        char dir[256];
        if (!workshop_layout_style_dir(id, dir, sizeof(dir)) || !common_path_exists(dir)) {
            return refuse(error, URSATZ_ERR_NOT_FOUND, "/params/scope", "there is no style '%s'", id);
        }
        ok = workshop_layout_style_settings_file(id, path, size);
    } else if (kind == WORKSHOP_SCOPE_PROJECT) {
        cJSON *project = NULL;
        if (workshop_material_project_get(id, &project) != URSATZ_OK) {
            return refuse(error, URSATZ_ERR_NOT_FOUND, "/params/scope", "there is no project '%s'", id);
        }
        cJSON_Delete(project);
        ok = workshop_layout_project_settings_file(id, path, size);
    } else {
        cJSON *meta = NULL;
        if (workshop_material_item_get(id, &meta, NULL) != URSATZ_OK) {
            return refuse(error, URSATZ_ERR_NOT_FOUND, "/params/scope", "there is no item '%s'", id);
        }
        ok = workshop_layout_item_settings_file(common_json_string(meta, "project_id"), id, path, size);
        cJSON_Delete(meta);
    }
    return ok ? URSATZ_OK : refuse(error, URSATZ_ERR_REQUEST, "/params/scope", "the path of the scope is too long");
}

UrsatzStatus workshop_scope_read(const char *scope, cJSON **out, UrsatzError *error) {
    *out = NULL;
    char path[320];
    UrsatzStatus status = workshop_scope_path(scope, path, sizeof(path), error);
    if (status != URSATZ_OK) {
        return status;
    }
    if (common_path_exists(path)) {
        status = workshop_format_read(path, WORKSHOP_FORMAT_SETTINGS, true, out, error);
        return status;
    }
    cJSON *empty = cJSON_CreateObject();
    if (!empty || !cJSON_AddStringToObject(empty, "scope", scope) || !cJSON_AddObjectToObject(empty, "values")) {
        cJSON_Delete(empty);
        return refuse(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    *out = empty;
    return URSATZ_OK;
}

UrsatzStatus workshop_scope_check(const cJSON *file, WorkshopScopeKind kind, const char *where, UrsatzError *error) {
    char path[160];
    bool profile = kind == WORKSHOP_SCOPE_TASTE;
    bool shared = profile || kind == WORKSHOP_SCOPE_STYLE;
    if (shared && cJSON_GetObjectItemCaseSensitive(file, "overrides")) {
        snprintf(path, sizeof path, "%s/overrides", where);
        return refuse(error, URSATZ_ERR_REQUEST, path, "overrides go on a project, an item or a request, never in a profile");
    }
    if (shared && cJSON_GetObjectItemCaseSensitive(file, "off")) {
        snprintf(path, sizeof path, "%s/off", where);
        return refuse(error, URSATZ_ERR_REQUEST, path, "a switch goes on a project, an item or a request, never in a profile");
    }
    const cJSON *off = cJSON_GetObjectItemCaseSensitive(file, "off");
    const cJSON *word;
    cJSON_ArrayForEach(word, off) {
        if (cJSON_IsString(word) && strcmp(word->valuestring, "project") == 0 && kind != WORKSHOP_SCOPE_ITEM) {
            snprintf(path, sizeof path, "%s/off", where);
            return refuse(error, URSATZ_ERR_REQUEST, path, "only an item can switch its project off");
        }
    }
    if (profile && cJSON_GetObjectItemCaseSensitive(file, "taste")) {
        snprintf(path, sizeof path, "%s/taste", where);
        return refuse(error, URSATZ_ERR_REQUEST, path, "a taste profile does not name a taste profile");
    }
    if (!profile && cJSON_GetObjectItemCaseSensitive(file, "kept")) {
        snprintf(path, sizeof path, "%s/kept", where);
        return refuse(error, URSATZ_ERR_REQUEST, path, "only a taste profile keeps what a listening round chose");
    }
    return URSATZ_OK;
}

UrsatzStatus workshop_scope_stage(WorkshopTxn *txn, const char *scope, cJSON *file, UrsatzError *error) {
    WorkshopScopeKind kind;
    char id[80];
    char path[320];
    if (!workshop_scope_parse(scope, &kind, id, sizeof(id))) {
        return refuse(error, URSATZ_ERR_REQUEST, "/params/scope", "a scope is taste:<name>, style:<id>, project:<id> or item:<id>");
    }
    UrsatzStatus status = workshop_scope_path(scope, path, sizeof(path), error);
    if (status == URSATZ_OK) {
        status = workshop_scope_check(file, kind, "", error);
    }
    if (status == URSATZ_OK) {
        /* the file names its own scope */
        cJSON_DeleteItemFromObjectCaseSensitive(file, "scope");
        cJSON *named = cJSON_CreateString(scope);
        if (!named || !cJSON_AddItemToObject(file, "scope", named)) {
            cJSON_Delete(named);
            return refuse(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
        char folder[320];
        snprintf(folder, sizeof folder, "%s", path);
        char *slash = strrchr(folder, '/');
        if (slash) {
            *slash = '\0';
            status = common_dir_make(folder, error);
        }
    }
    if (status == URSATZ_OK) {
        status = workshop_txn_stage_json(txn, path, WORKSHOP_FORMAT_SETTINGS, file);
    }
    return status;
}

/* The ids of the folders (or the names of the .json files) in a folder, added to out. */
static bool add_names(cJSON *list, const char *folder, bool files, const char *needs_file) {
    CommonNames names;
    memset(&names, 0, sizeof(names));
    if (common_dir_list(folder, &names, NULL) != URSATZ_OK) {
        common_names_free(&names);
        return true; /* a folder that is not there holds none */
    }
    bool ok = true;
    for (size_t i = 0; ok && i < names.count; i++) {
        const char *name = names.items[i];
        char text[128];
        if (files) {
            size_t length = strlen(name);
            if (length <= 5 || strcmp(name + length - 5, ".json") != 0 || name[0] == '.') {
                continue;
            }
            snprintf(text, sizeof text, "%.*s", (int)(length - 5), name);
        } else {
            char path[384];
            char scope_folder[384];
            if (name[0] == '.' || !common_path_join(scope_folder, sizeof scope_folder, folder, name) ||
                !common_path_join(path, sizeof path, scope_folder, needs_file) || !common_path_exists(path)) {
                continue;
            }
            snprintf(text, sizeof text, "%s", name);
        }
        cJSON *item = cJSON_CreateString(text);
        ok = item && cJSON_AddItemToArray(list, item);
        if (!ok) {
            cJSON_Delete(item);
        }
    }
    common_names_free(&names);
    return ok;
}

UrsatzStatus workshop_scope_list(cJSON **out, UrsatzError *error) {
    *out = NULL;
    cJSON *root = cJSON_CreateObject();
    cJSON *tastes = root ? cJSON_AddArrayToObject(root, "tastes") : NULL;
    cJSON *styles = tastes ? cJSON_AddArrayToObject(root, "styles") : NULL;
    cJSON *projects = styles ? cJSON_AddArrayToObject(root, "projects") : NULL;
    cJSON *items = projects ? cJSON_AddArrayToObject(root, "items") : NULL;
    bool ok = items && add_names(tastes, WORKSHOP_DIR_TASTE, true, NULL) && add_names(styles, WORKSHOP_DIR_STYLES, false, WORKSHOP_FILE_SETTINGS) &&
              add_names(projects, WORKSHOP_DIR_PROJECTS, false, WORKSHOP_FILE_SETTINGS);
    CommonNames project_names;
    memset(&project_names, 0, sizeof(project_names));
    if (ok && common_dir_list(WORKSHOP_DIR_PROJECTS, &project_names, NULL) == URSATZ_OK) {
        for (size_t i = 0; ok && i < project_names.count; i++) {
            char folder[256];
            if (workshop_layout_items_dir(project_names.items[i], folder, sizeof(folder))) {
                ok = add_names(items, folder, false, WORKSHOP_FILE_SETTINGS);
            }
        }
    }
    common_names_free(&project_names);
    if (!ok) {
        cJSON_Delete(root);
        return refuse(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    *out = root;
    return URSATZ_OK;
}
