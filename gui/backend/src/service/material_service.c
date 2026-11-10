#include "material_service.h"
#include "util/fs_util.h"
#include "util/json_util.h"
#include "util/store_lock.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "composer_client.h"
#include "id_service.h"
#include "motif_service.h"
#include "subject_service.h"

#define NAME_MAX_CHARS 200
#define NOTE_MAX_CHARS 500
#define DESCRIPTION_MAX_CHARS 2000
#define ID_MAX_BYTES 64
#define ID_MAX_TRIES 8
#define PATH_BYTES 400

static char g_dir[300];
static bool g_ready = false;

static UrsatzGuiLock g_lock = URSATZ_GUI_LOCK_INIT;
#define LOCK() ursatz_gui_lock_acquire(&g_lock)
#define UNLOCK() ursatz_gui_lock_release(&g_lock)


static bool write_json(const char *path, const cJSON *json) {
    char *text = cJSON_Print(json);
    if (!text) {
        return false;
    }
    bool ok = ursatz_gui_write_file_atomic(path, text, strlen(text));
    free(text);
    return ok;
}

/* The names in dir other than "." and "..", sorted by strcmp. *out_names is NULL when there are none. */
static bool list_names(const char *dir, char ***out_names, size_t *out_count) {
    char **names = NULL;
    size_t count = 0;
    size_t capacity = 0;
    *out_names = NULL;
    *out_count = 0;

#ifdef _WIN32
    char pattern[PATH_BYTES + 4];
    if (snprintf(pattern, sizeof(pattern), "%s\\*", dir) >= (int)sizeof(pattern)) {
        return false;
    }
    WIN32_FIND_DATAA find_data;
    HANDLE handle = FindFirstFileA(pattern, &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        return GetLastError() == ERROR_FILE_NOT_FOUND;
    }
    do {
        const char *name = find_data.cFileName;
#else
    DIR *handle = opendir(dir);
    if (!handle) {
        return false;
    }
    struct dirent *entry;
    while ((entry = readdir(handle)) != NULL) {
        const char *name = entry->d_name;
#endif
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
            if (count == capacity) {
                size_t grown_capacity = capacity ? capacity * 2 : 16;
                char **grown = realloc(names, grown_capacity * sizeof(char *));
                if (!grown) {
                    goto fail;
                }
                names = grown;
                capacity = grown_capacity;
            }
            names[count] = ursatz_gui_dup_string(name);
            if (!names[count]) {
                goto fail;
            }
            count++;
        }
#ifdef _WIN32
    } while (FindNextFileA(handle, &find_data));
    FindClose(handle);
#else
    }
    closedir(handle);
#endif
    *out_names = names;
    *out_count = count;
    return true;

fail:
#ifdef _WIN32
    FindClose(handle);
#else
    closedir(handle);
#endif
    for (size_t i = 0; i < count; i++) {
        free(names[i]);
    }
    free(names);
    return false;
}

static void free_names(char **names, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(names[i]);
    }
    free(names);
}

static bool is_valid_id(const char *id) {
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
    const char *start = text ? text : "";
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    size_t length = strlen(start);
    while (length > 0 && isspace((unsigned char)start[length - 1])) {
        length--;
    }
    size_t chars = 0;
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)start[i];
        if (c < 0x20 || c == 0x7f) {
            return NULL;
        }
        if ((c & 0xC0) != 0x80) {
            chars++;
        }
    }
    if (chars > max_chars) {
        return NULL;
    }
    char *copy = malloc(length + 1);
    if (!copy) {
        *alloc_failed = true;
        return NULL;
    }
    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

static UrsatzGuiMaterialResult normalize_name(const char *text, char **out) {
    bool alloc_failed;
    *out = trim_text(text, NAME_MAX_CHARS, &alloc_failed);
    if (!*out) {
        return alloc_failed ? URSATZ_GUI_MATERIAL_ERR_ALLOC : URSATZ_GUI_MATERIAL_ERR_INVALID_NAME;
    }
    if ((*out)[0] == '\0') {
        free(*out);
        *out = NULL;
        return URSATZ_GUI_MATERIAL_ERR_INVALID_NAME;
    }
    return URSATZ_GUI_MATERIAL_OK;
}

static UrsatzGuiMaterialResult normalize_description(const char *text, char **out) {
    bool alloc_failed;
    *out = trim_text(text, DESCRIPTION_MAX_CHARS, &alloc_failed);
    if (!*out) {
        return alloc_failed ? URSATZ_GUI_MATERIAL_ERR_ALLOC : URSATZ_GUI_MATERIAL_ERR_INVALID_NAME;
    }
    return URSATZ_GUI_MATERIAL_OK;
}

static int compare_ci(const char *a, const char *b) {
    for (; *a && *b; a++, b++) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) {
            return ca < cb ? -1 : 1;
        }
    }
    return (*a != '\0') - (*b != '\0');
}


/* --- paths --------------------------------------------------------------- */

static bool project_path(const char *id, char *out, size_t out_size) {
    return is_valid_id(id) && snprintf(out, out_size, "%s/projects/%s.json", g_dir, id) < (int)out_size;
}

/* --- projects ------------------------------------------------------------ */

/* Number of items whose item.json names project_id. */
static size_t count_items_locked(const char *project_id) {
    char dir[PATH_BYTES];
    char **names = NULL;
    size_t count = 0;
    size_t found = 0;
    if (snprintf(dir, sizeof(dir), "%s/items", g_dir) >= (int)sizeof(dir) || !list_names(dir, &names, &count)) {
        return 0;
    }
    for (size_t i = 0; i < count; i++) {
        char path[PATH_BYTES];
        if (!is_valid_id(names[i]) ||
            snprintf(path, sizeof(path), "%s/items/%s/item.json", g_dir, names[i]) >= (int)sizeof(path)) {
            continue;
        }
        cJSON *meta = ursatz_gui_read_json_object(path);
        if (meta && strcmp(ursatz_gui_json_string(meta, "project_id"), project_id) == 0) {
            found++;
        }
        cJSON_Delete(meta);
    }
    free_names(names, count);
    return found;
}

/* Loads projects/<id>.json; the file name is the id, a copy inside the file is not trusted. */
static UrsatzGuiMaterialResult load_project_locked(const char *id, cJSON **out) {
    char path[PATH_BYTES];
    if (!project_path(id, path, sizeof(path))) {
        return URSATZ_GUI_MATERIAL_ERR_INVALID_ID;
    }
    cJSON *project = ursatz_gui_read_json_object(path);
    if (!project) {
        return ursatz_gui_path_exists(path) ? URSATZ_GUI_MATERIAL_ERR_FAILED : URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(project, "id");
    cJSON *id_item = cJSON_CreateString(id);
    if (!id_item) {
        cJSON_Delete(project);
        return URSATZ_GUI_MATERIAL_ERR_ALLOC;
    }
    cJSON_AddItemToObject(project, "id", id_item);
    *out = project;
    return URSATZ_GUI_MATERIAL_OK;
}

static UrsatzGuiMaterialResult save_project_locked(const cJSON *project) {
    char path[PATH_BYTES];
    if (!project_path(ursatz_gui_json_string(project, "id"), path, sizeof(path))) {
        return URSATZ_GUI_MATERIAL_ERR_INVALID_ID;
    }
    return write_json(path, project) ? URSATZ_GUI_MATERIAL_OK : URSATZ_GUI_MATERIAL_ERR_FAILED;
}


static UrsatzGuiMaterialResult create_project_locked(const char *name, const char *description, cJSON **out_project) {
    char *clean_name = NULL;
    char *clean_description = NULL;
    UrsatzGuiMaterialResult result = normalize_name(name, &clean_name);
    if (result == URSATZ_GUI_MATERIAL_OK) {
        result = normalize_description(description, &clean_description);
    }
    if (result != URSATZ_GUI_MATERIAL_OK) {
        free(clean_name);
        return result;
    }

    char *id = NULL;
    char path[PATH_BYTES];
    for (int attempt = 0; attempt < ID_MAX_TRIES; attempt++) {
        free(id);
        id = ursatz_gui_new_id("prj");
        if (!id) {
            result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
            break;
        }
        if (project_path(id, path, sizeof(path)) && !ursatz_gui_path_exists(path)) {
            break;
        }
        free(id);
        id = NULL;
    }
    if (result == URSATZ_GUI_MATERIAL_OK && !id) {
        result = URSATZ_GUI_MATERIAL_ERR_FAILED;
    }

    cJSON *project = NULL;
    if (result == URSATZ_GUI_MATERIAL_OK) {
        char now[32];
        ursatz_gui_iso_utc_now(now, sizeof(now));
        project = cJSON_CreateObject();
        bool ok = project && cJSON_AddStringToObject(project, "id", id) &&
                  cJSON_AddStringToObject(project, "name", clean_name) &&
                  cJSON_AddStringToObject(project, "description", clean_description) &&
                  cJSON_AddStringToObject(project, "created_at", now) &&
                  cJSON_AddStringToObject(project, "updated_at", now);
        result = !ok ? URSATZ_GUI_MATERIAL_ERR_ALLOC : save_project_locked(project);
    }
    if (result == URSATZ_GUI_MATERIAL_OK && out_project) {
        *out_project = project;
    } else {
        cJSON_Delete(project);
    }
    free(id);
    free(clean_name);
    free(clean_description);
    return result;
}

bool ursatz_gui_material_service_init(const char *dir) {
    if (!dir || dir[0] == '\0') {
        return false;
    }
    LOCK();
    g_ready = false;
    char projects[PATH_BYTES];
    char items[PATH_BYTES];
    bool ok = snprintf(g_dir, sizeof(g_dir), "%s", dir) < (int)sizeof(g_dir) &&
              snprintf(projects, sizeof(projects), "%s/projects", dir) < (int)sizeof(projects) &&
              snprintf(items, sizeof(items), "%s/items", dir) < (int)sizeof(items) && ursatz_gui_ensure_dir(g_dir) &&
              ursatz_gui_ensure_dir(projects) && ursatz_gui_ensure_dir(items);

    char **names = NULL;
    size_t count = 0;
    if (ok) {
        ok = list_names(projects, &names, &count);
    }
    bool has_project = false;
    for (size_t i = 0; i < count; i++) {
        has_project = has_project || has_suffix(names[i], ".json");
    }
    free_names(names, count);

    if (ok) {
        g_ready = true;
        if (!has_project) {
            ok = create_project_locked("General", "", NULL) == URSATZ_GUI_MATERIAL_OK;
            g_ready = ok;
        }
    }
    UNLOCK();
    return ok;
}

void ursatz_gui_material_service_shutdown(void) {
    LOCK();
    g_ready = false;
    g_dir[0] = '\0';
    UNLOCK();
}

UrsatzGuiMaterialResult ursatz_gui_material_project_create(const char *name, const char *description,
                                                            cJSON **out_project) {
    if (out_project) {
        *out_project = NULL;
    }
    LOCK();
    UrsatzGuiMaterialResult result =
        g_ready ? create_project_locked(name, description, out_project) : URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    UNLOCK();
    return result;
}

static int compare_projects(const void *a, const void *b) {
    const cJSON *left = *(const cJSON *const *)a;
    const cJSON *right = *(const cJSON *const *)b;
    int by_name = compare_ci(ursatz_gui_json_string(left, "name"), ursatz_gui_json_string(right, "name"));
    return by_name != 0 ? by_name : strcmp(ursatz_gui_json_string(left, "id"), ursatz_gui_json_string(right, "id"));
}

UrsatzGuiMaterialResult ursatz_gui_material_project_list(cJSON **out_projects) {
    *out_projects = NULL;
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    }

    char dir[PATH_BYTES];
    char **names = NULL;
    size_t count = 0;
    if (snprintf(dir, sizeof(dir), "%s/projects", g_dir) >= (int)sizeof(dir) || !list_names(dir, &names, &count)) {
        UNLOCK();
        return URSATZ_GUI_MATERIAL_ERR_FAILED;
    }

    UrsatzGuiMaterialResult result = URSATZ_GUI_MATERIAL_OK;
    cJSON **projects = count ? calloc(count, sizeof(cJSON *)) : NULL;
    size_t loaded = 0;
    if (count && !projects) {
        result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
    }
    for (size_t i = 0; result == URSATZ_GUI_MATERIAL_OK && i < count; i++) {
        if (!has_suffix(names[i], ".json")) {
            continue;
        }
        char id[ID_MAX_BYTES + 1];
        size_t id_len = strlen(names[i]) - strlen(".json");
        if (id_len > ID_MAX_BYTES) {
            continue;
        }
        memcpy(id, names[i], id_len);
        id[id_len] = '\0';
        cJSON *project = NULL;
        if (load_project_locked(id, &project) != URSATZ_GUI_MATERIAL_OK) {
            continue;
        }
        if (!cJSON_AddNumberToObject(project, "item_count", (double)count_items_locked(id))) {
            cJSON_Delete(project);
            result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
            break;
        }
        projects[loaded++] = project;
    }
    free_names(names, count);
    UNLOCK();

    cJSON *array = result == URSATZ_GUI_MATERIAL_OK ? cJSON_CreateArray() : NULL;
    if (result == URSATZ_GUI_MATERIAL_OK && !array) {
        result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
    }
    if (loaded > 1) {
        qsort(projects, loaded, sizeof(cJSON *), compare_projects);
    }
    for (size_t i = 0; i < loaded; i++) {
        if (result == URSATZ_GUI_MATERIAL_OK) {
            cJSON_AddItemToArray(array, projects[i]);
        } else {
            cJSON_Delete(projects[i]);
        }
    }
    free(projects);
    if (result == URSATZ_GUI_MATERIAL_OK) {
        *out_projects = array;
    }
    return result;
}

UrsatzGuiMaterialResult ursatz_gui_material_project_get(const char *id, cJSON **out_project) {
    *out_project = NULL;
    LOCK();
    UrsatzGuiMaterialResult result = g_ready ? load_project_locked(id, out_project)
                                             : URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    if (result == URSATZ_GUI_MATERIAL_OK &&
        !cJSON_AddNumberToObject(*out_project, "item_count", (double)count_items_locked(id))) {
        cJSON_Delete(*out_project);
        *out_project = NULL;
        result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
    }
    UNLOCK();
    return result;
}

UrsatzGuiMaterialResult ursatz_gui_material_project_update(const char *id, const char *name, const char *description,
                                                            cJSON **out_project) {
    if (out_project) {
        *out_project = NULL;
    }
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    }
    cJSON *project = NULL;
    char *clean_name = NULL;
    char *clean_description = NULL;
    UrsatzGuiMaterialResult result = load_project_locked(id, &project);
    if (result == URSATZ_GUI_MATERIAL_OK && name) {
        result = normalize_name(name, &clean_name);
    }
    if (result == URSATZ_GUI_MATERIAL_OK && description) {
        result = normalize_description(description, &clean_description);
    }
    if (result == URSATZ_GUI_MATERIAL_OK) {
        char now[32];
        ursatz_gui_iso_utc_now(now, sizeof(now));
        bool ok = (!clean_name || ursatz_gui_json_set_string(project, "name", clean_name)) &&
                  (!clean_description || ursatz_gui_json_set_string(project, "description", clean_description)) &&
                  ursatz_gui_json_set_string(project, "updated_at", now);
        result = ok ? save_project_locked(project) : URSATZ_GUI_MATERIAL_ERR_ALLOC;
    }
    if (result == URSATZ_GUI_MATERIAL_OK && out_project) {
        *out_project = project;
    } else {
        cJSON_Delete(project);
    }
    UNLOCK();
    free(clean_name);
    free(clean_description);
    return result;
}

UrsatzGuiMaterialResult ursatz_gui_material_project_delete(const char *id) {
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    }
    char path[PATH_BYTES];
    UrsatzGuiMaterialResult result = URSATZ_GUI_MATERIAL_OK;
    if (!project_path(id, path, sizeof(path))) {
        result = URSATZ_GUI_MATERIAL_ERR_INVALID_ID;
    } else if (!ursatz_gui_path_exists(path)) {
        result = URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
    } else if (count_items_locked(id) > 0) {
        result = URSATZ_GUI_MATERIAL_ERR_NOT_EMPTY;
    } else if (remove(path) != 0) {
        result = URSATZ_GUI_MATERIAL_ERR_FAILED;
    }
    UNLOCK();
    return result;
}

/* --- items and versions -------------------------------------------------- */

static const char *prefix_for_type(const char *type) {
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

static bool item_dir_path(const char *id, char *out, size_t out_size) {
    return is_valid_id(id) && snprintf(out, out_size, "%s/items/%s", g_dir, id) < (int)out_size;
}

static bool item_file_path(const char *id, const char *file, char *out, size_t out_size) {
    return is_valid_id(id) && snprintf(out, out_size, "%s/items/%s/%s", g_dir, id, file) < (int)out_size;
}

static bool version_path(const char *id, int number, char *out, size_t out_size) {
    return is_valid_id(id) && number >= 1 && snprintf(out, out_size, "%s/items/%s/v%d.json", g_dir, id, number) < (int)out_size;
}

static bool project_exists_locked(const char *project_id) {
    char path[PATH_BYTES];
    return project_path(project_id, path, sizeof(path)) && ursatz_gui_path_exists(path);
}

/* --- body validation ----------------------------------------------------- */

#define LABEL_MAX_CHARS 8
#define NOTES_MAX_CHARS 4000
#define HARMONY_MAX_CHARS 200

static UrsatzGuiMaterialResult invalid_body(char *error, size_t error_size, const char *message) {
    if (error && error_size > 0) {
        snprintf(error, error_size, "%s", message);
    }
    return URSATZ_GUI_MATERIAL_ERR_INVALID_BODY;
}

static bool in_set(const cJSON *value, const char *const *set) {
    if (!cJSON_IsString(value)) {
        return false;
    }
    for (; *set; set++) {
        if (strcmp(value->valuestring, *set) == 0) {
            return true;
        }
    }
    return false;
}

static bool is_whole_number(const cJSON *value, double minimum) {
    return cJSON_IsNumber(value) && value->valuedouble >= minimum && value->valuedouble == (double)(long long)value->valuedouble;
}

static size_t utf8_length(const char *text) {
    size_t chars = 0;
    for (; *text; text++) {
        if (((unsigned char)*text & 0xC0) != 0x80) {
            chars++;
        }
    }
    return chars;
}

/* A member that is absent or null. */
static bool is_absent_or_null(const cJSON *object, const char *name) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
    return !value || cJSON_IsNull(value);
}

/* digits with an optional "/digits": "4/4" when meter is set (both parts 1 to 99), else "0", "1/2", "3". */
static bool is_rational(const char *text, bool meter) {
    const char *slash = strchr(text, '/');
    size_t left = slash ? (size_t)(slash - text) : strlen(text);
    size_t right = slash ? strlen(slash + 1) : 0;
    if (left == 0 || left > (meter ? 2 : 9) || (meter && !slash) || (slash && (right == 0 || right > (meter ? 2 : 9)))) {
        return false;
    }
    bool denominator_nonzero = !slash;
    for (const char *c = text; *c; c++) {
        if (*c == '/' && c == slash) {
            continue;
        }
        if (!isdigit((unsigned char)*c)) {
            return false;
        }
        if (slash && c > slash && *c != '0') {
            denominator_nonzero = true;
        }
    }
    if (meter && text[0] == '0') {
        return false;
    }
    return denominator_nonzero;
}

static UrsatzGuiMaterialResult validate_uses(const cJSON *body, char *error, size_t error_size) {
    const cJSON *uses = cJSON_GetObjectItemCaseSensitive(body, "uses");
    if (!cJSON_IsArray(uses)) {
        return invalid_body(error, error_size, "uses must be an array");
    }
    const cJSON *use = NULL;
    cJSON_ArrayForEach(use, uses) {
        const cJSON *item_id = cJSON_GetObjectItemCaseSensitive(use, "item_id");
        const cJSON *copy = cJSON_GetObjectItemCaseSensitive(use, "copy");
        if (!cJSON_IsObject(use) || !cJSON_IsString(item_id) || !is_valid_id(item_id->valuestring) ||
            !is_whole_number(cJSON_GetObjectItemCaseSensitive(use, "version"), 1) || (copy && !cJSON_IsObject(copy))) {
            return invalid_body(error, error_size, "each use needs item_id, version and, if present, an object copy");
        }
    }
    return URSATZ_GUI_MATERIAL_OK;
}

static bool is_string_list_from(const cJSON *value, const char *const *set) {
    if (!cJSON_IsArray(value)) {
        return false;
    }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, value) {
        if (!in_set(entry, set)) {
            return false;
        }
    }
    return true;
}

static UrsatzGuiMaterialResult validate_derived(const cJSON *derived, char *error, size_t error_size) {
    static const char *const kinds[] = {"exact", "motive-form", "variant", NULL};
    static const char *const dimensions[] = {"rhythm", "intervals", "harmony", "contour", NULL};
    static const char *const functions[] = {"beginning", "continuation", "contrast", "cadential", NULL};
    static const char *const flags[] = {"approved", "rejected", "unflagged", NULL};
    const cJSON *standalone = cJSON_GetObjectItemCaseSensitive(derived, "standalone");
    const cJSON *source_item = cJSON_GetObjectItemCaseSensitive(derived, "source_item");
    const cJSON *seed = cJSON_GetObjectItemCaseSensitive(derived, "seed");
    if (!cJSON_IsObject(derived)) {
        return invalid_body(error, error_size, "derived must be null or an object");
    }
    if (!cJSON_IsString(source_item) || !is_valid_id(source_item->valuestring) ||
        !is_whole_number(cJSON_GetObjectItemCaseSensitive(derived, "source_version"), 1)) {
        return invalid_body(error, error_size, "derived needs source_item and source_version");
    }
    if (!cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(derived, "operations"))) {
        return invalid_body(error, error_size, "derived operations must be an array");
    }
    if (seed && !cJSON_IsNull(seed) && !is_whole_number(seed, 0)) {
        return invalid_body(error, error_size, "derived seed must be null or a whole number");
    }
    if (!in_set(cJSON_GetObjectItemCaseSensitive(derived, "kind"), kinds)) {
        return invalid_body(error, error_size, "derived kind must be exact, motive-form or variant");
    }
    if (!is_string_list_from(cJSON_GetObjectItemCaseSensitive(derived, "kept"), dimensions) ||
        !is_string_list_from(cJSON_GetObjectItemCaseSensitive(derived, "changed"), dimensions)) {
        return invalid_body(error, error_size, "derived kept and changed must list rhythm, intervals, harmony or contour");
    }
    if (!is_string_list_from(cJSON_GetObjectItemCaseSensitive(derived, "functions"), functions)) {
        return invalid_body(error, error_size, "derived functions must list beginning, continuation, contrast or cadential");
    }
    const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(derived, "harmony");
    if (harmony && !cJSON_IsNull(harmony) &&
        (!cJSON_IsString(harmony) || utf8_length(harmony->valuestring) > HARMONY_MAX_CHARS)) {
        return invalid_body(error, error_size, "derived harmony must be null or a string of at most 200 characters");
    }
    const cJSON *remoteness = cJSON_GetObjectItemCaseSensitive(derived, "remoteness");
    if (remoteness && !cJSON_IsNull(remoteness) &&
        (!cJSON_IsNumber(remoteness) || remoteness->valuedouble < 0 || remoteness->valuedouble > 1)) {
        return invalid_body(error, error_size, "derived remoteness must be null or a number from 0 to 1");
    }
    if (!in_set(cJSON_GetObjectItemCaseSensitive(derived, "flag"), flags)) {
        return invalid_body(error, error_size, "derived flag must be approved, rejected or unflagged");
    }
    if (standalone && !cJSON_IsBool(standalone)) {
        return invalid_body(error, error_size, "derived standalone must be true or false");
    }
    return URSATZ_GUI_MATERIAL_OK;
}

#define TONE_OVERRIDES_MAX 64
#define TONE_ROLE_MAX 24

/* motif.tone_overrides is absent, null or an object of "<note>": "<role>" (1-based note); the role is the Composer's
 * word (R-276), so a save keeps its type and size and validate marks a role the Composer does not read. */
static UrsatzGuiMaterialResult validate_tone_overrides(const cJSON *overrides, char *error, size_t error_size) {
    if (!overrides || cJSON_IsNull(overrides)) {
        return URSATZ_GUI_MATERIAL_OK;
    }
    if (!cJSON_IsObject(overrides) || cJSON_GetArraySize(overrides) > TONE_OVERRIDES_MAX) {
        return invalid_body(error, error_size, "motif tone_overrides must be an object of at most 64 notes");
    }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, overrides) {
        const char *name = entry->string ? entry->string : "";
        char *end = NULL;
        long note = strtol(name, &end, 10);
        if (name[0] == '\0' || *end != '\0' || name[0] == '+' || name[0] == '-' || note < 1 || note > TONE_OVERRIDES_MAX ||
            !cJSON_IsString(entry) || strlen(entry->valuestring) > TONE_ROLE_MAX) {
            return invalid_body(error, error_size,
                                "motif tone_overrides must map a note number from 1 to 64 to a tone type");
        }
    }
    return URSATZ_GUI_MATERIAL_OK;
}

static UrsatzGuiMaterialResult validate_motif(const cJSON *motif, char *error, size_t error_size) {
    static const char *const roles[] = {"melodic", "accompaniment", NULL};
    static const char *const forms[] = {"full", "rhythm", "contour", NULL};
    if (!cJSON_IsObject(motif)) {
        return invalid_body(error, error_size, "a motif body needs a motif object");
    }
    if (!in_set(cJSON_GetObjectItemCaseSensitive(motif, "role"), roles)) {
        return invalid_body(error, error_size, "motif role must be melodic or accompaniment");
    }
    if (!in_set(cJSON_GetObjectItemCaseSensitive(motif, "form"), forms)) {
        return invalid_body(error, error_size, "motif form must be full, rhythm or contour");
    }
    const cJSON *notes = cJSON_GetObjectItemCaseSensitive(motif, "notes");
    if (!cJSON_IsString(notes) || notes->valuestring[0] == '\0' || utf8_length(notes->valuestring) > NOTES_MAX_CHARS) {
        return invalid_body(error, error_size, "motif notes must be a non-empty string of at most 4000 characters");
    }
    const cJSON *meter = cJSON_GetObjectItemCaseSensitive(motif, "meter");
    if (!cJSON_IsString(meter) || !is_rational(meter->valuestring, true)) {
        return invalid_body(error, error_size, "motif meter must look like 4/4");
    }
    const cJSON *key = cJSON_GetObjectItemCaseSensitive(motif, "key");
    if (key && !cJSON_IsNull(key) && !cJSON_IsString(key)) {
        return invalid_body(error, error_size, "motif key must be null or a string");
    }
    const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(motif, "pickup");
    if (!cJSON_IsString(pickup) || !is_rational(pickup->valuestring, false)) {
        return invalid_body(error, error_size, "motif pickup must be beats as n or n/d");
    }
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(motif, "features");
    if (!cJSON_IsArray(features)) {
        return invalid_body(error, error_size, "motif features must be an array");
    }
    const cJSON *feature = NULL;
    cJSON_ArrayForEach(feature, features) {
        const cJSON *label = cJSON_GetObjectItemCaseSensitive(feature, "label");
        const cJSON *first = cJSON_GetObjectItemCaseSensitive(feature, "first");
        const cJSON *last = cJSON_GetObjectItemCaseSensitive(feature, "last");
        if (!cJSON_IsObject(feature) || !cJSON_IsString(label) || utf8_length(label->valuestring) < 1 ||
            utf8_length(label->valuestring) > LABEL_MAX_CHARS || !is_whole_number(first, 0) ||
            !is_whole_number(last, 0) || first->valuedouble > last->valuedouble) {
            return invalid_body(error, error_size, "each feature needs a label of 1 to 8 characters and 0 <= first <= last");
        }
    }
    const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(motif, "harmony");
    if (harmony && !cJSON_IsNull(harmony) &&
        (!cJSON_IsString(harmony) || utf8_length(harmony->valuestring) > HARMONY_MAX_CHARS)) {
        return invalid_body(error, error_size, "motif harmony must be null or a string of at most 200 characters");
    }
    UrsatzGuiMaterialResult tones = validate_tone_overrides(cJSON_GetObjectItemCaseSensitive(motif, "tone_overrides"), error, error_size);
    if (tones != URSATZ_GUI_MATERIAL_OK) {
        return tones;
    }
    const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(motif, "analysis");
    if (analysis && !cJSON_IsNull(analysis) && !cJSON_IsObject(analysis)) {
        return invalid_body(error, error_size, "motif analysis must be null or an object");
    }
    const cJSON *degrees = cJSON_GetObjectItemCaseSensitive(motif, "degrees");
    if (degrees && !cJSON_IsNull(degrees) &&
        (!cJSON_IsString(degrees) || utf8_length(degrees->valuestring) > NOTES_MAX_CHARS)) {
        return invalid_body(error, error_size, "motif degrees must be null or a string of at most 4000 characters");
    }
    return is_absent_or_null(motif, "derived")
               ? URSATZ_GUI_MATERIAL_OK
               : validate_derived(cJSON_GetObjectItemCaseSensitive(motif, "derived"), error, error_size);
}

static const char *const kThemePools[] = {"approved-and-unflagged", "approved", "none", NULL};
static const char *const kThemeMotiveCategories[] = {"motive-degrees", "motive-intervals", "motive-rhythm",
                                                     "motive-nct-figure", NULL};
/* The members a placeholder theme (form null or a string, slots of anything) does not have. */
static const char *const kThemeNewMembers[] = {"ideas", "chords", "accompaniment", "brief", "style_weight",
                                               "without", "tempo", "check",         "seed",  "result", NULL};

#define THEME_BRIEF_MAX_SCOPES 16
#define THEME_BRIEF_MAX_PIECES 5000
#define THEME_BRIEF_MAX_STYLES 16
#define THEME_WITHOUT_MAX 4
#define THEME_IDEAS_MAX 10
#define THEME_CHORD_BARS 8
#define THEME_CHORD_TEXT_MAX 64

static bool is_string_or_null(const cJSON *object, const char *name) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
    return !value || cJSON_IsNull(value) || cJSON_IsString(value);
}

/* A member that is absent, null or one of set. */
static bool is_in_set_or_null(const cJSON *object, const char *name, const char *const *set) {
    return is_absent_or_null(object, name) || in_set(cJSON_GetObjectItemCaseSensitive(object, name), set);
}

#define THEME_FORM_WORD_MAX 40

/* The form words are the Composer's (R-276): a save keeps their type and size, and validate marks a value the Composer
 * does not read, as compose refuses it with its path. */
static UrsatzGuiMaterialResult validate_theme_form(const cJSON *form, char *error, size_t error_size) {
    static const char *const words[] = {"into", "hybrid", "mode", "meter", "cadence", "first_cadence", NULL};
    for (const char *const *word = words; *word; word++) {
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(form, *word);
        if (!is_string_or_null(form, *word) || (cJSON_IsString(value) && strlen(value->valuestring) > THEME_FORM_WORD_MAX)) {
            return invalid_body(error, error_size, "theme form into, hybrid, mode, meter, cadence and first_cadence must be text");
        }
    }
    if (cJSON_GetObjectItemCaseSensitive(form, "key")) {
        return invalid_body(error, error_size, "themes are in C; remove key");
    }
    const cJSON *bars = cJSON_GetObjectItemCaseSensitive(form, "bars");
    if (!is_absent_or_null(form, "bars") && !cJSON_IsNumber(bars)) {
        return invalid_body(error, error_size, "theme form bars must be a number");
    }
    return URSATZ_GUI_MATERIAL_OK;
}

/* {slot, item_id, version}: a reference to a version of another item. */
static bool theme_reference_ok(const cJSON *reference) {
    const cJSON *item_id = cJSON_GetObjectItemCaseSensitive(reference, "item_id");
    return cJSON_IsObject(reference) && cJSON_IsString(item_id) && is_valid_id(item_id->valuestring) &&
           is_whole_number(cJSON_GetObjectItemCaseSensitive(reference, "version"), 1);
}

static UrsatzGuiMaterialResult validate_theme_slots(const cJSON *slots, char *error, size_t error_size) {
    bool seen[THEME_IDEAS_MAX] = {false};
    const cJSON *slot = NULL;
    cJSON_ArrayForEach(slot, slots) {
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(slot, "slot");
        int index = cJSON_IsString(name) ? ursatz_gui_composer_vocabulary_index("theme_slots", name->valuestring) : -1;
        if (index < 0 || index >= THEME_IDEAS_MAX || !theme_reference_ok(slot)) {
            return invalid_body(error, error_size, "each theme slot needs slot, item_id and version");
        }
        if (seen[index]) {
            return invalid_body(error, error_size, "each theme slot may be named once");
        }
        seen[index] = true;
    }
    return seen[0] ? URSATZ_GUI_MATERIAL_OK : invalid_body(error, error_size, "a theme needs a basic slot");
}

/* [{idea, item_id, version}]: a motif version for a single idea, each idea named once. */
static UrsatzGuiMaterialResult validate_theme_ideas(const cJSON *ideas, char *error, size_t error_size) {
    bool seen[THEME_IDEAS_MAX] = {false};
    if (!cJSON_IsArray(ideas) || cJSON_GetArraySize(ideas) > THEME_IDEAS_MAX) {
        return invalid_body(error, error_size, "theme ideas must be a list of at most 10");
    }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, ideas) {
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(entry, "idea");
        int index = cJSON_IsString(name) ? ursatz_gui_composer_vocabulary_index("theme_ideas", name->valuestring) : -1;
        if (index < 0 || index >= THEME_IDEAS_MAX || !theme_reference_ok(entry)) {
            return invalid_body(error, error_size, "each theme idea needs idea, item_id and version");
        }
        if (seen[index]) {
            return invalid_body(error, error_size, "each theme idea may be named once");
        }
        seen[index] = true;
    }
    return URSATZ_GUI_MATERIAL_OK;
}

/* [{bar, text}]: the chords given for a bar, bar 0 to 7, each bar once; the Composer checks the syntax. */
static UrsatzGuiMaterialResult validate_theme_chords(const cJSON *chords, char *error, size_t error_size) {
    bool seen[THEME_CHORD_BARS] = {false};
    if (!cJSON_IsArray(chords) || cJSON_GetArraySize(chords) > THEME_CHORD_BARS) {
        return invalid_body(error, error_size, "theme chords must be a list of at most 8 bars");
    }
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, chords) {
        const cJSON *bar = cJSON_GetObjectItemCaseSensitive(entry, "bar");
        const cJSON *text = cJSON_GetObjectItemCaseSensitive(entry, "text");
        if (!cJSON_IsObject(entry) || !is_whole_number(bar, 0) || bar->valuedouble >= THEME_CHORD_BARS ||
            !cJSON_IsString(text) || utf8_length(text->valuestring) > THEME_CHORD_TEXT_MAX) {
            return invalid_body(error, error_size, "each theme chord needs a bar from 0 to 7 and a text of at most 64 characters");
        }
        if (seen[(int)bar->valuedouble]) {
            return invalid_body(error, error_size, "each theme chord bar may be named once");
        }
        seen[(int)bar->valuedouble] = true;
    }
    return URSATZ_GUI_MATERIAL_OK;
}

static UrsatzGuiMaterialResult validate_theme_accompaniment(const cJSON *accompaniment, char *error,
                                                            size_t error_size) {
    static const char *const patterns[] = {"block", "arpeggiated", "alberti", "oom-pah", "any", NULL};
    const cJSON *pattern = cJSON_GetObjectItemCaseSensitive(accompaniment, "pattern");
    if (cJSON_IsObject(accompaniment) && cJSON_IsString(pattern) && strcmp(pattern->valuestring, "motive") == 0) {
        return theme_reference_ok(accompaniment)
                   ? URSATZ_GUI_MATERIAL_OK
                   : invalid_body(error, error_size, "a motive accompaniment needs item_id and version");
    }
    return cJSON_IsObject(accompaniment) && in_set(pattern, patterns)
               ? URSATZ_GUI_MATERIAL_OK
               : invalid_body(error, error_size, "theme accompaniment pattern must be block, arpeggiated, alberti, oom-pah, any or motive");
}

static bool theme_id_list_ok(const cJSON *brief, const char *name, size_t max) {
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(brief, name);
    if (!list) {
        return true;
    }
    if (!cJSON_IsArray(list) || (size_t)cJSON_GetArraySize(list) > max) {
        return false;
    }
    const cJSON *id = NULL;
    cJSON_ArrayForEach(id, list) {
        if (!cJSON_IsString(id) || !is_valid_id(id->valuestring)) {
            return false;
        }
    }
    return true;
}

/* {scopes, piece_ids, style_ids} as the styles route takes a brief: at most 16 scopes, 5000 pieces and 16 styles. */
static bool theme_brief_ok(const cJSON *brief) {
    static const char *const kinds[] = {"all", "collection", "filter", "subject", "project", NULL};
    const cJSON *scopes = cJSON_GetObjectItemCaseSensitive(brief, "scopes");
    if (!cJSON_IsObject(brief) || (scopes && (!cJSON_IsArray(scopes) || cJSON_GetArraySize(scopes) > THEME_BRIEF_MAX_SCOPES)) ||
        !theme_id_list_ok(brief, "piece_ids", THEME_BRIEF_MAX_PIECES) ||
        !theme_id_list_ok(brief, "style_ids", THEME_BRIEF_MAX_STYLES)) {
        return false;
    }
    const cJSON *scope = NULL;
    cJSON_ArrayForEach(scope, scopes) {
        const cJSON *filter = cJSON_GetObjectItemCaseSensitive(scope, "filter");
        if (!cJSON_IsObject(scope) || !in_set(cJSON_GetObjectItemCaseSensitive(scope, "kind"), kinds) ||
            !is_string_or_null(scope, "collection_id") || !is_string_or_null(scope, "subject_id") ||
            !is_string_or_null(scope, "project_id") ||
            (filter && !cJSON_IsNull(filter) &&
             (!cJSON_IsObject(filter) || !is_string_or_null(filter, "composer") || !is_string_or_null(filter, "source") ||
              !is_string_or_null(filter, "tag")))) {
            return false;
        }
    }
    int entries = (scopes ? cJSON_GetArraySize(scopes) : 0) +
                  cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(brief, "piece_ids")) +
                  cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(brief, "style_ids"));
    return entries > 0;
}

/* True when the theme is a placeholder: form null, absent or a string, no new member, and no slot written as an object with a slot name. */
static bool theme_is_placeholder(const cJSON *theme) {
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(theme, "form");
    if (cJSON_IsObject(form)) {
        return false;
    }
    for (const char *const *name = kThemeNewMembers; *name; name++) {
        if (cJSON_GetObjectItemCaseSensitive(theme, *name)) {
            return false;
        }
    }
    const cJSON *slot = NULL;
    cJSON_ArrayForEach(slot, cJSON_GetObjectItemCaseSensitive(theme, "slots")) {
        if (cJSON_IsObject(slot) && cJSON_GetObjectItemCaseSensitive(slot, "slot")) {
            return false;
        }
    }
    return true;
}

static UrsatzGuiMaterialResult validate_theme(const cJSON *theme, char *error, size_t error_size) {
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(theme, "form");
    const cJSON *slots = cJSON_GetObjectItemCaseSensitive(theme, "slots");
    if (!cJSON_IsObject(theme) || (form && !cJSON_IsNull(form) && !cJSON_IsString(form) && !cJSON_IsObject(form)) ||
        !cJSON_IsArray(slots)) {
        return invalid_body(error, error_size, "a theme body needs a theme object with form and slots");
    }
    if (cJSON_GetObjectItemCaseSensitive(theme, "pool")) {
        return invalid_body(error, error_size, "themes have no pool");
    }
    if (theme_is_placeholder(theme)) {
        return URSATZ_GUI_MATERIAL_OK;
    }
    UrsatzGuiMaterialResult result = cJSON_IsObject(form) ? validate_theme_form(form, error, error_size)
                                                          : URSATZ_GUI_MATERIAL_OK;
    if (result == URSATZ_GUI_MATERIAL_OK) {
        result = validate_theme_slots(slots, error, error_size);
    }
    if (result != URSATZ_GUI_MATERIAL_OK) {
        return result;
    }
    const cJSON *ideas = cJSON_GetObjectItemCaseSensitive(theme, "ideas");
    if (ideas && validate_theme_ideas(ideas, error, error_size) != URSATZ_GUI_MATERIAL_OK) {
        return URSATZ_GUI_MATERIAL_ERR_INVALID_BODY;
    }
    const cJSON *chords = cJSON_GetObjectItemCaseSensitive(theme, "chords");
    if (chords && validate_theme_chords(chords, error, error_size) != URSATZ_GUI_MATERIAL_OK) {
        return URSATZ_GUI_MATERIAL_ERR_INVALID_BODY;
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(theme, "accompaniment");
    if (!is_absent_or_null(theme, "accompaniment") &&
        validate_theme_accompaniment(accompaniment, error, error_size) != URSATZ_GUI_MATERIAL_OK) {
        return URSATZ_GUI_MATERIAL_ERR_INVALID_BODY;
    }
    if (!is_absent_or_null(theme, "brief") && !theme_brief_ok(cJSON_GetObjectItemCaseSensitive(theme, "brief"))) {
        return invalid_body(error, error_size, "theme brief needs scopes, piece_ids or style_ids within the limits");
    }
    const cJSON *weight = cJSON_GetObjectItemCaseSensitive(theme, "style_weight");
    if (!is_absent_or_null(theme, "style_weight") &&
        (!cJSON_IsString(weight) || !is_rational(weight->valuestring, false))) {
        return invalid_body(error, error_size, "theme style_weight must be n or n/d");
    }
    const cJSON *without = cJSON_GetObjectItemCaseSensitive(theme, "without");
    if (without && (!is_string_list_from(without, kThemeMotiveCategories) ||
                    cJSON_GetArraySize(without) > THEME_WITHOUT_MAX)) {
        return invalid_body(error, error_size, "theme without lists up to four motive categories");
    }
    const cJSON *tempo = cJSON_GetObjectItemCaseSensitive(theme, "tempo");
    if (!is_absent_or_null(theme, "tempo") && !is_whole_number(tempo, 0)) {
        return invalid_body(error, error_size, "theme tempo must be null or a whole number");
    }
    const cJSON *check = cJSON_GetObjectItemCaseSensitive(theme, "check");
    if (check && !cJSON_IsBool(check)) {
        return invalid_body(error, error_size, "theme check must be true or false");
    }
    const cJSON *seed = cJSON_GetObjectItemCaseSensitive(theme, "seed");
    if (!is_absent_or_null(theme, "seed") && !is_whole_number(seed, 0)) {
        return invalid_body(error, error_size, "theme seed must be null or a whole number");
    }
    const cJSON *result_member = cJSON_GetObjectItemCaseSensitive(theme, "result");
    if (result_member && !cJSON_IsNull(result_member) && !cJSON_IsObject(result_member)) {
        return invalid_body(error, error_size, "theme result must be null or an object");
    }
    return URSATZ_GUI_MATERIAL_OK;
}

/* --- piece bodies -------------------------------------------------------- */

/* The members a placeholder piece (structure null, a string, an array or an object without sections) does not have. */
static const char *const kPieceNewMembers[] = {"motive", "pool",  "accompaniment", "brief", "style_weight", "without",
                                               "tempo",  "check", "seed",          "result", NULL};

#define PIECE_SECTIONS_MAX 32
#define PIECE_ROLES_MAX 8
#define PIECE_ID_MAX 15
#define PIECE_LETTER_MAX 8
#define PIECE_TEXT_MAX 1024
#define PIECE_WORD_MAX 40
#define PIECE_CELLS_MAX 8

/* True when the piece is a placeholder: no new member and a structure that is not an object with a list of sections. */
static bool piece_is_placeholder(const cJSON *piece) {
    const cJSON *structure = cJSON_GetObjectItemCaseSensitive(piece, "structure");
    if (cJSON_IsObject(structure) && (cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(structure, "sections")) ||
                                      cJSON_IsString(cJSON_GetObjectItemCaseSensitive(structure, "form")))) {
        return false;
    }
    for (const char *const *name = kPieceNewMembers; *name; name++) {
        if (cJSON_GetObjectItemCaseSensitive(piece, *name)) {
            return false;
        }
    }
    return true;
}

/* An id like s0, s12 or end. */
static bool piece_section_id_ok(const char *id) {
    if (strcmp(id, "end") == 0) {
        return true;
    }
    if (id[0] != 's' || id[1] == '\0' || strlen(id) > PIECE_ID_MAX) {
        return false;
    }
    for (const char *c = id + 1; *c; c++) {
        if (!isdigit((unsigned char)*c)) {
            return false;
        }
    }
    return true;
}

/* {kind, ...}: null or empty, a motif, a theme version or a progression. */
static UrsatzGuiMaterialResult validate_piece_fill(const cJSON *fill, char *error, size_t error_size) {
    static const char *const kinds[] = {"empty", "motif", "theme", "progression", NULL};
    if (!fill || cJSON_IsNull(fill)) {
        return URSATZ_GUI_MATERIAL_OK;
    }
    const cJSON *kind = cJSON_GetObjectItemCaseSensitive(fill, "kind");
    if (!cJSON_IsObject(fill) || !in_set(kind, kinds)) {
        return invalid_body(error, error_size, "a section fill must be null or have a kind of empty, motif, theme or progression");
    }
    if (strcmp(kind->valuestring, "motif") == 0 || strcmp(kind->valuestring, "theme") == 0) {
        if (!theme_reference_ok(fill)) {
            return invalid_body(error, error_size, "a motif or theme fill needs item_id and version");
        }
    }
    if (strcmp(kind->valuestring, "motif") == 0) {
        const cJSON *into = cJSON_GetObjectItemCaseSensitive(fill, "into");
        const cJSON *hybrid = cJSON_GetObjectItemCaseSensitive(fill, "hybrid");
        if (!is_string_or_null(fill, "into") || !is_string_or_null(fill, "hybrid") ||
            (cJSON_IsString(into) && strlen(into->valuestring) > PIECE_WORD_MAX) ||
            (cJSON_IsString(hybrid) && strlen(hybrid->valuestring) > PIECE_WORD_MAX)) {
            return invalid_body(error, error_size, "a motif fill into and hybrid must be text");
        }
    }
    if (strcmp(kind->valuestring, "progression") == 0) {
        const cJSON *numerals = cJSON_GetObjectItemCaseSensitive(fill, "numerals");
        const cJSON *cells = cJSON_GetObjectItemCaseSensitive(fill, "cells");
        if (!cJSON_IsString(numerals) || numerals->valuestring[0] == '\0' || strlen(numerals->valuestring) > PIECE_TEXT_MAX) {
            return invalid_body(error, error_size, "a progression fill needs numerals as text");
        }
        if (!is_absent_or_null(fill, "cells") && (!is_whole_number(cells, 1) || cells->valuedouble > PIECE_CELLS_MAX)) {
            return invalid_body(error, error_size, "a progression fill cells must be 1 to 8");
        }
    }
    return URSATZ_GUI_MATERIAL_OK;
}

/* The words of a form's list (roles or sizes members) joined by ", ": member (value or label) of each entry. */
static void join_members(const cJSON *list, const char *member, char *out, size_t out_size) {
    size_t used = 0;
    out[0] = '\0';
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, list) {
        const char *text = ursatz_gui_json_string(entry, member);
        if (text && used + strlen(text) + 3 < out_size) {
            used += (size_t)snprintf(out + used, out_size - used, "%s%s", used ? ", " : "", text);
        }
    }
}

/* The key, mode and meter of a structure: the Composer's words (R-276), a save keeps their type and size. */
static UrsatzGuiMaterialResult validate_piece_words(const cJSON *structure, char *error, size_t error_size) {
    static const char *const words[] = {"key", "mode", "meter", NULL};
    for (const char *const *word = words; *word; word++) {
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(structure, *word);
        if (!is_string_or_null(structure, *word) || (cJSON_IsString(value) && strlen(value->valuestring) > PIECE_WORD_MAX)) {
            return invalid_body(error, error_size, "piece key, mode and meter must be text");
        }
    }
    return URSATZ_GUI_MATERIAL_OK;
}

/* A form whose sections are the request's own (the game loop): each part is one of the form's roles, in their order,
 * with every required role present. */
static UrsatzGuiMaterialResult validate_piece_sections(const cJSON *structure, const cJSON *form, char *error,
                                                       size_t error_size) {
    const cJSON *sections = cJSON_GetObjectItemCaseSensitive(structure, "sections");
    const cJSON *roles = cJSON_GetObjectItemCaseSensitive(form, "roles");
    int role_count = cJSON_GetArraySize(roles);
    if (!cJSON_IsArray(sections)) {
        return invalid_body(error, error_size, "a piece structure needs key, mode, meter and sections");
    }
    if (role_count < 1 || role_count > PIECE_ROLES_MAX) {
        return invalid_body(error, error_size, "the Composer gave no roles for this form");
    }
    char values[160];
    char labels[160];
    join_members(roles, "value", values, sizeof(values));
    join_members(roles, "label", labels, sizeof(labels));
    int count = cJSON_GetArraySize(sections);
    if (count < 1 || count > PIECE_SECTIONS_MAX) {
        return invalid_body(error, error_size, "a piece has 1 to 32 sections");
    }
    double beats_max = ursatz_gui_composer_limit("piece_beats");
    int last_part = -1;
    bool present[PIECE_ROLES_MAX] = {false};
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, sections) {
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(section, "id");
        const cJSON *part = cJSON_GetObjectItemCaseSensitive(section, "part");
        const cJSON *letter = cJSON_GetObjectItemCaseSensitive(section, "letter");
        const cJSON *beats = cJSON_GetObjectItemCaseSensitive(section, "beats");
        int part_index = -1;
        int index = 0;
        const cJSON *role = NULL;
        cJSON_ArrayForEach(role, roles) {
            const char *value = ursatz_gui_json_string(role, "value");
            if (cJSON_IsString(part) && value && strcmp(value, part->valuestring) == 0) {
                part_index = index;
            }
            index++;
        }
        if (!cJSON_IsObject(section) || !cJSON_IsString(id) || !piece_section_id_ok(id->valuestring) || part_index < 0) {
            char message[320];
            snprintf(message, sizeof(message), "each section needs an id like s0 or end and a part of %s", values);
            return invalid_body(error, error_size, message);
        }
        for (const cJSON *earlier = sections->child; earlier != section; earlier = earlier->next) {
            if (strcmp(cJSON_GetObjectItemCaseSensitive(earlier, "id")->valuestring, id->valuestring) == 0) {
                return invalid_body(error, error_size, "each section id may be used once");
            }
        }
        if (part_index < last_part) {
            char message[320];
            snprintf(message, sizeof(message), "sections must come in the order %s", labels);
            return invalid_body(error, error_size, message);
        }
        last_part = part_index;
        present[part_index] = true;
        if (!is_absent_or_null(section, "letter") &&
            (!cJSON_IsString(letter) || letter->valuestring[0] == '\0' || utf8_length(letter->valuestring) > PIECE_LETTER_MAX)) {
            return invalid_body(error, error_size, "a section letter must be 1 to 8 characters");
        }
        if (!is_whole_number(beats, 1) || (beats_max >= 1 && beats->valuedouble > beats_max)) {
            char message[96];
            snprintf(message, sizeof(message), "a section has 1 to %.0f beats", beats_max);
            return invalid_body(error, error_size, message);
        }
        const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(section, "pickup");
        if (!is_absent_or_null(section, "pickup") && (!cJSON_IsString(pickup) || !is_rational(pickup->valuestring, false))) {
            return invalid_body(error, error_size, "a section pickup must be beats as n or n/d");
        }
        const cJSON *cadence = cJSON_GetObjectItemCaseSensitive(section, "cadence");
        if (!is_string_or_null(section, "cadence") || (cJSON_IsString(cadence) && strlen(cadence->valuestring) > PIECE_WORD_MAX)) {
            return invalid_body(error, error_size, "a section cadence must be null or text");
        }
        UrsatzGuiMaterialResult filled = validate_piece_fill(cJSON_GetObjectItemCaseSensitive(section, "fill"), error, error_size);
        if (filled != URSATZ_GUI_MATERIAL_OK) {
            return filled;
        }
    }
    int index = 0;
    const cJSON *role = NULL;
    cJSON_ArrayForEach(role, roles) {
        const cJSON *required = cJSON_GetObjectItemCaseSensitive(role, "required");
        if (cJSON_IsTrue(required) && !present[index]) {
            char message[160];
            snprintf(message, sizeof(message), "a piece needs at least one %s section", ursatz_gui_json_string(role, "label"));
            return invalid_body(error, error_size, message);
        }
        index++;
    }
    return URSATZ_GUI_MATERIAL_OK;
}

/* A form with sizes: no sections, bars and count (absent is 0) that one of the form's sizes has, 0 and 0 being the
 * default, and modulate only where the form allows it. */
static UrsatzGuiMaterialResult validate_piece_size(const cJSON *structure, const cJSON *form, char *error,
                                                   size_t error_size) {
    const char *label = ursatz_gui_json_string(form, "label");
    char message[256];
    if (!is_absent_or_null(structure, "sections")) {
        snprintf(message, sizeof(message), "a %s piece has no sections of its own", label ? label : "sized");
        return invalid_body(error, error_size, message);
    }
    const cJSON *bars = cJSON_GetObjectItemCaseSensitive(structure, "bars");
    const cJSON *count = cJSON_GetObjectItemCaseSensitive(structure, "count");
    if ((!is_absent_or_null(structure, "bars") && !is_whole_number(bars, 0)) ||
        (!is_absent_or_null(structure, "count") && !is_whole_number(count, 0))) {
        return invalid_body(error, error_size, "piece bars and count must be whole numbers");
    }
    double want_bars = cJSON_IsNumber(bars) ? bars->valuedouble : 0;
    double want_count = cJSON_IsNumber(count) ? count->valuedouble : 0;
    if (want_bars != 0 || want_count != 0) {
        bool found = false;
        const cJSON *size = NULL;
        cJSON_ArrayForEach(size, cJSON_GetObjectItemCaseSensitive(form, "sizes")) {
            const cJSON *size_bars = cJSON_GetObjectItemCaseSensitive(size, "bars");
            const cJSON *size_count = cJSON_GetObjectItemCaseSensitive(size, "count");
            found = found || (cJSON_IsNumber(size_bars) && cJSON_IsNumber(size_count) &&
                              size_bars->valuedouble == want_bars && size_count->valuedouble == want_count);
        }
        if (!found) {
            snprintf(message, sizeof(message), "a %s piece has no size of %.0f bars and %.0f count", label ? label : "sized",
                     want_bars, want_count);
            return invalid_body(error, error_size, message);
        }
    }
    const cJSON *modulate = cJSON_GetObjectItemCaseSensitive(structure, "modulate");
    if (!is_absent_or_null(structure, "modulate")) {
        const cJSON *allowed = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(form, "settings"), "modulate");
        if (!cJSON_IsBool(modulate)) {
            return invalid_body(error, error_size, "piece modulate must be true or false");
        }
        if (cJSON_IsTrue(modulate) && !cJSON_IsTrue(allowed)) {
            snprintf(message, sizeof(message), "a %s piece does not modulate", label ? label : "sized");
            return invalid_body(error, error_size, message);
        }
    }
    return URSATZ_GUI_MATERIAL_OK;
}

/* structure.form names a form of the Composer (absent is game-loop); the form says whether the structure lists its
 * own sections or is a size. */
static UrsatzGuiMaterialResult validate_piece_structure(const cJSON *structure, char *error, size_t error_size) {
    if (!cJSON_IsObject(structure)) {
        return invalid_body(error, error_size, "a piece structure needs key, mode, meter and sections");
    }
    const cJSON *form_name = cJSON_GetObjectItemCaseSensitive(structure, "form");
    if (!is_absent_or_null(structure, "form") && !cJSON_IsString(form_name)) {
        return invalid_body(error, error_size, "a piece form must be text");
    }
    const char *name = cJSON_IsString(form_name) ? form_name->valuestring : "game-loop";
    const cJSON *form = ursatz_gui_composer_form(name);
    if (!form) {
        return invalid_body(error, error_size, "a piece form must be one the Composer builds");
    }
    UrsatzGuiMaterialResult words = validate_piece_words(structure, error, error_size);
    if (words != URSATZ_GUI_MATERIAL_OK) {
        return words;
    }
    const char *sections = ursatz_gui_json_string(form, "sections");
    if (sections && strcmp(sections, "request") == 0) {
        return validate_piece_sections(structure, form, error, error_size);
    }
    return validate_piece_size(structure, form, error, error_size);
}

static UrsatzGuiMaterialResult validate_piece(const cJSON *piece, char *error, size_t error_size) {
    const cJSON *structure = cJSON_GetObjectItemCaseSensitive(piece, "structure");
    if (!cJSON_IsObject(piece) || (structure && !cJSON_IsNull(structure) && !cJSON_IsString(structure) &&
                                   !cJSON_IsObject(structure) && !cJSON_IsArray(structure))) {
        return invalid_body(error, error_size, "a piece body needs a piece object with a structure");
    }
    if (piece_is_placeholder(piece)) {
        return URSATZ_GUI_MATERIAL_OK;
    }
    UrsatzGuiMaterialResult result = validate_piece_structure(structure, error, error_size);
    if (result != URSATZ_GUI_MATERIAL_OK) {
        return result;
    }
    if (!is_absent_or_null(piece, "motive") && !theme_reference_ok(cJSON_GetObjectItemCaseSensitive(piece, "motive"))) {
        return invalid_body(error, error_size, "a piece motive needs item_id and version");
    }
    if (!is_in_set_or_null(piece, "pool", kThemePools)) {
        return invalid_body(error, error_size, "piece pool must be approved-and-unflagged, approved or none");
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(piece, "accompaniment");
    const char *pattern = ursatz_gui_json_string(accompaniment, "pattern");
    if (!is_absent_or_null(piece, "accompaniment") &&
        ((pattern && strcmp(pattern, "motive") == 0) ||
         validate_theme_accompaniment(accompaniment, error, error_size) != URSATZ_GUI_MATERIAL_OK)) {
        return invalid_body(error, error_size, "piece accompaniment pattern must be block, arpeggiated, alberti, oom-pah or any");
    }
    if (!is_absent_or_null(piece, "brief") && !theme_brief_ok(cJSON_GetObjectItemCaseSensitive(piece, "brief"))) {
        return invalid_body(error, error_size, "piece brief needs scopes, piece_ids or style_ids within the limits");
    }
    const cJSON *weight = cJSON_GetObjectItemCaseSensitive(piece, "style_weight");
    if (!is_absent_or_null(piece, "style_weight") && (!cJSON_IsString(weight) || !is_rational(weight->valuestring, false))) {
        return invalid_body(error, error_size, "piece style_weight must be n or n/d");
    }
    const cJSON *without = cJSON_GetObjectItemCaseSensitive(piece, "without");
    if (without && (!is_string_list_from(without, kThemeMotiveCategories) || cJSON_GetArraySize(without) > THEME_WITHOUT_MAX)) {
        return invalid_body(error, error_size, "piece without lists up to four motive categories");
    }
    const cJSON *tempo = cJSON_GetObjectItemCaseSensitive(piece, "tempo");
    if (!is_absent_or_null(piece, "tempo") && !is_whole_number(tempo, 0)) {
        return invalid_body(error, error_size, "piece tempo must be null or a whole number");
    }
    const cJSON *check = cJSON_GetObjectItemCaseSensitive(piece, "check");
    if (check && !cJSON_IsBool(check)) {
        return invalid_body(error, error_size, "piece check must be true or false");
    }
    if (!is_absent_or_null(piece, "seed") && !is_whole_number(cJSON_GetObjectItemCaseSensitive(piece, "seed"), 0)) {
        return invalid_body(error, error_size, "piece seed must be null or a whole number");
    }
    const cJSON *result_member = cJSON_GetObjectItemCaseSensitive(piece, "result");
    if (result_member && !cJSON_IsNull(result_member) && !cJSON_IsObject(result_member)) {
        return invalid_body(error, error_size, "piece result must be null or an object");
    }
    return URSATZ_GUI_MATERIAL_OK;
}

UrsatzGuiMaterialResult ursatz_gui_material_body_validate(const char *type, const cJSON *body, char *error,
                                                           size_t error_size) {
    if (error && error_size > 0) {
        error[0] = '\0';
    }
    if (!prefix_for_type(type)) {
        return URSATZ_GUI_MATERIAL_ERR_INVALID_TYPE;
    }
    if (!cJSON_IsObject(body)) {
        return invalid_body(error, error_size, "a version body must be an object");
    }
    UrsatzGuiMaterialResult result = validate_uses(body, error, error_size);
    if (result != URSATZ_GUI_MATERIAL_OK) {
        return result;
    }
    static const char *const members[] = {"motif", "theme", "piece"};
    for (size_t i = 0; i < sizeof(members) / sizeof(members[0]); i++) {
        if (strcmp(members[i], type) != 0 && cJSON_GetObjectItemCaseSensitive(body, members[i])) {
            return invalid_body(error, error_size, "a body must not carry another type's member");
        }
    }
    const cJSON *own = cJSON_GetObjectItemCaseSensitive(body, type);
    if (strcmp(type, "motif") == 0) {
        return validate_motif(own, error, error_size);
    }
    return strcmp(type, "theme") == 0 ? validate_theme(own, error, error_size) : validate_piece(own, error, error_size);
}

static UrsatzGuiMaterialResult load_item_locked(const char *id, cJSON **out) {
    char path[PATH_BYTES];
    if (!item_file_path(id, "item.json", path, sizeof(path))) {
        return URSATZ_GUI_MATERIAL_ERR_INVALID_ID;
    }
    cJSON *item = ursatz_gui_read_json_object(path);
    if (!item) {
        return ursatz_gui_path_exists(path) ? URSATZ_GUI_MATERIAL_ERR_FAILED : URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
    }
    if (!ursatz_gui_json_set_string(item, "id", id)) {
        cJSON_Delete(item);
        return URSATZ_GUI_MATERIAL_ERR_ALLOC;
    }
    *out = item;
    return URSATZ_GUI_MATERIAL_OK;
}

static UrsatzGuiMaterialResult save_item_locked(const cJSON *item) {
    char path[PATH_BYTES];
    if (!item_file_path(ursatz_gui_json_string(item, "id"), "item.json", path, sizeof(path))) {
        return URSATZ_GUI_MATERIAL_ERR_INVALID_ID;
    }
    return write_json(path, item) ? URSATZ_GUI_MATERIAL_OK : URSATZ_GUI_MATERIAL_ERR_FAILED;
}

static int current_version_of(const cJSON *item) {
    const cJSON *number = cJSON_GetObjectItemCaseSensitive(item, "current_version");
    return cJSON_IsNumber(number) ? number->valueint : 0;
}

static bool touch_item(cJSON *item) {
    char now[32];
    ursatz_gui_iso_utc_now(now, sizeof(now));
    return ursatz_gui_json_set_string(item, "updated_at", now);
}

/* Removes every file in the item directory and in the folders under it (a theme version's files), then the directory. */
static void remove_item_dir(const char *dir) {
    char **names = NULL;
    size_t count = 0;
    if (list_names(dir, &names, &count)) {
        for (size_t i = 0; i < count; i++) {
            char path[PATH_BYTES];
            if (snprintf(path, sizeof(path), "%s/%s", dir, names[i]) < (int)sizeof(path)) {
                if (ursatz_gui_path_is_dir(path)) {
                    remove_item_dir(path);
                } else {
                    remove(path);
                }
            }
        }
        free_names(names, count);
    }
#ifdef _WIN32
    _rmdir(dir);
#else
    rmdir(dir);
#endif
}

static bool write_version_locked(const char *id, int number, const cJSON *body) {
    char path[PATH_BYTES];
    return version_path(id, number, path, sizeof(path)) && write_json(path, body);
}

/* Appends {number, created_at, note} to the item's versions and makes it current. */
static bool append_version_entry(cJSON *item, int number, const char *note) {
    char now[32];
    ursatz_gui_iso_utc_now(now, sizeof(now));
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

static UrsatzGuiMaterialResult read_version_locked(const char *id, int number, cJSON **out_body) {
    char path[PATH_BYTES];
    if (!version_path(id, number, path, sizeof(path))) {
        return URSATZ_GUI_MATERIAL_ERR_INVALID_ID;
    }
    cJSON *body = ursatz_gui_read_json_object(path);
    if (!body) {
        return ursatz_gui_path_exists(path) ? URSATZ_GUI_MATERIAL_ERR_FAILED : URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
    }
    *out_body = body;
    return URSATZ_GUI_MATERIAL_OK;
}

/*
 * Returns in *out a copy of body whose uses each carry a fresh "copy" of the
 * version they name (a client-sent copy is replaced). A use of an item or
 * version that does not exist, or of self_id (may be NULL), is INVALID_BODY.
 */
static UrsatzGuiMaterialResult fill_uses_locked(const char *self_id, const cJSON *body, cJSON **out) {
    cJSON *filled = cJSON_Duplicate(body, 1);
    if (!filled) {
        return URSATZ_GUI_MATERIAL_ERR_ALLOC;
    }
    UrsatzGuiMaterialResult result = URSATZ_GUI_MATERIAL_OK;
    cJSON *use = NULL;
    cJSON_ArrayForEach(use, cJSON_GetObjectItemCaseSensitive(filled, "uses")) {
        const char *source_id = ursatz_gui_json_string(use, "item_id");
        int number = cJSON_GetObjectItemCaseSensitive(use, "version")->valueint;
        cJSON *source = NULL;
        cJSON *copy = NULL;
        if (self_id && strcmp(source_id, self_id) == 0) {
            result = URSATZ_GUI_MATERIAL_ERR_INVALID_BODY;
        } else {
            result = load_item_locked(source_id, &source);
        }
        if (result == URSATZ_GUI_MATERIAL_OK) {
            result = number <= current_version_of(source) ? read_version_locked(source_id, number, &copy)
                                                           : URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
        }
        cJSON_Delete(source);
        if (result == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND || result == URSATZ_GUI_MATERIAL_ERR_INVALID_ID) {
            result = URSATZ_GUI_MATERIAL_ERR_INVALID_BODY;
        }
        if (result == URSATZ_GUI_MATERIAL_OK) {
            cJSON_DeleteItemFromObjectCaseSensitive(use, "copy");
            if (!cJSON_AddItemToObject(use, "copy", copy)) {
                cJSON_Delete(copy);
                result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
            }
        }
        if (result != URSATZ_GUI_MATERIAL_OK) {
            break;
        }
    }
    if (result != URSATZ_GUI_MATERIAL_OK) {
        cJSON_Delete(filled);
        return result;
    }
    *out = filled;
    return URSATZ_GUI_MATERIAL_OK;
}

#define BODY_REFERENCES_MAX 40

typedef enum { REF_MELODIC_MOTIF, REF_ACCOMPANIMENT_MOTIF, REF_THEME } ReferenceKind;

/*
 * The references a body makes to versions of other items: a theme's slots in order, its ideas, then its accompaniment when its
 * pattern is "motive"; a piece's motive, then the motif and theme fills of its sections in order. Placeholders make
 * none. refs receives up to BODY_REFERENCES_MAX pointers into body, kinds the kind of each; the count is returned.
 */
static size_t body_references(const cJSON *body, const cJSON *refs[BODY_REFERENCES_MAX], ReferenceKind kinds[BODY_REFERENCES_MAX]) {
    size_t count = 0;
    const cJSON *theme = cJSON_GetObjectItemCaseSensitive(body, "theme");
    if (cJSON_IsObject(theme) && !theme_is_placeholder(theme)) {
        const cJSON *slot = NULL;
        cJSON_ArrayForEach(slot, cJSON_GetObjectItemCaseSensitive(theme, "slots")) {
            if (count < 4) {
                kinds[count] = REF_MELODIC_MOTIF;
                refs[count++] = slot;
            }
        }
        const cJSON *idea = NULL;
        cJSON_ArrayForEach(idea, cJSON_GetObjectItemCaseSensitive(theme, "ideas")) {
            if (count < BODY_REFERENCES_MAX) {
                kinds[count] = REF_MELODIC_MOTIF;
                refs[count++] = idea;
            }
        }
        const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(theme, "accompaniment");
        const cJSON *pattern = cJSON_GetObjectItemCaseSensitive(accompaniment, "pattern");
        if (cJSON_IsObject(accompaniment) && cJSON_IsString(pattern) && strcmp(pattern->valuestring, "motive") == 0) {
            kinds[count] = REF_ACCOMPANIMENT_MOTIF;
            refs[count++] = accompaniment;
        }
    }
    const cJSON *piece = cJSON_GetObjectItemCaseSensitive(body, "piece");
    if (cJSON_IsObject(piece) && !piece_is_placeholder(piece)) {
        const cJSON *motive = cJSON_GetObjectItemCaseSensitive(piece, "motive");
        if (cJSON_IsObject(motive)) {
            kinds[count] = REF_MELODIC_MOTIF;
            refs[count++] = motive;
        }
        const cJSON *section = NULL;
        cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(piece, "structure"), "sections")) {
            const cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
            const char *kind = ursatz_gui_json_string(fill, "kind");
            bool theme_fill = strcmp(kind, "theme") == 0;
            if (count < BODY_REFERENCES_MAX && (theme_fill || strcmp(kind, "motif") == 0)) {
                kinds[count] = theme_fill ? REF_THEME : REF_MELODIC_MOTIF;
                refs[count++] = fill;
            }
        }
    }
    return count;
}

/*
 * What a reference names must exist at that version: a motif with form full and the role the use needs, or a theme
 * version that was composed (a result and a plan.json beside the version).
 */
static UrsatzGuiMaterialResult check_reference_locked(const cJSON *reference, ReferenceKind kind) {
    const char *source_id = ursatz_gui_json_string(reference, "item_id");
    int number = cJSON_GetObjectItemCaseSensitive(reference, "version")->valueint;
    bool theme = kind == REF_THEME;
    cJSON *source = NULL;
    cJSON *version = NULL;
    UrsatzGuiMaterialResult result = load_item_locked(source_id, &source);
    if (result == URSATZ_GUI_MATERIAL_OK) {
        result = strcmp(ursatz_gui_json_string(source, "type"), theme ? "theme" : "motif") == 0 && number <= current_version_of(source)
                     ? read_version_locked(source_id, number, &version)
                     : URSATZ_GUI_MATERIAL_ERR_INVALID_BODY;
    }
    if (result == URSATZ_GUI_MATERIAL_OK && theme) {
        char plan[PATH_BYTES];
        const cJSON *composed = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(version, "theme"), "result");
        if (!cJSON_IsObject(composed) ||
            snprintf(plan, sizeof(plan), "%s/items/%s/v%d/plan.json", g_dir, source_id, number) >= (int)sizeof(plan) ||
            !ursatz_gui_path_exists(plan)) {
            result = URSATZ_GUI_MATERIAL_ERR_INVALID_BODY;
        }
    } else if (result == URSATZ_GUI_MATERIAL_OK) {
        const cJSON *motif = cJSON_GetObjectItemCaseSensitive(version, "motif");
        const char *form = ursatz_gui_json_string(motif, "form");
        const char *role = ursatz_gui_json_string(motif, "role");
        if (!form || !role || strcmp(form, "full") != 0 ||
            strcmp(role, kind == REF_ACCOMPANIMENT_MOTIF ? "accompaniment" : "melodic") != 0) {
            result = URSATZ_GUI_MATERIAL_ERR_INVALID_BODY;
        }
    }
    if (result == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND || result == URSATZ_GUI_MATERIAL_ERR_INVALID_ID) {
        result = URSATZ_GUI_MATERIAL_ERR_INVALID_BODY;
    }
    cJSON_Delete(source);
    cJSON_Delete(version);
    return result;
}

/* Adds {item_id, version} to uses unless an entry with both is there. false on allocation failure. */
static bool add_use(cJSON *uses, const char *item_id, int version) {
    const cJSON *use = NULL;
    cJSON_ArrayForEach(use, uses) {
        if (strcmp(ursatz_gui_json_string(use, "item_id"), item_id) == 0 &&
            cJSON_GetObjectItemCaseSensitive(use, "version")->valueint == version) {
            return true;
        }
    }
    cJSON *entry = cJSON_CreateObject();
    if (!entry || !cJSON_AddStringToObject(entry, "item_id", item_id) || !cJSON_AddNumberToObject(entry, "version", version)) {
        cJSON_Delete(entry);
        return false;
    }
    cJSON_AddItemToArray(uses, entry);
    return true;
}

/*
 * Returns in *out a copy of body. For a theme or piece that is not a placeholder, the motifs and themes it names
 * are checked, and its uses of those items are replaced by one use for each version named (a client use of them is
 * dropped; its other uses stay). Any other body is copied as it is.
 */
static UrsatzGuiMaterialResult prepare_uses_locked(const cJSON *body, cJSON **out) {
    cJSON *prepared = cJSON_Duplicate(body, 1);
    if (!prepared) {
        return URSATZ_GUI_MATERIAL_ERR_ALLOC;
    }
    const cJSON *refs[BODY_REFERENCES_MAX];
    ReferenceKind kinds[BODY_REFERENCES_MAX];
    size_t count = body_references(prepared, refs, kinds);
    if (count == 0) {
        *out = prepared;
        return URSATZ_GUI_MATERIAL_OK;
    }
    UrsatzGuiMaterialResult result = URSATZ_GUI_MATERIAL_OK;
    for (size_t i = 0; result == URSATZ_GUI_MATERIAL_OK && i < count; i++) {
        result = check_reference_locked(refs[i], kinds[i]);
    }
    cJSON *uses = cJSON_CreateArray();
    if (result == URSATZ_GUI_MATERIAL_OK && !uses) {
        result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
    }
    const cJSON *use = NULL;
    cJSON_ArrayForEach(use, cJSON_GetObjectItemCaseSensitive(prepared, "uses")) {
        bool replaced = false;
        for (size_t i = 0; i < count; i++) {
            replaced = replaced || strcmp(ursatz_gui_json_string(refs[i], "item_id"), ursatz_gui_json_string(use, "item_id")) == 0;
        }
        if (result == URSATZ_GUI_MATERIAL_OK && !replaced) {
            cJSON *kept = cJSON_Duplicate(use, 1);
            if (kept) {
                cJSON_AddItemToArray(uses, kept);
            } else {
                result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
            }
        }
    }
    for (size_t i = 0; result == URSATZ_GUI_MATERIAL_OK && i < count; i++) {
        if (!add_use(uses, ursatz_gui_json_string(refs[i], "item_id"),
                           cJSON_GetObjectItemCaseSensitive(refs[i], "version")->valueint)) {
            result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
        }
    }
    if (result == URSATZ_GUI_MATERIAL_OK) {
        cJSON_DeleteItemFromObjectCaseSensitive(prepared, "uses");
        if (!cJSON_AddItemToObject(prepared, "uses", uses)) {
            result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
        } else {
            uses = NULL;
        }
    }
    cJSON_Delete(uses);
    if (result != URSATZ_GUI_MATERIAL_OK) {
        cJSON_Delete(prepared);
        return result;
    }
    *out = prepared;
    return URSATZ_GUI_MATERIAL_OK;
}

/* prepare_uses_locked, then fill_uses_locked: *out is body with the uses a theme's slots imply and a fresh copy in each. */
static UrsatzGuiMaterialResult fill_body_uses_locked(const char *self_id, const cJSON *body, cJSON **out) {
    cJSON *prepared = NULL;
    UrsatzGuiMaterialResult result = prepare_uses_locked(body, &prepared);
    if (result == URSATZ_GUI_MATERIAL_OK) {
        result = fill_uses_locked(self_id, prepared, out);
    }
    cJSON_Delete(prepared);
    return result;
}

UrsatzGuiMaterialResult ursatz_gui_material_body_with_uses(const cJSON *body, cJSON **out) {
    *out = NULL;
    LOCK();
    UrsatzGuiMaterialResult result = fill_body_uses_locked(NULL, body, out);
    UNLOCK();
    return result;
}

/* Points every reference of a theme or piece body that names source_id at version latest. false on allocation failure. */
static bool bump_versions(cJSON *body, const char *source_id, int latest) {
    const cJSON *refs[BODY_REFERENCES_MAX];
    ReferenceKind kinds[BODY_REFERENCES_MAX];
    size_t count = body_references(body, refs, kinds);
    bool ok = true;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(ursatz_gui_json_string(refs[i], "item_id"), source_id) == 0) {
            cJSON_DeleteItemFromObjectCaseSensitive((cJSON *)refs[i], "version");
            ok = ok && cJSON_AddNumberToObject((cJSON *)refs[i], "version", latest) != NULL;
        }
    }
    return ok;
}

static UrsatzGuiMaterialResult create_item_locked(const char *type, const char *name, const char *project_id,
                                                  const cJSON *body, const char *note, cJSON **out_item) {
    const char *prefix = prefix_for_type(type);
    if (!prefix) {
        return URSATZ_GUI_MATERIAL_ERR_INVALID_TYPE;
    }
    if (!is_valid_id(project_id)) {
        return URSATZ_GUI_MATERIAL_ERR_INVALID_ID;
    }
    char *clean_name = NULL;
    char *clean_note = NULL;
    UrsatzGuiMaterialResult result = normalize_name(name, &clean_name);
    if (result == URSATZ_GUI_MATERIAL_OK) {
        bool alloc_failed = false;
        clean_note = trim_text(note, NOTE_MAX_CHARS, &alloc_failed);
        if (!clean_note) {
            result = alloc_failed ? URSATZ_GUI_MATERIAL_ERR_ALLOC : URSATZ_GUI_MATERIAL_ERR_INVALID_NAME;
        }
    }
    if (result == URSATZ_GUI_MATERIAL_OK && !project_exists_locked(project_id)) {
        result = URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
    }
    cJSON *stored = NULL;
    if (result == URSATZ_GUI_MATERIAL_OK) {
        result = ursatz_gui_material_body_validate(type, body, NULL, 0);
    }
    if (result == URSATZ_GUI_MATERIAL_OK) {
        result = fill_body_uses_locked(NULL, body, &stored);
    }

    char *id = NULL;
    char dir[PATH_BYTES];
    for (int attempt = 0; result == URSATZ_GUI_MATERIAL_OK && attempt < ID_MAX_TRIES; attempt++) {
        id = ursatz_gui_new_id(prefix);
        if (!id) {
            result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
        } else if (item_dir_path(id, dir, sizeof(dir)) && !ursatz_gui_path_exists(dir)) {
            break;
        } else {
            free(id);
            id = NULL;
        }
    }
    if (result == URSATZ_GUI_MATERIAL_OK && !id) {
        result = URSATZ_GUI_MATERIAL_ERR_FAILED;
    }

    cJSON *item = NULL;
    bool dir_made = false;
    if (result == URSATZ_GUI_MATERIAL_OK) {
        char now[32];
        ursatz_gui_iso_utc_now(now, sizeof(now));
        item = cJSON_CreateObject();
        bool ok = item && cJSON_AddStringToObject(item, "id", id) && cJSON_AddStringToObject(item, "type", type) &&
                  cJSON_AddStringToObject(item, "name", clean_name) &&
                  cJSON_AddStringToObject(item, "project_id", project_id) &&
                  cJSON_AddStringToObject(item, "created_at", now) && cJSON_AddStringToObject(item, "updated_at", now) &&
                  append_version_entry(item, 1, clean_note);
        if (!ok) {
            result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
        } else if (!ursatz_gui_make_dir(dir)) {
            result = URSATZ_GUI_MATERIAL_ERR_FAILED;
        } else {
            dir_made = true;
            /* item.json goes last: a directory without it is not an item. */
            if (!write_version_locked(id, 1, stored)) {
                result = URSATZ_GUI_MATERIAL_ERR_FAILED;
            } else {
                result = save_item_locked(item);
            }
        }
    }
    if (result != URSATZ_GUI_MATERIAL_OK && dir_made) {
        remove_item_dir(dir);
    }
    if (result == URSATZ_GUI_MATERIAL_OK && out_item) {
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

UrsatzGuiMaterialResult ursatz_gui_material_item_create_noted(const char *type, const char *name,
                                                               const char *project_id, const cJSON *body,
                                                               const char *note, cJSON **out_item) {
    if (out_item) {
        *out_item = NULL;
    }
    LOCK();
    UrsatzGuiMaterialResult result = g_ready ? create_item_locked(type, name, project_id, body, note, out_item)
                                             : URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    UNLOCK();
    return result;
}

UrsatzGuiMaterialResult ursatz_gui_material_item_create(const char *type, const char *name, const char *project_id,
                                                         const cJSON *body, cJSON **out_item) {
    return ursatz_gui_material_item_create_noted(type, name, project_id, body, NULL, out_item);
}

bool ursatz_gui_material_item_is_variant(const cJSON *body) {
    const cJSON *motif = cJSON_GetObjectItemCaseSensitive(body, "motif");
    const cJSON *derived = cJSON_GetObjectItemCaseSensitive(motif, "derived");
    return cJSON_IsObject(derived) && !cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(derived, "standalone"));
}

/* Sets "variant_of" on an item object: the source item of a variant, else null. The member is added to answers only;
 * item.json never carries it. known_body is the item's current version when the caller has it, else NULL and a motif
 * reads it. */
static bool annotate_variant_of_locked(cJSON *item, const cJSON *known_body) {
    cJSON *own_body = NULL;
    const cJSON *body = known_body;
    if (!body && strcmp(ursatz_gui_json_string(item, "type"), "motif") == 0 &&
        read_version_locked(ursatz_gui_json_string(item, "id"), current_version_of(item), &own_body) == URSATZ_GUI_MATERIAL_OK) {
        body = own_body;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(item, "variant_of");
    bool ok;
    if (body && ursatz_gui_material_item_is_variant(body)) {
        const cJSON *derived = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(body, "motif"), "derived");
        ok = cJSON_AddStringToObject(item, "variant_of", ursatz_gui_json_string(derived, "source_item")) != NULL;
    } else {
        ok = cJSON_AddNullToObject(item, "variant_of") != NULL;
    }
    cJSON_Delete(own_body);
    return ok;
}

static int compare_items(const void *a, const void *b) {
    const cJSON *left = *(const cJSON *const *)a;
    const cJSON *right = *(const cJSON *const *)b;
    int by_updated = strcmp(ursatz_gui_json_string(right, "updated_at"), ursatz_gui_json_string(left, "updated_at"));
    return by_updated != 0 ? by_updated : strcmp(ursatz_gui_json_string(left, "id"), ursatz_gui_json_string(right, "id"));
}

UrsatzGuiMaterialResult ursatz_gui_material_item_list(const char *project_id, const char *type, cJSON **out_items) {
    *out_items = NULL;
    if (project_id && !is_valid_id(project_id)) {
        return URSATZ_GUI_MATERIAL_ERR_INVALID_ID;
    }
    if (type && !prefix_for_type(type)) {
        return URSATZ_GUI_MATERIAL_ERR_INVALID_TYPE;
    }
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    }

    char dir[PATH_BYTES];
    char **names = NULL;
    size_t count = 0;
    if (snprintf(dir, sizeof(dir), "%s/items", g_dir) >= (int)sizeof(dir) || !list_names(dir, &names, &count)) {
        UNLOCK();
        return URSATZ_GUI_MATERIAL_ERR_FAILED;
    }

    UrsatzGuiMaterialResult result = URSATZ_GUI_MATERIAL_OK;
    cJSON **items = count ? calloc(count, sizeof(cJSON *)) : NULL;
    size_t loaded = 0;
    if (count && !items) {
        result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
    }
    for (size_t i = 0; result == URSATZ_GUI_MATERIAL_OK && i < count; i++) {
        cJSON *item = NULL;
        if (load_item_locked(names[i], &item) != URSATZ_GUI_MATERIAL_OK) {
            continue;
        }
        if ((project_id && strcmp(ursatz_gui_json_string(item, "project_id"), project_id) != 0) ||
            (type && strcmp(ursatz_gui_json_string(item, "type"), type) != 0)) {
            cJSON_Delete(item);
            continue;
        }
        if (!annotate_variant_of_locked(item, NULL)) {
            cJSON_Delete(item);
            result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
            break;
        }
        items[loaded++] = item;
    }
    free_names(names, count);
    UNLOCK();

    cJSON *array = result == URSATZ_GUI_MATERIAL_OK ? cJSON_CreateArray() : NULL;
    if (result == URSATZ_GUI_MATERIAL_OK && !array) {
        result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
    }
    if (loaded > 1) {
        qsort(items, loaded, sizeof(cJSON *), compare_items);
    }
    for (size_t i = 0; i < loaded; i++) {
        if (result == URSATZ_GUI_MATERIAL_OK) {
            cJSON_AddItemToArray(array, items[i]);
        } else {
            cJSON_Delete(items[i]);
        }
    }
    free(items);
    if (result == URSATZ_GUI_MATERIAL_OK) {
        *out_items = array;
    }
    return result;
}

UrsatzGuiMaterialResult ursatz_gui_material_item_get(const char *id, cJSON **out_meta, cJSON **out_body) {
    if (out_meta) {
        *out_meta = NULL;
    }
    if (out_body) {
        *out_body = NULL;
    }
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    }
    cJSON *item = NULL;
    cJSON *body = NULL;
    UrsatzGuiMaterialResult result = load_item_locked(id, &item);
    if (result == URSATZ_GUI_MATERIAL_OK && out_body) {
        result = read_version_locked(id, current_version_of(item), &body);
    }
    if (result == URSATZ_GUI_MATERIAL_OK && out_meta && !annotate_variant_of_locked(item, body)) {
        result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
    }
    UNLOCK();
    if (result == URSATZ_GUI_MATERIAL_OK && out_meta) {
        *out_meta = item;
    } else {
        cJSON_Delete(item);
    }
    if (result == URSATZ_GUI_MATERIAL_OK && out_body) {
        *out_body = body;
    }
    return result;
}

UrsatzGuiMaterialResult ursatz_gui_material_item_get_version(const char *id, int number, cJSON **out_body) {
    *out_body = NULL;
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    }
    cJSON *item = NULL;
    UrsatzGuiMaterialResult result = load_item_locked(id, &item);
    if (result == URSATZ_GUI_MATERIAL_OK) {
        result = number >= 1 && number <= current_version_of(item) ? read_version_locked(id, number, out_body)
                                                                    : URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
    }
    cJSON_Delete(item);
    UNLOCK();
    return result;
}

UrsatzGuiMaterialResult ursatz_gui_material_uses_status(const char *id, cJSON **out_status) {
    *out_status = NULL;
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    }
    cJSON *item = NULL;
    cJSON *body = NULL;
    cJSON *status = NULL;
    UrsatzGuiMaterialResult result = load_item_locked(id, &item);
    if (result == URSATZ_GUI_MATERIAL_OK) {
        result = read_version_locked(id, current_version_of(item), &body);
    }
    if (result == URSATZ_GUI_MATERIAL_OK) {
        status = cJSON_CreateArray();
        result = status ? URSATZ_GUI_MATERIAL_OK : URSATZ_GUI_MATERIAL_ERR_ALLOC;
    }
    const cJSON *uses = result == URSATZ_GUI_MATERIAL_OK ? cJSON_GetObjectItemCaseSensitive(body, "uses") : NULL;
    const cJSON *use = NULL;
    cJSON_ArrayForEach(use, uses) {
        const char *source_id = ursatz_gui_json_string(use, "item_id");
        const cJSON *version = cJSON_GetObjectItemCaseSensitive(use, "version");
        int used = cJSON_IsNumber(version) ? version->valueint : 0;
        cJSON *source = NULL;
        cJSON *entry = cJSON_CreateObject();
        bool ok = entry && cJSON_AddStringToObject(entry, "item_id", source_id) &&
                  cJSON_AddNumberToObject(entry, "version", used);
        if (ok && load_item_locked(source_id, &source) == URSATZ_GUI_MATERIAL_OK) {
            int latest = current_version_of(source);
            ok = cJSON_AddNumberToObject(entry, "current_version", latest) &&
                 cJSON_AddStringToObject(entry, "name", ursatz_gui_json_string(source, "name")) &&
                 cJSON_AddStringToObject(entry, "type", ursatz_gui_json_string(source, "type")) &&
                 cJSON_AddStringToObject(entry, "project_id", ursatz_gui_json_string(source, "project_id")) &&
                 cJSON_AddBoolToObject(entry, "updated", latest > used) && cJSON_AddBoolToObject(entry, "missing", false);
        } else if (ok) {
            ok = cJSON_AddBoolToObject(entry, "updated", false) && cJSON_AddBoolToObject(entry, "missing", true);
        }
        cJSON_Delete(source);
        if (!ok) {
            cJSON_Delete(entry);
            result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
            break;
        }
        cJSON_AddItemToArray(status, entry);
    }
    UNLOCK();
    cJSON_Delete(item);
    cJSON_Delete(body);
    if (result == URSATZ_GUI_MATERIAL_OK) {
        *out_status = status;
    } else {
        cJSON_Delete(status);
    }
    return result;
}

/*
 * Builds the array of items whose current version uses id: {id, name, type,
 * project_id, version}. The item need not exist (a deleted source still has users).
 */
static UrsatzGuiMaterialResult used_by_locked(const char *id, cJSON **out) {
    char dir[PATH_BYTES];
    char **names = NULL;
    size_t count = 0;
    if (!is_valid_id(id)) {
        return URSATZ_GUI_MATERIAL_ERR_INVALID_ID;
    }
    if (snprintf(dir, sizeof(dir), "%s/items", g_dir) >= (int)sizeof(dir) || !list_names(dir, &names, &count)) {
        return URSATZ_GUI_MATERIAL_ERR_FAILED;
    }
    cJSON *users = cJSON_CreateArray();
    UrsatzGuiMaterialResult result = users ? URSATZ_GUI_MATERIAL_OK : URSATZ_GUI_MATERIAL_ERR_ALLOC;
    for (size_t i = 0; result == URSATZ_GUI_MATERIAL_OK && i < count; i++) {
        cJSON *item = NULL;
        cJSON *body = NULL;
        if (strcmp(names[i], id) == 0 || load_item_locked(names[i], &item) != URSATZ_GUI_MATERIAL_OK) {
            continue;
        }
        if (read_version_locked(names[i], current_version_of(item), &body) == URSATZ_GUI_MATERIAL_OK) {
            const cJSON *use = NULL;
            cJSON_ArrayForEach(use, cJSON_GetObjectItemCaseSensitive(body, "uses")) {
                const cJSON *version = cJSON_GetObjectItemCaseSensitive(use, "version");
                if (strcmp(ursatz_gui_json_string(use, "item_id"), id) != 0) {
                    continue;
                }
                cJSON *entry = cJSON_CreateObject();
                bool ok = entry && cJSON_AddStringToObject(entry, "id", names[i]) &&
                          cJSON_AddStringToObject(entry, "name", ursatz_gui_json_string(item, "name")) &&
                          cJSON_AddStringToObject(entry, "type", ursatz_gui_json_string(item, "type")) &&
                          cJSON_AddStringToObject(entry, "project_id", ursatz_gui_json_string(item, "project_id")) &&
                          cJSON_AddNumberToObject(entry, "version", cJSON_IsNumber(version) ? version->valueint : 0);
                if (!ok) {
                    cJSON_Delete(entry);
                    result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
                } else {
                    cJSON_AddItemToArray(users, entry);
                }
                break;
            }
        }
        cJSON_Delete(item);
        cJSON_Delete(body);
    }
    free_names(names, count);
    if (result == URSATZ_GUI_MATERIAL_OK) {
        *out = users;
    } else {
        cJSON_Delete(users);
    }
    return result;
}

static UrsatzGuiMaterialResult derived_motifs_locked(cJSON **out) {
    char dir[PATH_BYTES];
    char **names = NULL;
    size_t count = 0;
    if (snprintf(dir, sizeof(dir), "%s/items", g_dir) >= (int)sizeof(dir) || !list_names(dir, &names, &count)) {
        return URSATZ_GUI_MATERIAL_ERR_FAILED;
    }
    cJSON *found = cJSON_CreateArray();
    UrsatzGuiMaterialResult result = found ? URSATZ_GUI_MATERIAL_OK : URSATZ_GUI_MATERIAL_ERR_ALLOC;
    for (size_t i = 0; result == URSATZ_GUI_MATERIAL_OK && i < count; i++) {
        cJSON *item = NULL;
        cJSON *body = NULL;
        if (load_item_locked(names[i], &item) != URSATZ_GUI_MATERIAL_OK) {
            continue;
        }
        if (strcmp(ursatz_gui_json_string(item, "type"), "motif") == 0 &&
            read_version_locked(names[i], current_version_of(item), &body) == URSATZ_GUI_MATERIAL_OK) {
            cJSON *derived = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(body, "motif"), "derived");
            if (cJSON_IsObject(derived)) {
                cJSON *entry = cJSON_CreateObject();
                cJSON *copy = cJSON_Duplicate(derived, 1);
                bool ok = entry && copy && cJSON_AddStringToObject(entry, "id", names[i]) &&
                          cJSON_AddStringToObject(entry, "name", ursatz_gui_json_string(item, "name")) &&
                          cJSON_AddNumberToObject(entry, "current_version", current_version_of(item)) &&
                          cJSON_AddItemToObject(entry, "derived", copy);
                if (!ok) {
                    if (entry && !cJSON_GetObjectItemCaseSensitive(entry, "derived")) {
                        cJSON_Delete(copy);
                    }
                    cJSON_Delete(entry);
                    result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
                } else {
                    cJSON_AddItemToArray(found, entry);
                }
            }
        }
        cJSON_Delete(item);
        cJSON_Delete(body);
    }
    free_names(names, count);
    if (result == URSATZ_GUI_MATERIAL_OK) {
        *out = found;
    } else {
        cJSON_Delete(found);
    }
    return result;
}

UrsatzGuiMaterialResult ursatz_gui_material_derived_motifs(cJSON **out_motifs) {
    *out_motifs = NULL;
    LOCK();
    UrsatzGuiMaterialResult result = g_ready ? derived_motifs_locked(out_motifs) : URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    UNLOCK();
    return result;
}

UrsatzGuiMaterialResult ursatz_gui_material_used_by(const char *id, cJSON **out_users) {
    *out_users = NULL;
    LOCK();
    UrsatzGuiMaterialResult result = g_ready ? used_by_locked(id, out_users) : URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    UNLOCK();
    return result;
}

/* Sets the name and/or project (each NULL = unchanged) on the item and saves it. */
static UrsatzGuiMaterialResult change_item(const char *id, const char *name, const char *project_id,
                                           cJSON **out_item) {
    if (out_item) {
        *out_item = NULL;
    }
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    }
    cJSON *item = NULL;
    char *clean_name = NULL;
    UrsatzGuiMaterialResult result = load_item_locked(id, &item);
    if (result == URSATZ_GUI_MATERIAL_OK && name) {
        result = normalize_name(name, &clean_name);
    }
    if (result == URSATZ_GUI_MATERIAL_OK && project_id) {
        if (!is_valid_id(project_id)) {
            result = URSATZ_GUI_MATERIAL_ERR_INVALID_ID;
        } else if (!project_exists_locked(project_id)) {
            result = URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
        }
    }
    if (result == URSATZ_GUI_MATERIAL_OK) {
        bool ok = (!clean_name || ursatz_gui_json_set_string(item, "name", clean_name)) &&
                  (!project_id || ursatz_gui_json_set_string(item, "project_id", project_id)) && touch_item(item);
        result = ok ? save_item_locked(item) : URSATZ_GUI_MATERIAL_ERR_ALLOC;
    }
    if (result == URSATZ_GUI_MATERIAL_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    UNLOCK();
    free(clean_name);
    return result;
}

UrsatzGuiMaterialResult ursatz_gui_material_item_rename(const char *id, const char *name, cJSON **out_item) {
    if (!name) {
        return URSATZ_GUI_MATERIAL_ERR_INVALID_NAME;
    }
    return change_item(id, name, NULL, out_item);
}

UrsatzGuiMaterialResult ursatz_gui_material_item_move(const char *id, const char *project_id, cJSON **out_item) {
    if (!project_id) {
        return URSATZ_GUI_MATERIAL_ERR_INVALID_ID;
    }
    return change_item(id, NULL, project_id, out_item);
}

/* Writes body as the next version of item and saves item.json with it made current. */
static UrsatzGuiMaterialResult commit_version_locked(const char *id, cJSON *item, const cJSON *body, const char *note) {
    int number = current_version_of(item) + 1;
    /* The body goes first; the version exists once item.json names it. */
    if (!append_version_entry(item, number, note)) {
        return URSATZ_GUI_MATERIAL_ERR_ALLOC;
    }
    if (!write_version_locked(id, number, body)) {
        return URSATZ_GUI_MATERIAL_ERR_FAILED;
    }
    return save_item_locked(item);
}

UrsatzGuiMaterialResult ursatz_gui_material_item_add_version(const char *id, const cJSON *body, const char *note,
                                                              cJSON **out_item) {
    if (out_item) {
        *out_item = NULL;
    }
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    }
    cJSON *item = NULL;
    char *clean_note = NULL;
    bool alloc_failed = false;
    UrsatzGuiMaterialResult result = load_item_locked(id, &item);
    if (result == URSATZ_GUI_MATERIAL_OK) {
        clean_note = trim_text(note, NOTE_MAX_CHARS, &alloc_failed);
        if (!clean_note) {
            result = alloc_failed ? URSATZ_GUI_MATERIAL_ERR_ALLOC : URSATZ_GUI_MATERIAL_ERR_INVALID_NAME;
        }
    }
    if (result == URSATZ_GUI_MATERIAL_OK) {
        result = ursatz_gui_material_body_validate(ursatz_gui_json_string(item, "type"), body, NULL, 0);
    }
    cJSON *stored = NULL;
    if (result == URSATZ_GUI_MATERIAL_OK) {
        result = fill_body_uses_locked(id, body, &stored);
    }
    if (result == URSATZ_GUI_MATERIAL_OK) {
        result = commit_version_locked(id, item, stored, clean_note);
    }
    if (result == URSATZ_GUI_MATERIAL_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    UNLOCK();
    cJSON_Delete(stored);
    free(clean_note);
    return result;
}

UrsatzGuiMaterialResult ursatz_gui_material_update_use(const char *id, const char *source_id, cJSON **out_item) {
    if (out_item) {
        *out_item = NULL;
    }
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    }
    cJSON *item = NULL;
    cJSON *source = NULL;
    cJSON *body = NULL;
    cJSON *stored = NULL;
    char note[NAME_MAX_CHARS + 40];
    UrsatzGuiMaterialResult result = is_valid_id(source_id) ? load_item_locked(id, &item)
                                                            : URSATZ_GUI_MATERIAL_ERR_INVALID_ID;
    if (result == URSATZ_GUI_MATERIAL_OK) {
        result = read_version_locked(id, current_version_of(item), &body);
    }
    if (result == URSATZ_GUI_MATERIAL_OK) {
        result = load_item_locked(source_id, &source);
    }
    if (result == URSATZ_GUI_MATERIAL_OK) {
        /* Point every use of the source at its current version; fill_uses_locked refreshes the copies. */
        int latest = current_version_of(source);
        bool found = false;
        bool ok = true;
        cJSON *use = NULL;
        cJSON_ArrayForEach(use, cJSON_GetObjectItemCaseSensitive(body, "uses")) {
            if (strcmp(ursatz_gui_json_string(use, "item_id"), source_id) == 0) {
                found = true;
                cJSON_DeleteItemFromObjectCaseSensitive(use, "version");
                ok = ok && cJSON_AddNumberToObject(use, "version", latest) != NULL;
            }
        }
        if (!ok || !bump_versions(body, source_id, latest)) {
            result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
        } else if (!found) {
            result = URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
        } else {
            snprintf(note, sizeof(note), "updated %s to v%d", ursatz_gui_json_string(source, "name"), latest);
            result = fill_body_uses_locked(id, body, &stored);
        }
    }
    if (result == URSATZ_GUI_MATERIAL_OK) {
        result = commit_version_locked(id, item, stored, note);
    }
    if (result == URSATZ_GUI_MATERIAL_OK && out_item) {
        *out_item = item;
    } else {
        cJSON_Delete(item);
    }
    UNLOCK();
    cJSON_Delete(source);
    cJSON_Delete(body);
    cJSON_Delete(stored);
    return result;
}

/* A set of item ids. */
typedef struct {
    char **ids;
    size_t count;
    size_t capacity;
} IdSet;

static bool id_set_has(const IdSet *set, const char *id) {
    for (size_t i = 0; i < set->count; i++) {
        if (strcmp(set->ids[i], id) == 0) {
            return true;
        }
    }
    return false;
}

static bool id_set_add(IdSet *set, const char *id) {
    if (set->count == set->capacity) {
        size_t capacity = set->capacity ? set->capacity * 2 : 8;
        char **grown = realloc(set->ids, capacity * sizeof(char *));
        if (!grown) {
            return false;
        }
        set->ids = grown;
        set->capacity = capacity;
    }
    set->ids[set->count] = strdup(id);
    if (!set->ids[set->count]) {
        return false;
    }
    set->count++;
    return true;
}

static void id_set_free(IdSet *set) {
    for (size_t i = 0; i < set->count; i++) {
        free(set->ids[i]);
    }
    free(set->ids);
    set->ids = NULL;
    set->count = 0;
    set->capacity = 0;
}

/* Fills out with the ids of the variants of id, recursively (a variant of a variant too), in one scan of the store.
 * id itself is not in the set. */
static UrsatzGuiMaterialResult variant_closure_locked(const char *id, IdSet *out) {
    char dir[PATH_BYTES];
    char **names = NULL;
    size_t count = 0;
    if (snprintf(dir, sizeof(dir), "%s/items", g_dir) >= (int)sizeof(dir) || !list_names(dir, &names, &count)) {
        return URSATZ_GUI_MATERIAL_ERR_FAILED;
    }
    IdSet children = {0};
    IdSet parents = {0};
    UrsatzGuiMaterialResult result = URSATZ_GUI_MATERIAL_OK;
    for (size_t i = 0; result == URSATZ_GUI_MATERIAL_OK && i < count; i++) {
        cJSON *item = NULL;
        cJSON *body = NULL;
        if (load_item_locked(names[i], &item) == URSATZ_GUI_MATERIAL_OK && strcmp(ursatz_gui_json_string(item, "type"), "motif") == 0 &&
            read_version_locked(names[i], current_version_of(item), &body) == URSATZ_GUI_MATERIAL_OK &&
            ursatz_gui_material_item_is_variant(body)) {
            const cJSON *derived = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(body, "motif"), "derived");
            if (!id_set_add(&children, names[i]) || !id_set_add(&parents, ursatz_gui_json_string(derived, "source_item"))) {
                result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
            }
        }
        cJSON_Delete(item);
        cJSON_Delete(body);
    }
    free_names(names, count);
    /* Walk down from id: the descendants found so far are also parents to look for. */
    for (size_t level = 0; result == URSATZ_GUI_MATERIAL_OK && level <= out->count; level++) {
        const char *parent = level == 0 ? id : out->ids[level - 1];
        for (size_t i = 0; result == URSATZ_GUI_MATERIAL_OK && i < children.count; i++) {
            if (strcmp(parents.ids[i], parent) == 0 && strcmp(children.ids[i], id) != 0 && !id_set_has(out, children.ids[i]) &&
                !id_set_add(out, children.ids[i])) {
                result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
            }
        }
    }
    id_set_free(&children);
    id_set_free(&parents);
    return result;
}

/* The items outside the delete set (id and its variants) that use any member of it, as used_by_locked entries once each. */
static UrsatzGuiMaterialResult delete_blockers_locked(const char *id, const IdSet *variants, cJSON **out) {
    cJSON *blockers = cJSON_CreateArray();
    if (!blockers) {
        return URSATZ_GUI_MATERIAL_ERR_ALLOC;
    }
    UrsatzGuiMaterialResult result = URSATZ_GUI_MATERIAL_OK;
    for (size_t member = 0; result == URSATZ_GUI_MATERIAL_OK && member <= variants->count; member++) {
        cJSON *users = NULL;
        result = used_by_locked(member == 0 ? id : variants->ids[member - 1], &users);
        const cJSON *user = NULL;
        cJSON_ArrayForEach(user, users) {
            const char *user_id = ursatz_gui_json_string(user, "id");
            bool seen = false;
            const cJSON *known = NULL;
            cJSON_ArrayForEach(known, blockers) {
                seen = seen || strcmp(ursatz_gui_json_string(known, "id"), user_id) == 0;
            }
            if (strcmp(user_id, id) == 0 || id_set_has(variants, user_id) || seen) {
                continue;
            }
            cJSON *copy = cJSON_Duplicate(user, 1);
            if (!copy) {
                result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
                break;
            }
            cJSON_AddItemToArray(blockers, copy);
        }
        cJSON_Delete(users);
    }
    if (result == URSATZ_GUI_MATERIAL_OK) {
        *out = blockers;
    } else {
        cJSON_Delete(blockers);
    }
    return result;
}

UrsatzGuiMaterialResult ursatz_gui_material_delete_blockers(const char *id, cJSON **out_users) {
    *out_users = NULL;
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    }
    IdSet variants = {0};
    UrsatzGuiMaterialResult result = variant_closure_locked(id, &variants);
    if (result == URSATZ_GUI_MATERIAL_OK) {
        result = delete_blockers_locked(id, &variants, out_users);
    }
    UNLOCK();
    id_set_free(&variants);
    return result;
}

UrsatzGuiMaterialResult ursatz_gui_material_item_delete(const char *id, bool force) {
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    }
    char dir[PATH_BYTES];
    char meta[PATH_BYTES];
    cJSON *users = NULL;
    IdSet variants = {0};
    IdSet removed = {0};
    UrsatzGuiMaterialResult result = URSATZ_GUI_MATERIAL_OK;
    if (!item_dir_path(id, dir, sizeof(dir)) || !item_file_path(id, "item.json", meta, sizeof(meta))) {
        result = URSATZ_GUI_MATERIAL_ERR_INVALID_ID;
    } else if (!ursatz_gui_path_exists(meta)) {
        result = URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
    } else if ((result = variant_closure_locked(id, &variants)) != URSATZ_GUI_MATERIAL_OK) {
        /* nothing is deleted when the variants cannot be found */
    } else if (!force && (result = delete_blockers_locked(id, &variants, &users)) == URSATZ_GUI_MATERIAL_OK && users->child) {
        result = URSATZ_GUI_MATERIAL_ERR_REFERENCED;
    } else if (result == URSATZ_GUI_MATERIAL_OK) {
        /* The variants go with their motif (R-187), then the motif. item.json first: a half-deleted directory is no
         * longer an item. */
        for (size_t member = 0; result == URSATZ_GUI_MATERIAL_OK && member <= variants.count; member++) {
            const char *member_id = member < variants.count ? variants.ids[member] : id;
            char member_dir[PATH_BYTES];
            char member_meta[PATH_BYTES];
            if (!item_dir_path(member_id, member_dir, sizeof(member_dir)) ||
                !item_file_path(member_id, "item.json", member_meta, sizeof(member_meta)) || remove(member_meta) != 0) {
                result = URSATZ_GUI_MATERIAL_ERR_FAILED;
            } else {
                remove_item_dir(member_dir);
                if (!id_set_add(&removed, member_id)) {
                    result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
                }
            }
        }
    }
    UNLOCK();
    cJSON_Delete(users);
    /* Subjects take their own lock, so this runs with the material lock released. */
    for (size_t i = 0; i < removed.count; i++) {
        ursatz_gui_subject_forget_item(removed.ids[i]);
    }
    id_set_free(&variants);
    id_set_free(&removed);
    return result;
}

/* --- operation marks ----------------------------------------------------- */

#define OP_CHECKS_MAX 64
#define OP_CHECK_COMMENT_MAX 500
#define OP_CHECK_NOTES_MAX 4000

static bool op_name_ok(const char *name) {
    size_t length = strlen(name);
    if (length == 0 || length > 40) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) {
            return false;
        }
    }
    return true;
}

static bool check_date_ok(const char *text) {
    if (strlen(text) != 10) {
        return false;
    }
    for (int i = 0; i < 10; i++) {
        bool digit = text[i] >= '0' && text[i] <= '9';
        if (i == 4 || i == 7 ? text[i] != '-' : !digit) {
            return false;
        }
    }
    int month = (text[5] - '0') * 10 + (text[6] - '0');
    int day = (text[8] - '0') * 10 + (text[9] - '0');
    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

UrsatzGuiMaterialResult ursatz_gui_material_op_checks_validate(const cJSON *body, char *error, size_t error_size) {
    const cJSON *checks = cJSON_GetObjectItemCaseSensitive(body, "checks");
    if (!cJSON_IsObject(body) || !cJSON_IsObject(checks)) {
        return invalid_body(error, error_size, "op-checks is {checks: {<operation>: {...}}}");
    }
    if (cJSON_GetArraySize(checks) > OP_CHECKS_MAX) {
        return invalid_body(error, error_size, "at most 64 operations can be marked");
    }
    const cJSON *check = NULL;
    cJSON_ArrayForEach(check, checks) {
        const cJSON *verdict = cJSON_GetObjectItemCaseSensitive(check, "verdict");
        const cJSON *comment = cJSON_GetObjectItemCaseSensitive(check, "comment");
        const cJSON *seed = cJSON_GetObjectItemCaseSensitive(check, "seed");
        const cJSON *notes = cJSON_GetObjectItemCaseSensitive(check, "notes_text");
        const cJSON *date = cJSON_GetObjectItemCaseSensitive(check, "date");
        if (!check->string || !op_name_ok(check->string)) {
            return invalid_body(error, error_size, "an operation name is lowercase letters, digits and hyphens");
        }
        if (!cJSON_IsObject(check) || !cJSON_IsString(verdict) ||
            (strcmp(verdict->valuestring, "works") != 0 && strcmp(verdict->valuestring, "wrong") != 0)) {
            return invalid_body(error, error_size, "a mark's verdict is works or wrong");
        }
        if (!cJSON_IsString(comment) || strlen(comment->valuestring) > OP_CHECK_COMMENT_MAX) {
            return invalid_body(error, error_size, "a mark's comment is text of at most 500 characters");
        }
        if (!cJSON_IsNumber(seed) || seed->valuedouble < 0 || seed->valuedouble != (double)(long long)seed->valuedouble) {
            return invalid_body(error, error_size, "a mark's seed is a whole number from 0");
        }
        if (!cJSON_IsString(notes) || strlen(notes->valuestring) > OP_CHECK_NOTES_MAX) {
            return invalid_body(error, error_size, "a mark's notes_text is the result checked, as text");
        }
        if (!cJSON_IsString(date) || !check_date_ok(date->valuestring)) {
            return invalid_body(error, error_size, "a mark's date is YYYY-MM-DD");
        }
    }
    return URSATZ_GUI_MATERIAL_OK;
}

/* The path of the marks file of motif id; NOT_FOUND unless id is a motif. */
static UrsatzGuiMaterialResult op_checks_path_locked(const char *id, char *path, size_t path_size) {
    cJSON *item = NULL;
    UrsatzGuiMaterialResult result = load_item_locked(id, &item);
    if (result == URSATZ_GUI_MATERIAL_OK) {
        if (strcmp(ursatz_gui_json_string(item, "type"), "motif") != 0) {
            result = URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
        } else if (!item_file_path(id, "op-checks.json", path, path_size)) {
            result = URSATZ_GUI_MATERIAL_ERR_INVALID_ID;
        }
    }
    cJSON_Delete(item);
    return result;
}

UrsatzGuiMaterialResult ursatz_gui_material_op_checks_get(const char *id, cJSON **out) {
    *out = NULL;
    LOCK();
    char path[PATH_BYTES];
    UrsatzGuiMaterialResult result = g_ready ? op_checks_path_locked(id, path, sizeof(path)) : URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    if (result == URSATZ_GUI_MATERIAL_OK) {
        cJSON *stored = ursatz_gui_read_json_object(path);
        if (stored && ursatz_gui_material_op_checks_validate(stored, NULL, 0) == URSATZ_GUI_MATERIAL_OK) {
            *out = stored;
        } else {
            /* No marks yet, or a file that is not marks: an empty set. */
            cJSON_Delete(stored);
            *out = cJSON_CreateObject();
            if (!*out || !cJSON_AddObjectToObject(*out, "checks")) {
                cJSON_Delete(*out);
                *out = NULL;
                result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
            }
        }
    }
    UNLOCK();
    return result;
}

UrsatzGuiMaterialResult ursatz_gui_material_op_checks_put(const char *id, const cJSON *body, char *error,
                                                           size_t error_size, cJSON **out) {
    *out = NULL;
    UrsatzGuiMaterialResult result = ursatz_gui_material_op_checks_validate(body, error, error_size);
    if (result != URSATZ_GUI_MATERIAL_OK) {
        return result;
    }
    LOCK();
    char path[PATH_BYTES];
    result = g_ready ? op_checks_path_locked(id, path, sizeof(path)) : URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    if (result == URSATZ_GUI_MATERIAL_OK) {
        /* Only the five members are kept, so a stray member never reaches the file. */
        cJSON *stored = cJSON_CreateObject();
        cJSON *checks = stored ? cJSON_AddObjectToObject(stored, "checks") : NULL;
        const cJSON *check = NULL;
        bool ok = checks != NULL;
        cJSON_ArrayForEach(check, cJSON_GetObjectItemCaseSensitive(body, "checks")) {
            cJSON *copy = cJSON_CreateObject();
            ok = ok && copy && cJSON_AddStringToObject(copy, "verdict", ursatz_gui_json_string(check, "verdict")) &&
                 cJSON_AddStringToObject(copy, "comment", ursatz_gui_json_string(check, "comment")) &&
                 cJSON_AddNumberToObject(copy, "seed", cJSON_GetObjectItemCaseSensitive(check, "seed")->valuedouble) &&
                 cJSON_AddStringToObject(copy, "notes_text", ursatz_gui_json_string(check, "notes_text")) &&
                 cJSON_AddStringToObject(copy, "date", ursatz_gui_json_string(check, "date")) &&
                 cJSON_AddItemToObject(checks, check->string, copy);
            if (!ok) {
                cJSON_Delete(copy);
                break;
            }
        }
        if (ok && write_json(path, stored)) {
            *out = stored;
        } else {
            cJSON_Delete(stored);
            result = ok ? URSATZ_GUI_MATERIAL_ERR_FAILED : URSATZ_GUI_MATERIAL_ERR_ALLOC;
        }
    }
    UNLOCK();
    return result;
}

/* --- version files ------------------------------------------------------- */

/* theme-<section id>.plan.json: the copy of a theme plan a piece version keeps. */
static bool is_theme_copy_name(const char *name) {
    static const char suffix[] = ".plan.json";
    size_t length = strlen(name);
    if (strncmp(name, "theme-", 6) != 0 || length <= 6 + sizeof(suffix) - 1 ||
        strcmp(name + length - (sizeof(suffix) - 1), suffix) != 0) {
        return false;
    }
    char id[PIECE_ID_MAX + 2];
    size_t id_length = length - 6 - (sizeof(suffix) - 1);
    if (id_length >= sizeof(id)) {
        return false;
    }
    memcpy(id, name + 6, id_length);
    id[id_length] = '\0';
    return piece_section_id_ok(id);
}

const char *ursatz_gui_material_version_file_type(const char *name) {
    static const struct {
        const char *name;
        const char *type;
    } files[] = {{"request.json", "application/json"},
                 {"brief.style.json", "application/json"},
                 {"plan.json", "application/json"},
                 {"theme.mid", "audio/midi"},
                 {"theme.musicxml", "application/xml"},
                 {"ending-plan.json", "application/json"},
                 {"loop.mid", "audio/midi"},
                 {"loop.musicxml", "application/xml"},
                 {"ending.mid", "audio/midi"},
                 {"ending.musicxml", "application/xml"}};
    for (size_t i = 0; name && i < sizeof(files) / sizeof(files[0]); i++) {
        if (strcmp(name, files[i].name) == 0) {
            return files[i].type;
        }
    }
    return name && is_theme_copy_name(name) ? "application/json" : NULL;
}

bool ursatz_gui_material_version_dir(const char *id, int number, char *out, size_t out_size) {
    LOCK();
    bool ok = g_ready && is_valid_id(id) && number >= 1 &&
              snprintf(out, out_size, "%s/items/%s/v%d", g_dir, id, number) < (int)out_size;
    UNLOCK();
    return ok;
}

bool ursatz_gui_material_render_tmp_dir(char *out, size_t out_size) {
    LOCK();
    bool ok = g_ready && snprintf(out, out_size, "%s/.render-tmp", g_dir) < (int)out_size;
    UNLOCK();
    return ok;
}

UrsatzGuiMaterialResult ursatz_gui_material_version_file(const char *id, int number, const char *name,
                                                          unsigned char **out_data, size_t *out_size) {
    *out_data = NULL;
    *out_size = 0;
    if (!ursatz_gui_material_version_file_type(name)) {
        return URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
    }
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    }
    cJSON *item = NULL;
    UrsatzGuiMaterialResult result = load_item_locked(id, &item);
    char path[PATH_BYTES];
    if (result == URSATZ_GUI_MATERIAL_OK &&
        (number < 1 || number > current_version_of(item) ||
         snprintf(path, sizeof(path), "%s/items/%s/v%d/%s", g_dir, id, number, name) >= (int)sizeof(path))) {
        result = URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
    }
    if (result == URSATZ_GUI_MATERIAL_OK) {
        FILE *file = fopen(path, "rb");
        size_t capacity = 65536;
        size_t length = 0;
        unsigned char *data = file ? malloc(capacity) : NULL;
        while (data) {
            size_t n = fread(data + length, 1, capacity - length, file);
            length += n;
            if (n == 0) {
                break;
            }
            if (length == capacity) {
                unsigned char *grown = realloc(data, capacity * 2);
                if (!grown) {
                    free(data);
                    data = NULL;
                    break;
                }
                data = grown;
                capacity *= 2;
            }
        }
        if (file) {
            fclose(file);
        }
        if (!file) {
            result = URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
        } else if (!data) {
            result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
        } else {
            *out_data = data;
            *out_size = length;
        }
    }
    UNLOCK();
    cJSON_Delete(item);
    return result;
}

/* True when the current version of a motif body is a full motif whose analysis has no tone labels (R-204). */
static bool lacks_tones(const cJSON *version_body) {
  const cJSON *motif = cJSON_GetObjectItemCaseSensitive(version_body, "motif");
  const cJSON *form = cJSON_GetObjectItemCaseSensitive(motif, "form");
  if (!cJSON_IsString(form) || strcmp(form->valuestring, "full") != 0) {
    return false;
  }
  const cJSON *document = cJSON_GetObjectItemCaseSensitive(motif, "analysis");
  const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(document, "analysis");
  return !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(analysis, "tones"));
}

int ursatz_gui_material_migrate_tones(const char *materials_dir) {
  char marker[512];
  if (snprintf(marker, sizeof(marker), "%s/tones-migrated", materials_dir) >= (int)sizeof(marker)) {
    return 0;
  }
  FILE *done = fopen(marker, "r");
  if (done) {
    fclose(done);
    return 0;
  }
  cJSON *items = NULL;
  if (ursatz_gui_material_item_list(NULL, "motif", &items) != URSATZ_GUI_MATERIAL_OK) {
    return 0;
  }
  int migrated = 0;
  const cJSON *listed = NULL;
  cJSON_ArrayForEach(listed, items) {
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(listed, "id");
    cJSON *meta = NULL;
    cJSON *version_body = NULL;
    if (!cJSON_IsString(id) || ursatz_gui_material_item_get(id->valuestring, &meta, &version_body) != URSATZ_GUI_MATERIAL_OK) {
      continue;
    }
    cJSON_Delete(meta);
    if (lacks_tones(version_body)) {
      char analysis_error[URSATZ_GUI_MOTIF_ANALYSIS_ERROR_SIZE];
      char refusal[512] = "";
      char refusal_path[128] = "";
      UrsatzGuiMotifAnalysis analysis = ursatz_gui_motif_analyze_body(version_body, NULL, analysis_error, refusal, sizeof(refusal), refusal_path,
                                                    sizeof(refusal_path));
      if (analysis == URSATZ_GUI_MOTIF_ANALYSIS_DONE) {
        cJSON *item = NULL;
        if (ursatz_gui_material_item_add_version(id->valuestring, version_body, "Re-analyzed", &item) == URSATZ_GUI_MATERIAL_OK) {
          migrated++;
        }
        cJSON_Delete(item);
      }
    }
    cJSON_Delete(version_body);
  }
  cJSON_Delete(items);
  FILE *out = fopen(marker, "w");
  if (out) {
    fputs("tones migrated\n", out);
    fclose(out);
  }
  return migrated;
}

/* True when the current version of a motif body has a motif with no features (R-312). */
static bool lacks_segments(const cJSON *version_body) {
  const cJSON *motif = cJSON_GetObjectItemCaseSensitive(version_body, "motif");
  if (!cJSON_IsObject(motif)) {
    return false;
  }
  const cJSON *features = cJSON_GetObjectItemCaseSensitive(motif, "features");
  return !cJSON_IsArray(features) || cJSON_GetArraySize(features) == 0;
}

int ursatz_gui_material_migrate_segments(const char *materials_dir) {
  char marker[512];
  if (snprintf(marker, sizeof(marker), "%s/segments-migrated", materials_dir) >= (int)sizeof(marker)) {
    return 0;
  }
  FILE *done = fopen(marker, "r");
  if (done) {
    fclose(done);
    return 0;
  }
  cJSON *items = NULL;
  if (ursatz_gui_material_item_list(NULL, "motif", &items) != URSATZ_GUI_MATERIAL_OK) {
    return 0;
  }
  int migrated = 0;
  const cJSON *listed = NULL;
  cJSON_ArrayForEach(listed, items) {
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(listed, "id");
    cJSON *meta = NULL;
    cJSON *version_body = NULL;
    if (!cJSON_IsString(id) || ursatz_gui_material_item_get(id->valuestring, &meta, &version_body) != URSATZ_GUI_MATERIAL_OK) {
      continue;
    }
    cJSON_Delete(meta);
    if (lacks_segments(version_body)) {
      char analysis_error[URSATZ_GUI_MOTIF_ANALYSIS_ERROR_SIZE];
      char refusal[512] = "";
      char refusal_path[128] = "";
      UrsatzGuiMotifAnalysis analysis = ursatz_gui_motif_analyze_body(version_body, NULL, analysis_error, refusal, sizeof(refusal),
                                                    refusal_path, sizeof(refusal_path));
      const cJSON *motif = cJSON_GetObjectItemCaseSensitive(version_body, "motif");
      if (analysis == URSATZ_GUI_MOTIF_ANALYSIS_DONE &&
          cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(motif, "segments_proposed"))) {
        cJSON *item = NULL;
        if (ursatz_gui_material_item_add_version(id->valuestring, version_body, "Segments proposed", &item) ==
            URSATZ_GUI_MATERIAL_OK) {
          migrated++;
        }
        cJSON_Delete(item);
      }
    }
    cJSON_Delete(version_body);
  }
  cJSON_Delete(items);
  FILE *out = fopen(marker, "w");
  if (out) {
    fputs("segments migrated\n", out);
    fclose(out);
  }
  return migrated;
}

int ursatz_gui_material_clear_themes(const char *materials_dir) {
  char marker[512];
  if (snprintf(marker, sizeof(marker), "%s/themes-cleared", materials_dir) >= (int)sizeof(marker)) {
    return 0;
  }
  FILE *done = fopen(marker, "r");
  if (done) {
    fclose(done);
    return 0;
  }
  cJSON *items = NULL;
  if (ursatz_gui_material_item_list(NULL, "theme", &items) != URSATZ_GUI_MATERIAL_OK) {
    fputs("ursatz-gui-backend: the themes could not be listed; they are not cleared\n", stderr);
    return 0;
  }
  int cleared = 0;
  bool failed = false;
  const cJSON *listed = NULL;
  cJSON_ArrayForEach(listed, items) {
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(listed, "id");
    if (cJSON_IsString(id) && ursatz_gui_material_item_delete(id->valuestring, true) == URSATZ_GUI_MATERIAL_OK) {
      cleared++;
    } else {
      failed = true;
      fprintf(stderr, "ursatz-gui-backend: a theme could not be cleared: %s\n", cJSON_IsString(id) ? id->valuestring : "?");
    }
  }
  cJSON_Delete(items);
  if (!failed) {
    FILE *out = fopen(marker, "w");
    if (out) {
      fputs("themes cleared\n", out);
      fclose(out);
    } else {
      fputs("ursatz-gui-backend: the themes-cleared marker could not be written\n", stderr);
    }
  }
  return cleared;
}
