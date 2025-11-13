#include "material_service.h"

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

#include "id_service.h"
#include "subject_service.h"

#define NAME_MAX_CHARS 200
#define NOTE_MAX_CHARS 500
#define DESCRIPTION_MAX_CHARS 2000
#define ID_MAX_BYTES 64
#define ID_MAX_TRIES 8
#define PATH_BYTES 400

static char g_dir[300];
static bool g_ready = false;

#ifdef _WIN32
static SRWLOCK g_lock = SRWLOCK_INIT;
#define LOCK() AcquireSRWLockExclusive(&g_lock)
#define UNLOCK() ReleaseSRWLockExclusive(&g_lock)
#else
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK() pthread_mutex_lock(&g_lock)
#define UNLOCK() pthread_mutex_unlock(&g_lock)
#endif

/* The helpers below mirror collection_service.c's file helpers; each store
 * keeps its own copy so neither depends on the other's internals. */

static char *dup_string(const char *value) {
    size_t len = strlen(value) + 1;
    char *copy = malloc(len);
    if (copy) {
        memcpy(copy, value, len);
    }
    return copy;
}

static void iso_utc_now(char *out, size_t out_size) {
    time_t now = time(NULL);
    struct tm parts;
#ifdef _WIN32
    gmtime_s(&parts, &now);
#else
    gmtime_r(&now, &parts);
#endif
    strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", &parts);
}

static bool path_exists(const char *path) {
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}

static bool path_is_dir(const char *path) {
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

static bool make_dir(const char *path) {
#ifdef _WIN32
    return _mkdir(path) == 0;
#else
    return mkdir(path, 0755) == 0;
#endif
}

/* Creates the directory when it is absent. */
static bool ensure_dir(const char *path) {
    return path_is_dir(path) || make_dir(path) || path_is_dir(path);
}

/* Writes data to "<path>.tmp", then renames it over path. */
static bool write_file_atomic(const char *path, const void *data, size_t size) {
    char tmp_path[PATH_BYTES + 8];
    if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path) >= (int)sizeof(tmp_path)) {
        return false;
    }
    FILE *file = fopen(tmp_path, "wb");
    if (!file) {
        return false;
    }
    bool ok = size == 0 || fwrite(data, 1, size, file) == size;
    ok = (fclose(file) == 0) && ok;
    if (!ok) {
        remove(tmp_path);
        return false;
    }
#ifdef _WIN32
    remove(path);
#endif
    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        return false;
    }
    return true;
}

/* The whole file as a NUL-terminated string (caller frees), or NULL. */
static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    size_t capacity = 4096;
    size_t length = 0;
    char *text = malloc(capacity);
    while (text) {
        size_t n = fread(text + length, 1, capacity - length - 1, file);
        length += n;
        if (n == 0 || length < capacity - 1) {
            break;
        }
        capacity *= 2;
        char *grown = realloc(text, capacity);
        if (!grown) {
            free(text);
            text = NULL;
        } else {
            text = grown;
        }
    }
    fclose(file);
    if (text) {
        text[length] = '\0';
    }
    return text;
}

/* Parses the JSON object in path (caller frees), or NULL when absent or not an object. */
static cJSON *read_json_object(const char *path) {
    char *text = read_file(path);
    if (!text) {
        return NULL;
    }
    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

static bool write_json(const char *path, const cJSON *json) {
    char *text = cJSON_Print(json);
    if (!text) {
        return false;
    }
    bool ok = write_file_atomic(path, text, strlen(text));
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
            names[count] = dup_string(name);
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

static const char *string_member(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
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
        cJSON *meta = read_json_object(path);
        if (meta && strcmp(string_member(meta, "project_id"), project_id) == 0) {
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
    cJSON *project = read_json_object(path);
    if (!project) {
        return path_exists(path) ? URSATZ_GUI_MATERIAL_ERR_FAILED : URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
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
    if (!project_path(string_member(project, "id"), path, sizeof(path))) {
        return URSATZ_GUI_MATERIAL_ERR_INVALID_ID;
    }
    return write_json(path, project) ? URSATZ_GUI_MATERIAL_OK : URSATZ_GUI_MATERIAL_ERR_FAILED;
}

static bool set_string_member(cJSON *object, const char *name, const char *value) {
    cJSON_DeleteItemFromObjectCaseSensitive(object, name);
    return cJSON_AddStringToObject(object, name, value) != NULL;
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
        if (project_path(id, path, sizeof(path)) && !path_exists(path)) {
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
        iso_utc_now(now, sizeof(now));
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
              snprintf(items, sizeof(items), "%s/items", dir) < (int)sizeof(items) && ensure_dir(g_dir) &&
              ensure_dir(projects) && ensure_dir(items);

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
    int by_name = compare_ci(string_member(left, "name"), string_member(right, "name"));
    return by_name != 0 ? by_name : strcmp(string_member(left, "id"), string_member(right, "id"));
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
        iso_utc_now(now, sizeof(now));
        bool ok = (!clean_name || set_string_member(project, "name", clean_name)) &&
                  (!clean_description || set_string_member(project, "description", clean_description)) &&
                  set_string_member(project, "updated_at", now);
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
    } else if (!path_exists(path)) {
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
    return project_path(project_id, path, sizeof(path)) && path_exists(path);
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

static UrsatzGuiMaterialResult validate_theme(const cJSON *theme, char *error, size_t error_size) {
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(theme, "form");
    if (!cJSON_IsObject(theme) || (form && !cJSON_IsNull(form) && !cJSON_IsString(form) && !cJSON_IsObject(form)) ||
        !cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(theme, "slots"))) {
        return invalid_body(error, error_size, "a theme body needs a theme object with form and slots");
    }
    return URSATZ_GUI_MATERIAL_OK;
}

static UrsatzGuiMaterialResult validate_piece(const cJSON *piece, char *error, size_t error_size) {
    const cJSON *structure = cJSON_GetObjectItemCaseSensitive(piece, "structure");
    if (!cJSON_IsObject(piece) || (structure && !cJSON_IsNull(structure) && !cJSON_IsString(structure) &&
                                   !cJSON_IsObject(structure) && !cJSON_IsArray(structure))) {
        return invalid_body(error, error_size, "a piece body needs a piece object with a structure");
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
    cJSON *item = read_json_object(path);
    if (!item) {
        return path_exists(path) ? URSATZ_GUI_MATERIAL_ERR_FAILED : URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
    }
    if (!set_string_member(item, "id", id)) {
        cJSON_Delete(item);
        return URSATZ_GUI_MATERIAL_ERR_ALLOC;
    }
    *out = item;
    return URSATZ_GUI_MATERIAL_OK;
}

static UrsatzGuiMaterialResult save_item_locked(const cJSON *item) {
    char path[PATH_BYTES];
    if (!item_file_path(string_member(item, "id"), "item.json", path, sizeof(path))) {
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
    iso_utc_now(now, sizeof(now));
    return set_string_member(item, "updated_at", now);
}

/* Removes every file in the item directory, then the directory. */
static void remove_item_dir(const char *dir) {
    char **names = NULL;
    size_t count = 0;
    if (list_names(dir, &names, &count)) {
        for (size_t i = 0; i < count; i++) {
            char path[PATH_BYTES];
            if (snprintf(path, sizeof(path), "%s/%s", dir, names[i]) < (int)sizeof(path)) {
                remove(path);
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
    iso_utc_now(now, sizeof(now));
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
    cJSON *body = read_json_object(path);
    if (!body) {
        return path_exists(path) ? URSATZ_GUI_MATERIAL_ERR_FAILED : URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
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
        const char *source_id = string_member(use, "item_id");
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
        result = fill_uses_locked(NULL, body, &stored);
    }

    char *id = NULL;
    char dir[PATH_BYTES];
    for (int attempt = 0; result == URSATZ_GUI_MATERIAL_OK && attempt < ID_MAX_TRIES; attempt++) {
        id = ursatz_gui_new_id(prefix);
        if (!id) {
            result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
        } else if (item_dir_path(id, dir, sizeof(dir)) && !path_exists(dir)) {
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
        iso_utc_now(now, sizeof(now));
        item = cJSON_CreateObject();
        bool ok = item && cJSON_AddStringToObject(item, "id", id) && cJSON_AddStringToObject(item, "type", type) &&
                  cJSON_AddStringToObject(item, "name", clean_name) &&
                  cJSON_AddStringToObject(item, "project_id", project_id) &&
                  cJSON_AddStringToObject(item, "created_at", now) && cJSON_AddStringToObject(item, "updated_at", now) &&
                  append_version_entry(item, 1, clean_note);
        if (!ok) {
            result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
        } else if (!make_dir(dir)) {
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

static int compare_items(const void *a, const void *b) {
    const cJSON *left = *(const cJSON *const *)a;
    const cJSON *right = *(const cJSON *const *)b;
    int by_updated = strcmp(string_member(right, "updated_at"), string_member(left, "updated_at"));
    return by_updated != 0 ? by_updated : strcmp(string_member(left, "id"), string_member(right, "id"));
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
        if ((project_id && strcmp(string_member(item, "project_id"), project_id) != 0) ||
            (type && strcmp(string_member(item, "type"), type) != 0)) {
            cJSON_Delete(item);
            continue;
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
        const char *source_id = string_member(use, "item_id");
        const cJSON *version = cJSON_GetObjectItemCaseSensitive(use, "version");
        int used = cJSON_IsNumber(version) ? version->valueint : 0;
        cJSON *source = NULL;
        cJSON *entry = cJSON_CreateObject();
        bool ok = entry && cJSON_AddStringToObject(entry, "item_id", source_id) &&
                  cJSON_AddNumberToObject(entry, "version", used);
        if (ok && load_item_locked(source_id, &source) == URSATZ_GUI_MATERIAL_OK) {
            int latest = current_version_of(source);
            ok = cJSON_AddNumberToObject(entry, "current_version", latest) &&
                 cJSON_AddStringToObject(entry, "name", string_member(source, "name")) &&
                 cJSON_AddStringToObject(entry, "type", string_member(source, "type")) &&
                 cJSON_AddStringToObject(entry, "project_id", string_member(source, "project_id")) &&
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
                if (strcmp(string_member(use, "item_id"), id) != 0) {
                    continue;
                }
                cJSON *entry = cJSON_CreateObject();
                bool ok = entry && cJSON_AddStringToObject(entry, "id", names[i]) &&
                          cJSON_AddStringToObject(entry, "name", string_member(item, "name")) &&
                          cJSON_AddStringToObject(entry, "type", string_member(item, "type")) &&
                          cJSON_AddStringToObject(entry, "project_id", string_member(item, "project_id")) &&
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
        if (strcmp(string_member(item, "type"), "motif") == 0 &&
            read_version_locked(names[i], current_version_of(item), &body) == URSATZ_GUI_MATERIAL_OK) {
            cJSON *derived = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(body, "motif"), "derived");
            if (cJSON_IsObject(derived)) {
                cJSON *entry = cJSON_CreateObject();
                cJSON *copy = cJSON_Duplicate(derived, 1);
                bool ok = entry && copy && cJSON_AddStringToObject(entry, "id", names[i]) &&
                          cJSON_AddStringToObject(entry, "name", string_member(item, "name")) &&
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
        bool ok = (!clean_name || set_string_member(item, "name", clean_name)) &&
                  (!project_id || set_string_member(item, "project_id", project_id)) && touch_item(item);
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
        result = ursatz_gui_material_body_validate(string_member(item, "type"), body, NULL, 0);
    }
    cJSON *stored = NULL;
    if (result == URSATZ_GUI_MATERIAL_OK) {
        result = fill_uses_locked(id, body, &stored);
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
            if (strcmp(string_member(use, "item_id"), source_id) == 0) {
                found = true;
                cJSON_DeleteItemFromObjectCaseSensitive(use, "version");
                ok = ok && cJSON_AddNumberToObject(use, "version", latest) != NULL;
            }
        }
        if (!ok) {
            result = URSATZ_GUI_MATERIAL_ERR_ALLOC;
        } else if (!found) {
            result = URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
        } else {
            snprintf(note, sizeof(note), "updated %s to v%d", string_member(source, "name"), latest);
            result = fill_uses_locked(id, body, &stored);
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

UrsatzGuiMaterialResult ursatz_gui_material_item_delete(const char *id, bool force) {
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_MATERIAL_ERR_NOT_INITIALIZED;
    }
    char dir[PATH_BYTES];
    char meta[PATH_BYTES];
    cJSON *users = NULL;
    UrsatzGuiMaterialResult result = URSATZ_GUI_MATERIAL_OK;
    if (!item_dir_path(id, dir, sizeof(dir)) || !item_file_path(id, "item.json", meta, sizeof(meta))) {
        result = URSATZ_GUI_MATERIAL_ERR_INVALID_ID;
    } else if (!path_exists(meta)) {
        result = URSATZ_GUI_MATERIAL_ERR_NOT_FOUND;
    } else if (!force && (result = used_by_locked(id, &users)) == URSATZ_GUI_MATERIAL_OK && users->child) {
        result = URSATZ_GUI_MATERIAL_ERR_REFERENCED;
    } else if (result == URSATZ_GUI_MATERIAL_OK) {
        /* item.json first: a half-deleted directory is no longer an item. */
        if (remove(meta) != 0) {
            result = URSATZ_GUI_MATERIAL_ERR_FAILED;
        } else {
            remove_item_dir(dir);
        }
    }
    UNLOCK();
    cJSON_Delete(users);
    /* Subjects take their own lock, so this runs with the material lock released. */
    if (result == URSATZ_GUI_MATERIAL_OK) {
        ursatz_gui_subject_forget_item(id);
    }
    return result;
}
