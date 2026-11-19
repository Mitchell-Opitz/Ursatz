#include "store_internal.h"
#include "composition/motifs.h"
#include "ids.h"

char store_dir[300];
bool store_ready = false;
WorkshopLock store_lock = WORKSHOP_LOCK_INIT;

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

/* True when name ends in suffix and has at least one character before it. */
static bool has_suffix(const char *name, const char *suffix) {
    size_t name_len = strlen(name);
    size_t suffix_len = strlen(suffix);
    return name_len > suffix_len && strcmp(name + name_len - suffix_len, suffix) == 0;
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
    return store_is_valid_id(id) && snprintf(out, out_size, "%s/projects/%s.json", store_dir, id) < (int)out_size;
}

/* --- projects ------------------------------------------------------------ */

/* Number of items whose item.json names project_id. */
static size_t count_items_locked(const char *project_id) {
    char dir[PATH_BYTES];
    CommonNames names = {0};
    size_t found = 0;
    if (snprintf(dir, sizeof(dir), "%s/items", store_dir) >= (int)sizeof(dir) || common_dir_list(dir, &names, NULL) != URSATZ_OK) {
        return 0;
    }
    for (size_t i = 0; i < names.count; i++) {
        char path[PATH_BYTES];
        if (!store_is_valid_id(names.items[i]) ||
            snprintf(path, sizeof(path), "%s/items/%s/item.json", store_dir, names.items[i]) >= (int)sizeof(path)) {
            continue;
        }
        cJSON *meta = NULL;
        common_json_read_object(path, &meta, NULL);
        if (meta && strcmp(common_json_string(meta, "project_id"), project_id) == 0) {
            found++;
        }
        cJSON_Delete(meta);
    }
    common_names_free(&names);
    return found;
}

/* Loads projects/<id>.json; the file name is the id, a copy inside the file is not trusted. */
static UrsatzStatus load_project_locked(const char *id, cJSON **out) {
    char path[PATH_BYTES];
    if (!project_path(id, path, sizeof(path))) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    cJSON *project = NULL;
    common_json_read_object(path, &project, NULL);
    if (!project) {
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

static UrsatzStatus save_project_locked(const cJSON *project) {
    char path[PATH_BYTES];
    if (!project_path(common_json_string(project, "id"), path, sizeof(path))) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    return (common_json_write_file(path, project, true, NULL) == URSATZ_OK) ? URSATZ_OK : workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
}


static UrsatzStatus create_project_locked(const char *name, const char *description, cJSON **out_project) {
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
        if (project_path(id, path, sizeof(path)) && !common_path_exists(path)) {
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
        result = !ok ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : save_project_locked(project);
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

bool workshop_material_service_init(const char *dir) {
    if (!dir || dir[0] == '\0') {
        return false;
    }
    LOCK();
    store_ready = false;
    char projects[PATH_BYTES];
    char items[PATH_BYTES];
    bool ok = snprintf(store_dir, sizeof(store_dir), "%s", dir) < (int)sizeof(store_dir) &&
              snprintf(projects, sizeof(projects), "%s/projects", dir) < (int)sizeof(projects) &&
              snprintf(items, sizeof(items), "%s/items", dir) < (int)sizeof(items) && (common_dir_make(store_dir, NULL) == URSATZ_OK) &&
              (common_dir_make(projects, NULL) == URSATZ_OK) && (common_dir_make(items, NULL) == URSATZ_OK);

    CommonNames names = {0};
    if (ok) {
        ok = (common_dir_list(projects, &names, NULL) == URSATZ_OK);
    }
    bool has_project = false;
    for (size_t i = 0; i < names.count; i++) {
        has_project = has_project || has_suffix(names.items[i], ".json");
    }
    common_names_free(&names);

    if (ok) {
        store_ready = true;
        if (!has_project) {
            ok = create_project_locked("General", "", NULL) == URSATZ_OK;
            store_ready = ok;
        }
    }
    UNLOCK();
    return ok;
}

void workshop_material_service_shutdown(void) {
    LOCK();
    store_ready = false;
    store_dir[0] = '\0';
    UNLOCK();
}

UrsatzStatus workshop_material_project_create(const char *name, const char *description,
                                                            cJSON **out_project) {
    if (out_project) {
        *out_project = NULL;
    }
    LOCK();
    UrsatzStatus result =
        store_ready ? create_project_locked(name, description, out_project) : workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    UNLOCK();
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

    char dir[PATH_BYTES];
    CommonNames names = {0};
    if (snprintf(dir, sizeof(dir), "%s/projects", store_dir) >= (int)sizeof(dir) || common_dir_list(dir, &names, NULL) != URSATZ_OK) {
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
        if (!has_suffix(names.items[i], ".json")) {
            continue;
        }
        char id[ID_MAX_BYTES + 1];
        size_t id_len = strlen(names.items[i]) - strlen(".json");
        if (id_len > ID_MAX_BYTES) {
            continue;
        }
        memcpy(id, names.items[i], id_len);
        id[id_len] = '\0';
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
    LOCK();
    if (!store_ready) {
        UNLOCK();
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
        result = ok ? save_project_locked(project) : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    if (result == URSATZ_OK && out_project) {
        *out_project = project;
    } else {
        cJSON_Delete(project);
    }
    UNLOCK();
    free(clean_name);
    free(clean_description);
    return result;
}

UrsatzStatus workshop_material_project_delete(const char *id) {
    LOCK();
    if (!store_ready) {
        UNLOCK();
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
    } else if (remove(path) != 0) {
        result = workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    UNLOCK();
    return result;
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

bool store_item_dir_path(const char *id, char *out, size_t out_size) {
    return store_is_valid_id(id) && snprintf(out, out_size, "%s/items/%s", store_dir, id) < (int)out_size;
}

bool store_item_file_path(const char *id, const char *file, char *out, size_t out_size) {
    return store_is_valid_id(id) && snprintf(out, out_size, "%s/items/%s/%s", store_dir, id, file) < (int)out_size;
}

static bool version_path(const char *id, int number, char *out, size_t out_size) {
    return store_is_valid_id(id) && number >= 1 && snprintf(out, out_size, "%s/items/%s/v%d.json", store_dir, id, number) < (int)out_size;
}

static bool project_exists_locked(const char *project_id) {
    char path[PATH_BYTES];
    return project_path(project_id, path, sizeof(path)) && common_path_exists(path);
}

UrsatzStatus store_load_item_locked(const char *id, cJSON **out) {
    char path[PATH_BYTES];
    if (!store_item_file_path(id, "item.json", path, sizeof(path))) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    cJSON *item = NULL;
    common_json_read_object(path, &item, NULL);
    if (!item) {
        return common_path_exists(path) ? workshop_fail(URSATZ_ERR_INTERNAL, "material store failure") : workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    if (!common_json_set_string(item, "id", id)) {
        cJSON_Delete(item);
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    *out = item;
    return URSATZ_OK;
}

static UrsatzStatus save_item_locked(const cJSON *item) {
    char path[PATH_BYTES];
    if (!store_item_file_path(common_json_string(item, "id"), "item.json", path, sizeof(path))) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    return (common_json_write_file(path, item, true, NULL) == URSATZ_OK) ? URSATZ_OK : workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
}

int store_current_version_of(const cJSON *item) {
    const cJSON *number = cJSON_GetObjectItemCaseSensitive(item, "current_version");
    return cJSON_IsNumber(number) ? number->valueint : 0;
}

static bool touch_item(cJSON *item) {
    char now[32];
    common_utc_now(now);
    return common_json_set_string(item, "updated_at", now);
}

static bool write_version_locked(const char *id, int number, const cJSON *body) {
    char path[PATH_BYTES];
    return version_path(id, number, path, sizeof(path)) && (common_json_write_file(path, body, true, NULL) == URSATZ_OK);
}

/* Appends {number, created_at, note} to the item's versions and makes it current. */
static bool append_version_entry(cJSON *item, int number, const char *note) {
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

UrsatzStatus store_read_version_locked(const char *id, int number, cJSON **out_body) {
    char path[PATH_BYTES];
    if (!version_path(id, number, path, sizeof(path))) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    cJSON *body = NULL;
    common_json_read_object(path, &body, NULL);
    if (!body) {
        return common_path_exists(path) ? workshop_fail(URSATZ_ERR_INTERNAL, "material store failure") : workshop_fail(URSATZ_ERR_NOT_FOUND, "not found");
    }
    workshop_material_body_migrate(body);
    *out_body = body;
    return URSATZ_OK;
}

static UrsatzStatus create_item_locked(const char *type, const char *name, const char *project_id,
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
    for (int attempt = 0; result == URSATZ_OK && attempt < ID_MAX_TRIES; attempt++) {
        id = workshop_new_id(prefix);
        if (!id) {
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        } else if (store_item_dir_path(id, dir, sizeof(dir)) && !common_path_exists(dir)) {
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
    bool dir_made = false;
    if (result == URSATZ_OK) {
        char now[32];
        common_utc_now(now);
        item = cJSON_CreateObject();
        bool ok = item && cJSON_AddStringToObject(item, "id", id) && cJSON_AddStringToObject(item, "type", type) &&
                  cJSON_AddStringToObject(item, "name", clean_name) &&
                  cJSON_AddStringToObject(item, "project_id", project_id) &&
                  cJSON_AddStringToObject(item, "created_at", now) && cJSON_AddStringToObject(item, "updated_at", now) &&
                  append_version_entry(item, 1, clean_note);
        if (!ok) {
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
        } else if (!(common_dir_create(dir, NULL) == URSATZ_OK)) {
            result = workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
        } else {
            dir_made = true;
            /* item.json goes last: a directory without it is not an item. */
            if (!write_version_locked(id, 1, stored)) {
                result = workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
            } else {
                result = save_item_locked(item);
            }
        }
    }
    if (result != URSATZ_OK && dir_made) {
        common_dir_remove(dir, NULL);
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
    LOCK();
    UrsatzStatus result = store_ready ? create_item_locked(type, name, project_id, body, note, out_item)
                                             : workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    UNLOCK();
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

    char dir[PATH_BYTES];
    CommonNames names = {0};
    if (snprintf(dir, sizeof(dir), "%s/items", store_dir) >= (int)sizeof(dir) || common_dir_list(dir, &names, NULL) != URSATZ_OK) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }

    UrsatzStatus result = URSATZ_OK;
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

/* Sets the name and/or project (each NULL = unchanged) on the item and saves it. */
static UrsatzStatus change_item(const char *id, const char *name, const char *project_id,
                                           cJSON **out_item) {
    if (out_item) {
        *out_item = NULL;
    }
    LOCK();
    if (!store_ready) {
        UNLOCK();
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
        bool ok = (!clean_name || common_json_set_string(item, "name", clean_name)) &&
                  (!project_id || common_json_set_string(item, "project_id", project_id)) && touch_item(item);
        result = ok ? save_item_locked(item) : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    if (result == URSATZ_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    UNLOCK();
    free(clean_name);
    return result;
}

UrsatzStatus workshop_material_item_rename(const char *id, const char *name, cJSON **out_item) {
    if (!name) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid name, description or note");
    }
    return change_item(id, name, NULL, out_item);
}

UrsatzStatus workshop_material_item_move(const char *id, const char *project_id, cJSON **out_item) {
    if (!project_id) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    return change_item(id, NULL, project_id, out_item);
}

/* Writes body as the next version of item and saves item.json with it made current. */
UrsatzStatus store_commit_version_locked(const char *id, cJSON *item, const cJSON *body, const char *note) {
    int number = store_current_version_of(item) + 1;
    /* The body goes first; the version exists once item.json names it. */
    if (!append_version_entry(item, number, note)) {
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    if (!write_version_locked(id, number, body)) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "material store failure");
    }
    return save_item_locked(item);
}

UrsatzStatus workshop_material_item_add_version(const char *id, const cJSON *body, const char *note,
                                                              cJSON **out_item) {
    if (out_item) {
        *out_item = NULL;
    }
    LOCK();
    if (!store_ready) {
        UNLOCK();
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
        result = store_fill_body_uses_locked(id, body, &stored);
    }
    if (result == URSATZ_OK) {
        result = store_commit_version_locked(id, item, stored, clean_note);
    }
    if (result == URSATZ_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    UNLOCK();
    cJSON_Delete(stored);
    free(clean_note);
    return result;
}
