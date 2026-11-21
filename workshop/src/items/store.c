#include "base/command_context.h"
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <common/files.h>
#include "store_internal.h"
#include "formats.h"
#include "ids.h"
#include "layout.h"

bool store_ready = false;
WorkshopLock store_lock = WORKSHOP_LOCK_INIT;
static void (*g_forget_item)(WorkshopTxn *txn, const char *item_id);

void workshop_store_set_forget_item(void (*forget)(WorkshopTxn *txn, const char *item_id)) {
    g_forget_item = forget;
}

void store_forget_item(WorkshopTxn *txn, const char *item_id) {
    if (g_forget_item) {
        g_forget_item(txn, item_id);
    }
}

WorkshopTxn *store_txn_begin(const char *command, const char *target) {
    return workshop_txn_begin(NULL, command, "michelle", target, 0);
}

UrsatzStatus store_txn_end(WorkshopTxn *txn, UrsatzStatus result) {
    if (result != URSATZ_OK) {
        workshop_txn_abort(txn);
        return result;
    }
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    UrsatzStatus committed = workshop_txn_commit(txn, &error);
    return committed == URSATZ_OK ? URSATZ_OK : workshop_fail(URSATZ_ERR_INTERNAL, error.message[0] ? error.message : "material store failure");
}

bool store_is_valid_id(const char *id) {
    if (!id || id[0] == '\0' || strlen(id) > ID_MAX_BYTES) {
        return false;
    }
    for (const char *c = id; *c; c++) {
        if (!(isalnum((unsigned char)*c) || *c == '-' || *c == '_')) {
            return false;
        }
    }
    return true;
}

/* --- text ---------------------------------------------------------------- */

/*
 * Trims surrounding whitespace and counts UTF-8 characters. Returns a copy
 * of the trimmed text, or NULL when it has more than max_chars characters,
 * holds a control character, or on allocation failure (*alloc_failed set).
 */
static char *trim_text(const char *text, size_t max_chars, bool *alloc_failed) {
    *alloc_failed = false;
    char *copy = common_strdup(text ? text : "");
    if (!copy) {
        *alloc_failed = true;
        return NULL;
    }
    size_t length = common_trim(copy);
    size_t chars = 0;
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)copy[i];
        if (c < 0x20 || c == 0x7f || (chars += (c & 0xC0) != 0x80) > max_chars) {
            free(copy);
            return NULL;
        }
    }
    return copy;
}

static UrsatzStatus normalize_name(const char *text, char **out) {
    bool alloc_failed;
    *out = trim_text(text, NAME_MAX_CHARS, &alloc_failed);
    if (!*out) {
        return alloc_failed ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : workshop_fail(URSATZ_ERR_REQUEST, "invalid name, description or note");
    }
    if ((*out)[0] == '\0') {
        free(*out);
        *out = NULL;
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid name, description or note");
    }
    return URSATZ_OK;
}

static UrsatzStatus normalize_description(const char *text, char **out) {
    bool alloc_failed;
    *out = trim_text(text, DESCRIPTION_MAX_CHARS, &alloc_failed);
    if (!*out) {
        return alloc_failed ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : workshop_fail(URSATZ_ERR_REQUEST, "invalid name, description or note");
    }
    return URSATZ_OK;
}

/* --- paths --------------------------------------------------------------- */

static bool project_path(const char *id, char *out, size_t out_size) {
    return workshop_layout_project_file(id, out, out_size);
}

/* The project folder an item is in, found by looking in each project's items folder (a handful of stats). */
bool store_find_item_project(const char *id, char *project, size_t project_size) {
    if (!store_is_valid_id(id)) {
        return false;
    }
    CommonNames projects = {0};
    if (common_dir_list(WORKSHOP_DIR_PROJECTS, &projects, NULL) != URSATZ_OK) {
        return false;
    }
    bool found = false;
    for (size_t i = 0; !found && i < projects.count; i++) {
        char path[PATH_BYTES];
        if (workshop_layout_item_file(projects.items[i], id, WORKSHOP_FILE_ITEM, path, sizeof(path)) && common_path_exists(path)) {
            found = snprintf(project, project_size, "%s", projects.items[i]) < (int)project_size;
        }
    }
    common_names_free(&projects);
    return found;
}

/* Every item id of the store, across the projects. */
UrsatzStatus store_item_ids_locked(CommonNames *out) {
    CommonNames projects = {0};
    if (common_dir_list(WORKSHOP_DIR_PROJECTS, &projects, NULL) != URSATZ_OK) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    UrsatzStatus result = URSATZ_OK;
    for (size_t i = 0; result == URSATZ_OK && i < projects.count; i++) {
        char dir[PATH_BYTES];
        CommonNames names = {0};
        if (!workshop_layout_items_dir(projects.items[i], dir, sizeof(dir)) || common_dir_list(dir, &names, NULL) != URSATZ_OK) {
            continue;
        }
        for (size_t j = 0; result == URSATZ_OK && j < names.count; j++) {
            char path[PATH_BYTES];
            if (workshop_layout_item_file(projects.items[i], names.items[j], WORKSHOP_FILE_ITEM, path, sizeof(path)) &&
                common_path_exists(path) && !common_names_add(out, names.items[j])) {
                result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            }
        }
        common_names_free(&names);
    }
    common_names_free(&projects);
    if (result == URSATZ_OK) {
        common_names_sort(out);
    }
    return result;
}

/* --- projects ------------------------------------------------------------ */

/* Number of items in a project. */
static size_t count_items_locked(const char *project_id) {
    char dir[PATH_BYTES];
    CommonNames names = {0};
    size_t found = 0;
    if (!workshop_layout_items_dir(project_id, dir, sizeof(dir)) || common_dir_list(dir, &names, NULL) != URSATZ_OK) {
        return 0;
    }
    for (size_t i = 0; i < names.count; i++) {
        char path[PATH_BYTES];
        if (workshop_layout_item_file(project_id, names.items[i], WORKSHOP_FILE_ITEM, path, sizeof(path)) && common_path_exists(path)) {
            found++;
        }
    }
    common_names_free(&names);
    return found;
}

/* Loads projects/<id>/project.json; the folder name is the id, a copy inside the file is not trusted. */
static UrsatzStatus load_project_locked(const char *id, cJSON **out) {
    char path[PATH_BYTES];
    if (!project_path(id, path, sizeof(path))) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    cJSON *project = NULL;
    if (workshop_format_read(path, WORKSHOP_FORMAT_PROJECT, true, &project, NULL) != URSATZ_OK) {
        return common_path_exists(path) ? workshop_fail(URSATZ_ERR_INTERNAL, "material store failure") : workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    cJSON_DeleteItemFromObjectCaseSensitive(project, "id");
    cJSON *id_item = cJSON_CreateString(id);
    if (!id_item) {
        cJSON_Delete(project);
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    cJSON_AddItemToObject(project, "id", id_item);
    *out = project;
    return URSATZ_OK;
}

/* Stages a project: a new one is a folder (project.json and its empty items/), an old one only its file. */
static UrsatzStatus save_project_locked(WorkshopTxn *txn, const cJSON *project, bool is_new) {
    char path[PATH_BYTES];
    char dir[PATH_BYTES];
    const char *id = common_json_string(project, "id");
    if (!project_path(id, path, sizeof(path)) || !workshop_layout_project_dir(id, dir, sizeof(dir))) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    cJSON *copy = cJSON_Duplicate(project, 1);
    if (!copy) {
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    UrsatzStatus staged = URSATZ_OK;
    if (is_new) {
        const char *pending = NULL;
        char file[PATH_BYTES * 2];
        staged = workshop_txn_stage_folder(txn, dir, &pending);
        if (staged == URSATZ_OK) {
            snprintf(file, sizeof(file), "%s/items", pending);
            if (common_dir_make(file, NULL) != URSATZ_OK) {
                staged = URSATZ_ERR_IO;
            } else {
                snprintf(file, sizeof(file), "%s/" WORKSHOP_FILE_PROJECT, pending);
                staged = workshop_format_write(file, WORKSHOP_FORMAT_PROJECT, copy, NULL);
            }
        }
    } else {
        staged = workshop_txn_stage_json(txn, path, WORKSHOP_FORMAT_PROJECT, copy);
    }
    cJSON_Delete(copy);
    return staged == URSATZ_OK ? URSATZ_OK : workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
}

static UrsatzStatus create_project_locked(WorkshopTxn *txn, const char *name, const char *description, cJSON **out_project) {
    char *clean_name = NULL;
    char *clean_description = NULL;
    UrsatzStatus result = normalize_name(name, &clean_name);
    if (result == URSATZ_OK) {
        result = normalize_description(description, &clean_description);
    }
    if (result != URSATZ_OK) {
        free(clean_name);
        return result;
    }

    char *id = NULL;
    char path[PATH_BYTES];
    for (int attempt = 0; attempt < ID_MAX_TRIES; attempt++) {
        free(id);
        id = workshop_new_id("prj");
        if (!id) {
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            break;
        }
        if (workshop_layout_project_dir(id, path, sizeof(path)) && !common_path_exists(path)) {
            break;
        }
        free(id);
        id = NULL;
    }
    if (result == URSATZ_OK && !id) {
        result = workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }

    cJSON *project = NULL;
    if (result == URSATZ_OK) {
        char now[32];
        common_utc_now(now);
        project = cJSON_CreateObject();
        bool ok = project && cJSON_AddStringToObject(project, "id", id) &&
                  cJSON_AddStringToObject(project, "name", clean_name) &&
                  cJSON_AddStringToObject(project, "description", clean_description) &&
                  cJSON_AddStringToObject(project, "created_at", now) &&
                  cJSON_AddStringToObject(project, "updated_at", now);
        result = !ok ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : save_project_locked(txn, project, true);
    }
    if (result == URSATZ_OK && out_project) {
        *out_project = project;
    } else {
        cJSON_Delete(project);
    }
    free(id);
    free(clean_name);
    free(clean_description);
    return result;
}

bool workshop_material_service_init(void) {
    WorkshopTxn *txn = store_txn_begin("project.create", "");
    if (!txn) {
        return false;
    }
    LOCK();
    store_ready = false;
    bool ok = common_dir_make(WORKSHOP_DIR_PROJECTS, NULL) == URSATZ_OK;

    CommonNames names = {0};
    if (ok) {
        ok = (common_dir_list(WORKSHOP_DIR_PROJECTS, &names, NULL) == URSATZ_OK);
    }
    bool has_project = false;
    for (size_t i = 0; i < names.count; i++) {
        char path[PATH_BYTES];
        has_project = has_project || (workshop_layout_project_file(names.items[i], path, sizeof(path)) && common_path_exists(path));
    }
    common_names_free(&names);

    if (ok) {
        store_ready = true;
        if (!has_project) {
            ok = create_project_locked(txn, "General", "", NULL) == URSATZ_OK;
            store_ready = ok;
        }
    }
    UNLOCK();
    return store_txn_end(txn, ok ? URSATZ_OK : URSATZ_ERR_INTERNAL) == URSATZ_OK && ok;
}

void workshop_material_service_shutdown(void) {
    LOCK();
    store_ready = false;
    UNLOCK();
}

UrsatzStatus workshop_material_project_create(const char *name, const char *description,
                                                            cJSON **out_project) {
    if (out_project) {
        *out_project = NULL;
    }
    WorkshopTxn *txn = store_txn_begin("project.create", "");
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    LOCK();
    UrsatzStatus result = store_ready ? create_project_locked(txn, name, description, out_project)
                                      : workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    UNLOCK();
    result = store_txn_end(txn, result);
    if (result != URSATZ_OK && out_project) {
        cJSON_Delete(*out_project);
        *out_project = NULL;
    }
    return result;
}

static int compare_projects(const void *a, const void *b) {
    const cJSON *left = *(const cJSON *const *)a;
    const cJSON *right = *(const cJSON *const *)b;
    int by_name = common_compare_ci(common_json_string(left, "name"), common_json_string(right, "name"));
    return by_name != 0 ? by_name : strcmp(common_json_string(left, "id"), common_json_string(right, "id"));
}

UrsatzStatus workshop_material_project_list(cJSON **out_projects) {
    *out_projects = NULL;
    LOCK();
    if (!store_ready) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }

    CommonNames names = {0};
    if (common_dir_list(WORKSHOP_DIR_PROJECTS, &names, NULL) != URSATZ_OK) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }

    UrsatzStatus result = URSATZ_OK;
    cJSON **projects = names.count ? calloc(names.count, sizeof(cJSON *)) : NULL;
    size_t loaded = 0;
    if (names.count && !projects) {
        result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    for (size_t i = 0; result == URSATZ_OK && i < names.count; i++) {
        const char *id = names.items[i];
        cJSON *project = NULL;
        if (load_project_locked(id, &project) != URSATZ_OK) {
            continue;
        }
        if (!cJSON_AddNumberToObject(project, "item_count", (double)count_items_locked(id))) {
            cJSON_Delete(project);
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            break;
        }
        projects[loaded++] = project;
    }
    common_names_free(&names);
    UNLOCK();

    cJSON *array = result == URSATZ_OK ? cJSON_CreateArray() : NULL;
    if (result == URSATZ_OK && !array) {
        result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    if (loaded > 1) {
        qsort(projects, loaded, sizeof(cJSON *), compare_projects);
    }
    for (size_t i = 0; i < loaded; i++) {
        if (result == URSATZ_OK) {
            cJSON_AddItemToArray(array, projects[i]);
        } else {
            cJSON_Delete(projects[i]);
        }
    }
    free(projects);
    if (result == URSATZ_OK) {
        *out_projects = array;
    }
    return result;
}

UrsatzStatus workshop_material_project_get(const char *id, cJSON **out_project) {
    *out_project = NULL;
    LOCK();
    UrsatzStatus result = store_ready ? load_project_locked(id, out_project)
                                             : workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    if (result == URSATZ_OK &&
        !cJSON_AddNumberToObject(*out_project, "item_count", (double)count_items_locked(id))) {
        cJSON_Delete(*out_project);
        *out_project = NULL;
        result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    UNLOCK();
    return result;
}

UrsatzStatus workshop_material_project_update(const char *id, const char *name, const char *description,
                                                            cJSON **out_project) {
    if (out_project) {
        *out_project = NULL;
    }
    WorkshopTxn *txn = store_txn_begin("project.update", id ? id : "");
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    LOCK();
    if (!store_ready) {
        UNLOCK();
        workshop_txn_abort(txn);
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    cJSON *project = NULL;
    char *clean_name = NULL;
    char *clean_description = NULL;
    UrsatzStatus result = load_project_locked(id, &project);
    if (result == URSATZ_OK && name) {
        result = normalize_name(name, &clean_name);
    }
    if (result == URSATZ_OK && description) {
        result = normalize_description(description, &clean_description);
    }
    if (result == URSATZ_OK) {
        char now[32];
        common_utc_now(now);
        bool ok = (!clean_name || common_json_set_string(project, "name", clean_name)) &&
                  (!clean_description || common_json_set_string(project, "description", clean_description)) &&
                  common_json_set_string(project, "updated_at", now);
        result = ok ? save_project_locked(txn, project, false) : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    UNLOCK();
    result = store_txn_end(txn, result);
    if (result == URSATZ_OK && out_project) {
        *out_project = project;
    } else {
        cJSON_Delete(project);
    }
    free(clean_name);
    free(clean_description);
    return result;
}

UrsatzStatus workshop_material_project_delete(const char *id) {
    WorkshopTxn *txn = store_txn_begin("project.delete", id ? id : "");
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    LOCK();
    if (!store_ready) {
        UNLOCK();
        workshop_txn_abort(txn);
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    char path[PATH_BYTES];
    UrsatzStatus result = URSATZ_OK;
    if (!project_path(id, path, sizeof(path))) {
        result = workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    } else if (!common_path_exists(path)) {
        result = workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    } else if (count_items_locked(id) > 0) {
        result = workshop_fail(URSATZ_ERR_CONFLICT, "the project still holds items");
    } else {
        char dir[PATH_BYTES];
        if (!workshop_layout_project_dir(id, dir, sizeof(dir)) || workshop_txn_remove(txn, dir) != URSATZ_OK) {
            result = workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
        }
    }
    UNLOCK();
    return store_txn_end(txn, result);
}

/* --- items and versions -------------------------------------------------- */

const char *store_prefix_for_type(const char *type) {
    if (!type) {
        return NULL;
    }
    if (strcmp(type, "motif") == 0) {
        return "mot";
    }
    if (strcmp(type, "theme") == 0) {
        return "thm";
    }
    if (strcmp(type, "piece") == 0) {
        return "pce";
    }
    return NULL;
}

/* The folder of an item that exists; false for an id that is not one or an item that is not there. */
bool store_item_dir_path(const char *id, char *out, size_t out_size) {
    char project[ID_MAX_BYTES + 1];
    return store_find_item_project(id, project, sizeof(project)) && workshop_layout_item_dir(project, id, out, out_size);
}

bool store_item_file_path(const char *id, const char *file, char *out, size_t out_size) {
    char project[ID_MAX_BYTES + 1];
    return store_find_item_project(id, project, sizeof(project)) && workshop_layout_item_file(project, id, file, out, out_size);
}

bool store_version_dir_path(const char *id, int number, char *out, size_t out_size) {
    char project[ID_MAX_BYTES + 1];
    return store_find_item_project(id, project, sizeof(project)) && workshop_layout_version_dir(project, id, number, out, out_size);
}

static bool project_exists_locked(const char *project_id) {
    char path[PATH_BYTES];
    return project_path(project_id, path, sizeof(path)) && common_path_exists(path);
}

static const char *body_format_of(const char *type) {
    return strcmp(type, "motif") == 0   ? WORKSHOP_FORMAT_MOTIF_BODY
           : strcmp(type, "theme") == 0 ? WORKSHOP_FORMAT_THEME_BODY
                                        : WORKSHOP_FORMAT_PIECE_BODY;
}

int store_current_version_of(const cJSON *item) {
    const cJSON *number = cJSON_GetObjectItemCaseSensitive(item, "current_version");
    return cJSON_IsNumber(number) ? number->valueint : 0;
}

/* The versions of an item, {number, created_at, note, actor, lineage} each, from the envelopes of its version folders. */
static cJSON *versions_from_envelopes(const char *project, const char *id, int current) {
    cJSON *versions = cJSON_CreateArray();
    for (int number = 1; versions && number <= current; number++) {
        char path[PATH_BYTES];
        cJSON *envelope = NULL;
        if (!workshop_layout_version_file(project, id, number, WORKSHOP_FILE_ENVELOPE, path, sizeof(path)) ||
            workshop_format_read(path, WORKSHOP_FORMAT_ITEM_VERSION, true, &envelope, NULL) != URSATZ_OK) {
            continue;
        }
        cJSON *entry = cJSON_CreateObject();
        const cJSON *lineage = cJSON_GetObjectItemCaseSensitive(envelope, "lineage");
        if (!entry || !cJSON_AddNumberToObject(entry, "number", number) ||
            !cJSON_AddStringToObject(entry, "created_at", common_json_string(envelope, "created_at")) ||
            !cJSON_AddStringToObject(entry, "note", common_json_string(envelope, "note")) ||
            !cJSON_AddStringToObject(entry, "actor", common_json_string(envelope, "actor")) ||
            !cJSON_AddItemToObject(entry, "lineage", lineage ? cJSON_Duplicate(lineage, true) : cJSON_CreateNull())) {
            cJSON_Delete(entry);
            cJSON_Delete(versions);
            versions = NULL;
        } else {
            cJSON_AddItemToArray(versions, entry);
        }
        cJSON_Delete(envelope);
    }
    return versions;
}

/* The item as the services hold it: its header, the project the folder is in and the versions its envelopes give. */
UrsatzStatus store_load_item_locked(const char *id, cJSON **out) {
    char project[ID_MAX_BYTES + 1];
    char path[PATH_BYTES];
    if (!store_is_valid_id(id)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    if (!store_find_item_project(id, project, sizeof(project))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    cJSON *item = NULL;
    if (!workshop_layout_item_file(project, id, WORKSHOP_FILE_ITEM, path, sizeof(path)) ||
        workshop_format_read(path, WORKSHOP_FORMAT_ITEM, true, &item, NULL) != URSATZ_OK) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    cJSON *versions = versions_from_envelopes(project, id, store_current_version_of(item));
    if (!common_json_set_string(item, "id", id) || !common_json_set_string(item, "project_id", project) || !versions) {
        cJSON_Delete(versions);
        cJSON_Delete(item);
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    cJSON_AddItemToObject(item, "versions", versions);
    *out = item;
    return URSATZ_OK;
}

/* Sets the header of an item in txn (the members the services add for answers are left out by the transaction). */
static UrsatzStatus save_item_locked(WorkshopTxn *txn, const cJSON *item) {
    return workshop_txn_set_header(txn, item) == URSATZ_OK ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
}

static bool touch_item(cJSON *item) {
    char now[32];
    common_utc_now(now);
    return common_json_set_string(item, "updated_at", now);
}

/* Makes number the current version of the item object (the versions list is read from the envelopes again on the next load). */
static bool advance_item(cJSON *item, int number, const char *note) {
    char now[32];
    common_utc_now(now);
    cJSON *versions = cJSON_GetObjectItemCaseSensitive(item, "versions");
    if (!cJSON_IsArray(versions)) {
        cJSON_DeleteItemFromObjectCaseSensitive(item, "versions");
        versions = cJSON_AddArrayToObject(item, "versions");
    }
    cJSON *entry = cJSON_CreateObject();
    bool ok = versions && entry && cJSON_AddNumberToObject(entry, "number", number) &&
              cJSON_AddStringToObject(entry, "created_at", now) && cJSON_AddStringToObject(entry, "note", note);
    if (!ok) {
        cJSON_Delete(entry);
        return false;
    }
    cJSON_AddItemToArray(versions, entry);
    cJSON_DeleteItemFromObjectCaseSensitive(item, "current_version");
    return cJSON_AddNumberToObject(item, "current_version", number) != NULL && touch_item(item);
}

/* The lineage of a version: the version of another item a motif's body says it derives from, else none. */
static cJSON *lineage_of(const char *type, const cJSON *body) {
    cJSON *lineage = cJSON_CreateObject();
    const cJSON *derived =
        strcmp(type, "motif") == 0 ? cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(body, "motif"), "derived") : NULL;
    const char *source = common_json_string(derived, "source_item");
    long long version = 0;
    cJSON *from = NULL;
    if (source[0] != '\0' && common_json_long(derived, "source_version", &version) && version >= 1) {
        from = cJSON_CreateObject();
        if (!from || !cJSON_AddStringToObject(from, "item_id", source) || !cJSON_AddNumberToObject(from, "version", (double)version)) {
            cJSON_Delete(from);
            cJSON_Delete(lineage);
            return NULL;
        }
    } else {
        from = cJSON_CreateNull();
    }
    if (!lineage || !from) {
        cJSON_Delete(from);
        cJSON_Delete(lineage);
        return NULL;
    }
    cJSON_AddItemToObject(lineage, "derived_from", from);
    return lineage;
}

/* Writes the version folder's body, then its envelope (a version exists once its envelope does), into dir, which exists. */
static UrsatzStatus write_version_files_locked(const char *dir, const char *id, const char *type, int number, const cJSON *body,
                                               const char *note, int restored_from) {
    char path[PATH_BYTES];
    cJSON *stored = cJSON_Duplicate(body, 1);
    UrsatzStatus result = stored ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    if (result == URSATZ_OK) {
        result = common_path_join(path, sizeof(path), dir, WORKSHOP_FILE_BODY) &&
                         workshop_format_write(path, body_format_of(type), stored, NULL) == URSATZ_OK
                     ? URSATZ_OK
                     : workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    cJSON_Delete(stored);
    if (result != URSATZ_OK) {
        return result;
    }
    char now[32];
    common_utc_now(now);
    cJSON *envelope = cJSON_CreateObject();
    cJSON *lineage = lineage_of(type, body);
    if (lineage && restored_from > 0) {
        cJSON_AddNumberToObject(lineage, "restored_from", restored_from);
    }
    bool attached = false;
    /* the command's actor (a job runs under the one that queued it); a version written with no command (a migration) is Michelle's */
    const WorkshopCommandContext *context = workshop_command_context();
    bool ok = envelope && lineage && cJSON_AddStringToObject(envelope, "id", id) && cJSON_AddStringToObject(envelope, "kind", type) &&
              cJSON_AddNumberToObject(envelope, "version", number) && cJSON_AddStringToObject(envelope, "note", note) &&
              (attached = cJSON_AddItemToObject(envelope, "lineage", lineage)) && cJSON_AddStringToObject(envelope, "actor", context ? context->actor : "michelle") &&
              cJSON_AddStringToObject(envelope, "created_at", now);
    if (!ok) {
        cJSON_Delete(envelope);
        if (!attached) {
            cJSON_Delete(lineage);
        }
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    result = common_path_join(path, sizeof(path), dir, WORKSHOP_FILE_ENVELOPE) &&
                     workshop_format_write(path, WORKSHOP_FORMAT_ITEM_VERSION, envelope, NULL) == URSATZ_OK
                 ? URSATZ_OK
                 : workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    cJSON_Delete(envelope);
    return result;
}

UrsatzStatus store_read_version_locked(const char *id, int number, cJSON **out_body) {
    char path[PATH_BYTES];
    char project[ID_MAX_BYTES + 1];
    if (!store_is_valid_id(id) || number < 1) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    if (!store_find_item_project(id, project, sizeof(project))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    cJSON *body = NULL;
    if (!workshop_layout_version_file(project, id, number, WORKSHOP_FILE_BODY, path, sizeof(path)) ||
        workshop_format_read(path, NULL, true, &body, NULL) != URSATZ_OK) {
        return common_path_exists(path) ? workshop_fail(URSATZ_ERR_INTERNAL, "material store failure") : workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    *out_body = body;
    return URSATZ_OK;
}

static UrsatzStatus create_item_locked(WorkshopTxn *txn, const char *type, const char *name, const char *project_id,
                                                  const cJSON *body, const char *note, cJSON **out_item) {
    const char *prefix = store_prefix_for_type(type);
    if (!prefix) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid type");
    }
    if (!store_is_valid_id(project_id)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    char *clean_name = NULL;
    char *clean_note = NULL;
    UrsatzStatus result = normalize_name(name, &clean_name);
    if (result == URSATZ_OK) {
        bool alloc_failed = false;
        clean_note = trim_text(note, NOTE_MAX_CHARS, &alloc_failed);
        if (!clean_note) {
            result = alloc_failed ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : workshop_fail(URSATZ_ERR_REQUEST, "invalid name, description or note");
        }
    }
    if (result == URSATZ_OK && !project_exists_locked(project_id)) {
        result = workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    cJSON *stored = NULL;
    if (result == URSATZ_OK) {
        result = workshop_material_body_validate(type, body, NULL);
    }
    if (result == URSATZ_OK) {
        result = store_fill_body_uses_locked(NULL, body, &stored);
    }

    char *id = NULL;
    char dir[PATH_BYTES];
    char probe[ID_MAX_BYTES + 1];
    for (int attempt = 0; result == URSATZ_OK && attempt < ID_MAX_TRIES; attempt++) {
        id = workshop_new_id(prefix);
        if (!id) {
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        } else if (workshop_layout_item_dir(project_id, id, dir, sizeof(dir)) && !store_find_item_project(id, probe, sizeof(probe)) &&
                   !common_path_exists(dir)) {
            break;
        } else {
            free(id);
            id = NULL;
        }
    }
    if (result == URSATZ_OK && !id) {
        result = workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }

    cJSON *item = NULL;
    if (result == URSATZ_OK) {
        char now[32];
        char version_dir[PATH_BYTES * 2];
        const char *pending = NULL;
        common_utc_now(now);
        item = cJSON_CreateObject();
        bool ok = item && cJSON_AddStringToObject(item, "id", id) && cJSON_AddStringToObject(item, "type", type) &&
                  cJSON_AddStringToObject(item, "name", clean_name) &&
                  cJSON_AddStringToObject(item, "project_id", project_id) &&
                  cJSON_AddStringToObject(item, "created_at", now) && cJSON_AddStringToObject(item, "updated_at", now) &&
                  advance_item(item, 1, clean_note);
        if (!ok) {
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        } else if (workshop_txn_stage_folder(txn, dir, &pending) != URSATZ_OK ||
                   snprintf(version_dir, sizeof(version_dir), "%s/v1", pending) >= (int)sizeof(version_dir) ||
                   common_dir_make(version_dir, NULL) != URSATZ_OK) {
            result = workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
        } else {
            /* the folder holds the version; the header, which makes it an item, is set by the transaction after the folder is in place */
            result = write_version_files_locked(version_dir, id, type, 1, stored, clean_note, 0);
            if (result == URSATZ_OK) {
                result = save_item_locked(txn, item);
            }
        }
    }
    if (result == URSATZ_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    cJSON_Delete(stored);
    free(id);
    free(clean_name);
    free(clean_note);
    return result;
}

UrsatzStatus workshop_material_item_create_noted(const char *type, const char *name,
                                                               const char *project_id, const cJSON *body,
                                                               const char *note, cJSON **out_item) {
    if (out_item) {
        *out_item = NULL;
    }
    WorkshopTxn *txn = store_txn_begin("item.create", "");
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    LOCK();
    UrsatzStatus result = store_ready ? create_item_locked(txn, type, name, project_id, body, note, out_item)
                                             : workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    UNLOCK();
    result = store_txn_end(txn, result);
    if (result != URSATZ_OK && out_item) {
        cJSON_Delete(*out_item);
        *out_item = NULL;
    }
    return result;
}

static int compare_items(const void *a, const void *b) {
    const cJSON *left = *(const cJSON *const *)a;
    const cJSON *right = *(const cJSON *const *)b;
    int by_updated = strcmp(common_json_string(right, "updated_at"), common_json_string(left, "updated_at"));
    return by_updated != 0 ? by_updated : strcmp(common_json_string(left, "id"), common_json_string(right, "id"));
}

UrsatzStatus workshop_material_item_list(const char *project_id, const char *type, cJSON **out_items) {
    *out_items = NULL;
    if (project_id && !store_is_valid_id(project_id)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    if (type && !store_prefix_for_type(type)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid type");
    }
    LOCK();
    if (!store_ready) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }

    CommonNames names = {0};
    UrsatzStatus result = store_item_ids_locked(&names);
    if (result != URSATZ_OK) {
        UNLOCK();
        common_names_free(&names);
        return result;
    }

    cJSON **items = names.count ? calloc(names.count, sizeof(cJSON *)) : NULL;
    size_t loaded = 0;
    if (names.count && !items) {
        result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    for (size_t i = 0; result == URSATZ_OK && i < names.count; i++) {
        cJSON *item = NULL;
        if (store_load_item_locked(names.items[i], &item) != URSATZ_OK) {
            continue;
        }
        if ((project_id && strcmp(common_json_string(item, "project_id"), project_id) != 0) ||
            (type && strcmp(common_json_string(item, "type"), type) != 0)) {
            cJSON_Delete(item);
            continue;
        }
        if (!store_annotate_variant_of_locked(item, NULL)) {
            cJSON_Delete(item);
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            break;
        }
        items[loaded++] = item;
    }
    common_names_free(&names);
    UNLOCK();

    cJSON *array = result == URSATZ_OK ? cJSON_CreateArray() : NULL;
    if (result == URSATZ_OK && !array) {
        result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    if (loaded > 1) {
        qsort(items, loaded, sizeof(cJSON *), compare_items);
    }
    for (size_t i = 0; i < loaded; i++) {
        if (result == URSATZ_OK) {
            cJSON_AddItemToArray(array, items[i]);
        } else {
            cJSON_Delete(items[i]);
        }
    }
    free(items);
    if (result == URSATZ_OK) {
        *out_items = array;
    }
    return result;
}

UrsatzStatus workshop_material_item_get(const char *id, cJSON **out_meta, cJSON **out_body) {
    if (out_meta) {
        *out_meta = NULL;
    }
    if (out_body) {
        *out_body = NULL;
    }
    LOCK();
    if (!store_ready) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    cJSON *item = NULL;
    cJSON *body = NULL;
    UrsatzStatus result = store_load_item_locked(id, &item);
    if (result == URSATZ_OK && out_body) {
        result = store_read_version_locked(id, store_current_version_of(item), &body);
    }
    if (result == URSATZ_OK && out_meta && !store_annotate_variant_of_locked(item, body)) {
        result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    UNLOCK();
    if (result == URSATZ_OK && out_meta) {
        *out_meta = item;
    } else {
        cJSON_Delete(item);
    }
    if (result == URSATZ_OK && out_body) {
        *out_body = body;
    }
    return result;
}

UrsatzStatus workshop_material_item_get_version(const char *id, int number, cJSON **out_body) {
    *out_body = NULL;
    LOCK();
    if (!store_ready) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    cJSON *item = NULL;
    UrsatzStatus result = store_load_item_locked(id, &item);
    if (result == URSATZ_OK) {
        result = number >= 1 && number <= store_current_version_of(item) ? store_read_version_locked(id, number, out_body)
                                                                    : workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    cJSON_Delete(item);
    UNLOCK();
    return result;
}

UrsatzStatus workshop_material_version_envelope(const char *id, int number, cJSON **out_envelope) {
    *out_envelope = NULL;
    LOCK();
    if (!store_ready) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    cJSON *item = NULL;
    UrsatzStatus result = store_load_item_locked(id, &item);
    char path[PATH_BYTES];
    if (result == URSATZ_OK &&
        (number < 1 || number > store_current_version_of(item) ||
         !workshop_layout_version_file(common_json_string(item, "project_id"), id, number, WORKSHOP_FILE_ENVELOPE, path, sizeof(path)))) {
        result = workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    if (result == URSATZ_OK && workshop_format_read(path, WORKSHOP_FORMAT_ITEM_VERSION, true, out_envelope, NULL) != URSATZ_OK) {
        result = workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    cJSON_Delete(item);
    UNLOCK();
    return result;
}

/* Copies the folder from into the existing folder to, with everything under it. */
static bool copy_folder(const char *from, const char *to) {
    CommonNames names = {0};
    if (common_dir_list(from, &names, NULL) != URSATZ_OK) {
        return false;
    }
    bool ok = true;
    for (size_t i = 0; ok && i < names.count; i++) {
        char source[PATH_BYTES * 2];
        char target[PATH_BYTES * 2];
        if (!common_path_join(source, sizeof(source), from, names.items[i]) ||
            !common_path_join(target, sizeof(target), to, names.items[i])) {
            ok = false;
        } else if (common_path_is_dir(source)) {
            ok = common_dir_make(target, NULL) == URSATZ_OK && copy_folder(source, target);
        } else {
            ok = common_file_copy(source, target, NULL) == URSATZ_OK;
        }
    }
    common_names_free(&names);
    return ok;
}

/* Sets the name and/or project (each NULL = unchanged) on the item and saves it; a new project is a rename of the item's folder. */
static UrsatzStatus change_item(const char *command, const char *id, const char *name, const char *project_id,
                                           cJSON **out_item) {
    if (out_item) {
        *out_item = NULL;
    }
    WorkshopTxn *txn = store_txn_begin(command, id ? id : "");
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    LOCK();
    if (!store_ready) {
        UNLOCK();
        workshop_txn_abort(txn);
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    cJSON *item = NULL;
    char *clean_name = NULL;
    UrsatzStatus result = store_load_item_locked(id, &item);
    if (result == URSATZ_OK && name) {
        result = normalize_name(name, &clean_name);
    }
    if (result == URSATZ_OK && project_id) {
        if (!store_is_valid_id(project_id)) {
            result = workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
        } else if (!project_exists_locked(project_id)) {
            result = workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
        }
    }
    if (result == URSATZ_OK) {
        bool ok = (!clean_name || common_json_set_string(item, "name", clean_name)) && touch_item(item);
        if (ok && project_id && strcmp(project_id, common_json_string(item, "project_id")) != 0) {
            /* the folder moves as a copy staged in the new project and the old one removed, in one record */
            char from[PATH_BYTES];
            char to[PATH_BYTES];
            char items[PATH_BYTES];
            const char *pending = NULL;
            if (!workshop_layout_item_dir(common_json_string(item, "project_id"), id, from, sizeof(from)) ||
                !workshop_layout_item_dir(project_id, id, to, sizeof(to)) || !workshop_layout_items_dir(project_id, items, sizeof(items)) ||
                common_dir_make(items, NULL) != URSATZ_OK || workshop_txn_stage_folder(txn, to, &pending) != URSATZ_OK ||
                !copy_folder(from, pending) || workshop_txn_remove(txn, from) != URSATZ_OK) {
                result = workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
            } else {
                ok = common_json_set_string(item, "project_id", project_id);
            }
        }
        if (result == URSATZ_OK) {
            result = ok ? save_item_locked(txn, item) : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        }
    }
    UNLOCK();
    result = store_txn_end(txn, result);
    if (result == URSATZ_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    free(clean_name);
    return result;
}

UrsatzStatus workshop_material_item_rename(const char *id, const char *name, cJSON **out_item) {
    if (!name) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid name, description or note");
    }
    return change_item("item.rename", id, name, NULL, out_item);
}

UrsatzStatus workshop_material_item_move(const char *id, const char *project_id, cJSON **out_item) {
    if (!project_id) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    return change_item("item.move", id, NULL, project_id, out_item);
}

/*
 * Stages body as the next version of item and its header with it made current, in txn. The version folder is the stage (a folder of the
 * files a compose made, which becomes the pending folder) when there is one, else a new folder; the body and the envelope go into it and the
 * header is set last, so a version a header names has everything.
 */
UrsatzStatus store_commit_version_locked(WorkshopTxn *txn, const char *id, cJSON *item, const cJSON *body, const char *note, const char *stage,
                                        int restored_from) {
    /* A command made against a version of this item (base/command_context.h) writes only on top of it: a job that composes finds, when it
     * comes to write, that another change got in first, and fails with the refusal a stale base version gets at submit. */
    const WorkshopCommandContext *context = workshop_command_context();
    if (context && context->base_version >= 0 && strcmp(context->target, id) == 0 && store_current_version_of(item) != context->base_version) {
        UrsatzError refusal;
        memset(&refusal, 0, sizeof(refusal));
        snprintf(refusal.message, sizeof(refusal.message), "%s", WORKSHOP_CHANGED_SINCE_READ);
        snprintf(refusal.path, sizeof(refusal.path), "/base_version");
        if (stage) {
            common_dir_remove(stage, NULL); /* the stage is gone on a failure of the commit too */
        }
        return workshop_fail_with(URSATZ_ERR_CONFLICT, &refusal);
    }
    int number = store_current_version_of(item) + 1;
    const char *project = common_json_string(item, "project_id");
    char dir[PATH_BYTES];
    if (!workshop_layout_version_dir(project, id, number, dir, sizeof(dir))) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    if (!advance_item(item, number, note)) {
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    const char *pending = NULL;
    /* a folder no version named is left over from a failed run */
    UrsatzStatus staged = workshop_txn_remove(txn, dir);
    if (staged == URSATZ_OK) {
        staged = stage ? workshop_txn_adopt_folder(txn, stage, dir, &pending) : workshop_txn_stage_folder(txn, dir, &pending);
    }
    if (staged != URSATZ_OK) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    UrsatzStatus result = write_version_files_locked(pending, id, common_json_string(item, "type"), number, body, note, restored_from);
    result = result == URSATZ_OK ? save_item_locked(txn, item) : result;
    if (result == URSATZ_OK && context && context->base_version >= 0 && strcmp(context->target, id) == 0) {
        workshop_command_context_set_base_version(number);
    }
    return result;
}

static UrsatzStatus add_version(const char *id, const cJSON *body, const char *note, const char *stage, int restored_from, cJSON **out_item) {
    if (out_item) {
        *out_item = NULL;
    }
    WorkshopTxn *txn = store_txn_begin(restored_from > 0 ? "item.restore" : "item.save", id ? id : "");
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    LOCK();
    if (!store_ready) {
        UNLOCK();
        workshop_txn_abort(txn);
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    cJSON *item = NULL;
    char *clean_note = NULL;
    bool alloc_failed = false;
    UrsatzStatus result = store_load_item_locked(id, &item);
    if (result == URSATZ_OK) {
        clean_note = trim_text(note, NOTE_MAX_CHARS, &alloc_failed);
        if (!clean_note) {
            result = alloc_failed ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : workshop_fail(URSATZ_ERR_REQUEST, "invalid name, description or note");
        }
    }
    if (result == URSATZ_OK) {
        result = workshop_material_body_validate(common_json_string(item, "type"), body, NULL);
    }
    cJSON *stored = NULL;
    if (result == URSATZ_OK) {
        /* a restored version keeps the uses of the version it copies (R-667) */
        if (restored_from > 0) {
            stored = cJSON_Duplicate(body, true);
            result = stored ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        } else {
            result = store_fill_body_uses_locked(id, body, &stored);
        }
    }
    if (result == URSATZ_OK) {
        result = store_commit_version_locked(txn, id, item, stored, clean_note, stage, restored_from);
    }
    UNLOCK();
    result = store_txn_end(txn, result);
    if (result == URSATZ_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    cJSON_Delete(stored);
    free(clean_note);
    return result;
}

UrsatzStatus workshop_material_item_add_version_staged(const char *id, const cJSON *body, const char *note, const char *stage,
                                                       cJSON **out_item) {
    return add_version(id, body, note, stage, 0, out_item);
}

UrsatzStatus workshop_material_item_add_version_restored(const char *id, const cJSON *body, const char *note, const char *stage, int restored_from,
                                                         cJSON **out_item) {
    return add_version(id, body, note, stage, restored_from, out_item);
}

UrsatzStatus workshop_material_version_copy(const char *id, int number, const char *to_folder, char *from_absolute, size_t size) {
    LOCK();
    cJSON *item = NULL;
    char dir[PATH_BYTES];
    UrsatzStatus result = store_ready ? store_load_item_locked(id, &item) : workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    if (result == URSATZ_OK &&
        (number < 1 || number > store_current_version_of(item) || !store_version_dir_path(id, number, dir, sizeof(dir)) || !common_path_is_dir(dir))) {
        result = workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    char resolved[PATH_MAX];
    if (result == URSATZ_OK && (!realpath(dir, resolved) || snprintf(from_absolute, size, "%s", resolved) >= (int)size || !copy_folder(dir, to_folder))) {
        result = workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    if (result == URSATZ_OK) {
        /* the envelope is the new version's own */
        char envelope[PATH_BYTES * 2];
        if (common_path_join(envelope, sizeof(envelope), to_folder, WORKSHOP_FILE_ENVELOPE)) {
            unlink(envelope);
        }
    }
    cJSON_Delete(item);
    UNLOCK();
    return result;
}

UrsatzStatus workshop_material_item_add_version(const char *id, const cJSON *body, const char *note,
                                                              cJSON **out_item) {
    return workshop_material_item_add_version_staged(id, body, note, NULL, out_item);
}
