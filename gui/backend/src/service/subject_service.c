#include "subject_service.h"

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

#include "collection_service.h"
#include "id_service.h"
#include "material_service.h"

#define NAME_MAX_CHARS 200
#define NOTE_MAX_CHARS 4000
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
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : "";
}

static bool valid_kind(const char *kind) {
    return kind && (strcmp(kind, "character") == 0 || strcmp(kind, "place") == 0 || strcmp(kind, "idea") == 0 ||
                    strcmp(kind, "note") == 0);
}

static bool subject_path(const char *id, char *out, size_t out_size) {
    return is_valid_id(id) && snprintf(out, out_size, "%s/%s.json", g_dir, id) < (int)out_size;
}

static bool set_string_member(cJSON *object, const char *name, const char *value) {
    cJSON_DeleteItemFromObjectCaseSensitive(object, name);
    return cJSON_AddStringToObject(object, name, value) != NULL;
}

static UrsatzGuiSubjectResult normalize_name(const char *text, char **out) {
    bool alloc_failed;
    *out = trim_text(text, NAME_MAX_CHARS, &alloc_failed);
    if (!*out) {
        return alloc_failed ? URSATZ_GUI_SUBJECT_ERR_ALLOC : URSATZ_GUI_SUBJECT_ERR_INVALID_NAME;
    }
    if ((*out)[0] == '\0') {
        free(*out);
        *out = NULL;
        return URSATZ_GUI_SUBJECT_ERR_INVALID_NAME;
    }
    return URSATZ_GUI_SUBJECT_OK;
}

static UrsatzGuiSubjectResult normalize_note(const char *text, char **out) {
    bool alloc_failed;
    *out = trim_text(text, NOTE_MAX_CHARS, &alloc_failed);
    if (!*out) {
        return alloc_failed ? URSATZ_GUI_SUBJECT_ERR_ALLOC : URSATZ_GUI_SUBJECT_ERR_INVALID_NAME;
    }
    return URSATZ_GUI_SUBJECT_OK;
}

/* Loads <id>.json; the file name is the id, a copy inside the file is not trusted. */
static UrsatzGuiSubjectResult load_subject_locked(const char *id, cJSON **out) {
    char path[PATH_BYTES];
    if (!subject_path(id, path, sizeof(path))) {
        return URSATZ_GUI_SUBJECT_ERR_INVALID_ID;
    }
    cJSON *subject = read_json_object(path);
    if (!subject) {
        return path_exists(path) ? URSATZ_GUI_SUBJECT_ERR_FAILED : URSATZ_GUI_SUBJECT_ERR_NOT_FOUND;
    }
    if (!set_string_member(subject, "id", id)) {
        cJSON_Delete(subject);
        return URSATZ_GUI_SUBJECT_ERR_ALLOC;
    }
    *out = subject;
    return URSATZ_GUI_SUBJECT_OK;
}

static UrsatzGuiSubjectResult save_subject_locked(const cJSON *subject) {
    char path[PATH_BYTES];
    if (!subject_path(string_member(subject, "id"), path, sizeof(path))) {
        return URSATZ_GUI_SUBJECT_ERR_INVALID_ID;
    }
    return write_json(path, subject) ? URSATZ_GUI_SUBJECT_OK : URSATZ_GUI_SUBJECT_ERR_FAILED;
}

bool ursatz_gui_subject_service_init(const char *dir) {
    if (!dir || dir[0] == '\0') {
        return false;
    }
    LOCK();
    g_ready = snprintf(g_dir, sizeof(g_dir), "%s", dir) < (int)sizeof(g_dir) && ensure_dir(g_dir);
    bool ok = g_ready;
    UNLOCK();
    return ok;
}

void ursatz_gui_subject_service_shutdown(void) {
    LOCK();
    g_ready = false;
    g_dir[0] = '\0';
    UNLOCK();
}

static UrsatzGuiSubjectResult create_subject_locked(const char *name, const char *kind, const char *note,
                                                    cJSON **out_subject) {
    if (!valid_kind(kind)) {
        return URSATZ_GUI_SUBJECT_ERR_INVALID_KIND;
    }
    char *clean_name = NULL;
    char *clean_note = NULL;
    UrsatzGuiSubjectResult result = normalize_name(name, &clean_name);
    if (result == URSATZ_GUI_SUBJECT_OK) {
        result = normalize_note(note, &clean_note);
    }
    if (result != URSATZ_GUI_SUBJECT_OK) {
        free(clean_name);
        return result;
    }

    char *id = NULL;
    char path[PATH_BYTES];
    for (int attempt = 0; attempt < ID_MAX_TRIES; attempt++) {
        free(id);
        id = ursatz_gui_new_id("sub");
        if (!id) {
            result = URSATZ_GUI_SUBJECT_ERR_ALLOC;
            break;
        }
        if (subject_path(id, path, sizeof(path)) && !path_exists(path)) {
            break;
        }
        free(id);
        id = NULL;
    }
    if (result == URSATZ_GUI_SUBJECT_OK && !id) {
        result = URSATZ_GUI_SUBJECT_ERR_FAILED;
    }

    cJSON *subject = NULL;
    if (result == URSATZ_GUI_SUBJECT_OK) {
        char now[32];
        iso_utc_now(now, sizeof(now));
        subject = cJSON_CreateObject();
        bool ok = subject && cJSON_AddStringToObject(subject, "id", id) &&
                  cJSON_AddStringToObject(subject, "name", clean_name) &&
                  cJSON_AddStringToObject(subject, "kind", kind) && cJSON_AddStringToObject(subject, "note", clean_note) &&
                  cJSON_AddArrayToObject(subject, "items") && cJSON_AddArrayToObject(subject, "pieces") &&
                  cJSON_AddArrayToObject(subject, "related") && cJSON_AddStringToObject(subject, "created_at", now) &&
                  cJSON_AddStringToObject(subject, "updated_at", now);
        result = !ok ? URSATZ_GUI_SUBJECT_ERR_ALLOC : save_subject_locked(subject);
    }
    if (result == URSATZ_GUI_SUBJECT_OK && out_subject) {
        *out_subject = subject;
    } else {
        cJSON_Delete(subject);
    }
    free(id);
    free(clean_name);
    free(clean_note);
    return result;
}

UrsatzGuiSubjectResult ursatz_gui_subject_create(const char *name, const char *kind, const char *note,
                                                 cJSON **out_subject) {
    if (out_subject) {
        *out_subject = NULL;
    }
    LOCK();
    UrsatzGuiSubjectResult result =
        g_ready ? create_subject_locked(name, kind, note, out_subject) : URSATZ_GUI_SUBJECT_ERR_NOT_INITIALIZED;
    UNLOCK();
    return result;
}

/* Adds the length of the array member `name` as a number member `count_name`. */
static bool add_count(cJSON *subject, const char *name, const char *count_name) {
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(subject, name);
    int size = cJSON_IsArray(list) ? cJSON_GetArraySize(list) : 0;
    return cJSON_AddNumberToObject(subject, count_name, size) != NULL;
}

static int compare_subjects(const void *a, const void *b) {
    const cJSON *left = *(const cJSON *const *)a;
    const cJSON *right = *(const cJSON *const *)b;
    int by_name = compare_ci(string_member(left, "name"), string_member(right, "name"));
    return by_name != 0 ? by_name : strcmp(string_member(left, "id"), string_member(right, "id"));
}

UrsatzGuiSubjectResult ursatz_gui_subject_list(cJSON **out_subjects) {
    *out_subjects = NULL;
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_SUBJECT_ERR_NOT_INITIALIZED;
    }

    char **names = NULL;
    size_t count = 0;
    if (!list_names(g_dir, &names, &count)) {
        UNLOCK();
        return URSATZ_GUI_SUBJECT_ERR_FAILED;
    }

    UrsatzGuiSubjectResult result = URSATZ_GUI_SUBJECT_OK;
    cJSON **subjects = count ? calloc(count, sizeof(cJSON *)) : NULL;
    size_t loaded = 0;
    if (count && !subjects) {
        result = URSATZ_GUI_SUBJECT_ERR_ALLOC;
    }
    for (size_t i = 0; result == URSATZ_GUI_SUBJECT_OK && i < count; i++) {
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
        cJSON *subject = NULL;
        if (load_subject_locked(id, &subject) != URSATZ_GUI_SUBJECT_OK) {
            continue;
        }
        if (!add_count(subject, "items", "item_count") || !add_count(subject, "pieces", "piece_count") ||
            !add_count(subject, "related", "related_count")) {
            cJSON_Delete(subject);
            result = URSATZ_GUI_SUBJECT_ERR_ALLOC;
            break;
        }
        subjects[loaded++] = subject;
    }
    free_names(names, count);
    UNLOCK();

    cJSON *array = result == URSATZ_GUI_SUBJECT_OK ? cJSON_CreateArray() : NULL;
    if (result == URSATZ_GUI_SUBJECT_OK && !array) {
        result = URSATZ_GUI_SUBJECT_ERR_ALLOC;
    }
    if (loaded > 1) {
        qsort(subjects, loaded, sizeof(cJSON *), compare_subjects);
    }
    for (size_t i = 0; i < loaded; i++) {
        if (result == URSATZ_GUI_SUBJECT_OK) {
            cJSON_AddItemToArray(array, subjects[i]);
        } else {
            cJSON_Delete(subjects[i]);
        }
    }
    free(subjects);
    if (result == URSATZ_GUI_SUBJECT_OK) {
        *out_subjects = array;
    }
    return result;
}

UrsatzGuiSubjectResult ursatz_gui_subject_get(const char *id, cJSON **out_subject) {
    *out_subject = NULL;
    LOCK();
    UrsatzGuiSubjectResult result =
        g_ready ? load_subject_locked(id, out_subject) : URSATZ_GUI_SUBJECT_ERR_NOT_INITIALIZED;
    UNLOCK();
    return result;
}

UrsatzGuiSubjectResult ursatz_gui_subject_update(const char *id, const char *name, const char *kind, const char *note,
                                                 cJSON **out_subject) {
    if (out_subject) {
        *out_subject = NULL;
    }
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_SUBJECT_ERR_NOT_INITIALIZED;
    }
    cJSON *subject = NULL;
    char *clean_name = NULL;
    char *clean_note = NULL;
    UrsatzGuiSubjectResult result = load_subject_locked(id, &subject);
    if (result == URSATZ_GUI_SUBJECT_OK && kind && !valid_kind(kind)) {
        result = URSATZ_GUI_SUBJECT_ERR_INVALID_KIND;
    }
    if (result == URSATZ_GUI_SUBJECT_OK && name) {
        result = normalize_name(name, &clean_name);
    }
    if (result == URSATZ_GUI_SUBJECT_OK && note) {
        result = normalize_note(note, &clean_note);
    }
    if (result == URSATZ_GUI_SUBJECT_OK) {
        char now[32];
        iso_utc_now(now, sizeof(now));
        bool ok = (!clean_name || set_string_member(subject, "name", clean_name)) &&
                  (!kind || set_string_member(subject, "kind", kind)) &&
                  (!clean_note || set_string_member(subject, "note", clean_note)) &&
                  set_string_member(subject, "updated_at", now);
        result = ok ? save_subject_locked(subject) : URSATZ_GUI_SUBJECT_ERR_ALLOC;
    }
    if (result == URSATZ_GUI_SUBJECT_OK && out_subject) {
        *out_subject = subject;
    } else {
        cJSON_Delete(subject);
    }
    UNLOCK();
    free(clean_name);
    free(clean_note);
    return result;
}

UrsatzGuiSubjectResult ursatz_gui_subject_delete(const char *id) {
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_SUBJECT_ERR_NOT_INITIALIZED;
    }
    char path[PATH_BYTES];
    UrsatzGuiSubjectResult result = URSATZ_GUI_SUBJECT_OK;
    if (!subject_path(id, path, sizeof(path))) {
        result = URSATZ_GUI_SUBJECT_ERR_INVALID_ID;
    } else if (!path_exists(path)) {
        result = URSATZ_GUI_SUBJECT_ERR_NOT_FOUND;
    } else if (remove(path) != 0) {
        result = URSATZ_GUI_SUBJECT_ERR_FAILED;
    }
    UNLOCK();
    return result;
}

/* --- links --------------------------------------------------------------- */

static bool list_has(const cJSON *list, const char *value) {
    const cJSON *entry;
    cJSON_ArrayForEach(entry, list) {
        if (cJSON_IsString(entry) && strcmp(entry->valuestring, value) == 0) {
            return true;
        }
    }
    return false;
}

/* The array member `name`, created when the file lacks it. */
static cJSON *list_member(cJSON *subject, const char *name) {
    cJSON *list = cJSON_GetObjectItemCaseSensitive(subject, name);
    if (cJSON_IsArray(list)) {
        return list;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(subject, name);
    return cJSON_AddArrayToObject(subject, name);
}

static UrsatzGuiSubjectResult touch_subject(cJSON *subject) {
    char now[32];
    iso_utc_now(now, sizeof(now));
    return set_string_member(subject, "updated_at", now) ? URSATZ_GUI_SUBJECT_OK : URSATZ_GUI_SUBJECT_ERR_ALLOC;
}

/* Adds ref to the array member `name`, or removes it when add is false. */
static UrsatzGuiSubjectResult change_link(const char *id, const char *name, const char *ref, bool add,
                                          bool *out_already, cJSON **out_subject) {
    if (out_already) {
        *out_already = false;
    }
    if (out_subject) {
        *out_subject = NULL;
    }
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_SUBJECT_ERR_NOT_INITIALIZED;
    }
    cJSON *subject = NULL;
    UrsatzGuiSubjectResult result = load_subject_locked(id, &subject);
    if (result == URSATZ_GUI_SUBJECT_OK) {
        cJSON *list = list_member(subject, name);
        bool present = list && list_has(list, ref);
        if (!list) {
            result = URSATZ_GUI_SUBJECT_ERR_ALLOC;
        } else if (add && present) {
            if (out_already) {
                *out_already = true;
            }
        } else if (!add && !present) {
            result = URSATZ_GUI_SUBJECT_ERR_NOT_LINKED;
        } else {
            if (add) {
                cJSON *entry = cJSON_CreateString(ref);
                if (!entry) {
                    result = URSATZ_GUI_SUBJECT_ERR_ALLOC;
                } else {
                    cJSON_AddItemToArray(list, entry);
                }
            } else {
                int index = 0;
                const cJSON *entry;
                cJSON_ArrayForEach(entry, list) {
                    if (cJSON_IsString(entry) && strcmp(entry->valuestring, ref) == 0) {
                        break;
                    }
                    index++;
                }
                cJSON_DeleteItemFromArray(list, index);
            }
            if (result == URSATZ_GUI_SUBJECT_OK) {
                result = touch_subject(subject);
            }
            if (result == URSATZ_GUI_SUBJECT_OK) {
                result = save_subject_locked(subject);
            }
        }
    }
    if (result == URSATZ_GUI_SUBJECT_OK && out_subject) {
        *out_subject = subject;
    } else {
        cJSON_Delete(subject);
    }
    UNLOCK();
    return result;
}

/*
 * The material store has its own lock and calls back into this service when
 * an item is deleted, so item existence is checked before this service's lock
 * is taken, never while holding it.
 */
UrsatzGuiSubjectResult ursatz_gui_subject_link_item(const char *id, const char *item_id, bool *out_already,
                                                    cJSON **out_subject) {
    cJSON *meta = NULL;
    UrsatzGuiMaterialResult found = ursatz_gui_material_item_get(item_id, &meta, NULL);
    cJSON_Delete(meta);
    if (found == URSATZ_GUI_MATERIAL_ERR_INVALID_ID) {
        return URSATZ_GUI_SUBJECT_ERR_INVALID_ID;
    }
    if (found != URSATZ_GUI_MATERIAL_OK) {
        return found == URSATZ_GUI_MATERIAL_ERR_NOT_FOUND ? URSATZ_GUI_SUBJECT_ERR_ITEM_NOT_FOUND
                                                          : URSATZ_GUI_SUBJECT_ERR_FAILED;
    }
    return change_link(id, "items", item_id, true, out_already, out_subject);
}

UrsatzGuiSubjectResult ursatz_gui_subject_unlink_item(const char *id, const char *item_id, cJSON **out_subject) {
    if (!is_valid_id(item_id)) {
        return URSATZ_GUI_SUBJECT_ERR_INVALID_ID;
    }
    return change_link(id, "items", item_id, false, NULL, out_subject);
}

UrsatzGuiSubjectResult ursatz_gui_subject_link_piece(const char *id, const char *piece_id, bool *out_already,
                                                     cJSON **out_subject) {
    if (!is_valid_id(piece_id)) {
        return URSATZ_GUI_SUBJECT_ERR_INVALID_ID;
    }
    if (!ursatz_gui_collection_piece_exists(piece_id)) {
        return URSATZ_GUI_SUBJECT_ERR_PIECE_NOT_FOUND;
    }
    return change_link(id, "pieces", piece_id, true, out_already, out_subject);
}

UrsatzGuiSubjectResult ursatz_gui_subject_unlink_piece(const char *id, const char *piece_id, cJSON **out_subject) {
    if (!is_valid_id(piece_id)) {
        return URSATZ_GUI_SUBJECT_ERR_INVALID_ID;
    }
    return change_link(id, "pieces", piece_id, false, NULL, out_subject);
}
