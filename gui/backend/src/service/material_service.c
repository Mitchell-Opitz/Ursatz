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

#define NAME_MAX_CHARS 200
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
