#include "subject_service.h"
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


static bool valid_kind(const char *kind) {
    return kind && (strcmp(kind, "character") == 0 || strcmp(kind, "place") == 0 || strcmp(kind, "idea") == 0 ||
                    strcmp(kind, "note") == 0);
}

static bool subject_path(const char *id, char *out, size_t out_size) {
    return is_valid_id(id) && snprintf(out, out_size, "%s/%s.json", g_dir, id) < (int)out_size;
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
    cJSON *subject = ursatz_gui_read_json_object(path);
    if (!subject) {
        return ursatz_gui_path_exists(path) ? URSATZ_GUI_SUBJECT_ERR_FAILED : URSATZ_GUI_SUBJECT_ERR_NOT_FOUND;
    }
    if (!ursatz_gui_json_set_string(subject, "id", id)) {
        cJSON_Delete(subject);
        return URSATZ_GUI_SUBJECT_ERR_ALLOC;
    }
    *out = subject;
    return URSATZ_GUI_SUBJECT_OK;
}

static UrsatzGuiSubjectResult save_subject_locked(const cJSON *subject) {
    char path[PATH_BYTES];
    if (!subject_path(ursatz_gui_json_string(subject, "id"), path, sizeof(path))) {
        return URSATZ_GUI_SUBJECT_ERR_INVALID_ID;
    }
    return write_json(path, subject) ? URSATZ_GUI_SUBJECT_OK : URSATZ_GUI_SUBJECT_ERR_FAILED;
}

bool ursatz_gui_subject_service_init(const char *dir) {
    if (!dir || dir[0] == '\0') {
        return false;
    }
    LOCK();
    g_ready = snprintf(g_dir, sizeof(g_dir), "%s", dir) < (int)sizeof(g_dir) && ursatz_gui_ensure_dir(g_dir);
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
        if (subject_path(id, path, sizeof(path)) && !ursatz_gui_path_exists(path)) {
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
        ursatz_gui_iso_utc_now(now, sizeof(now));
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
    int by_name = compare_ci(ursatz_gui_json_string(left, "name"), ursatz_gui_json_string(right, "name"));
    return by_name != 0 ? by_name : strcmp(ursatz_gui_json_string(left, "id"), ursatz_gui_json_string(right, "id"));
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
        ursatz_gui_iso_utc_now(now, sizeof(now));
        bool ok = (!clean_name || ursatz_gui_json_set_string(subject, "name", clean_name)) &&
                  (!kind || ursatz_gui_json_set_string(subject, "kind", kind)) &&
                  (!clean_note || ursatz_gui_json_set_string(subject, "note", clean_note)) &&
                  ursatz_gui_json_set_string(subject, "updated_at", now);
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

static UrsatzGuiSubjectResult edit_list(cJSON *subject, const char *name, const char *ref, bool add, bool *changed);

/* Removes ref from the array member `name` of every subject that holds it. */
static UrsatzGuiSubjectResult remove_from_all_locked(const char *name, const char *ref) {
    char **names = NULL;
    size_t count = 0;
    if (!list_names(g_dir, &names, &count)) {
        return URSATZ_GUI_SUBJECT_ERR_FAILED;
    }
    UrsatzGuiSubjectResult result = URSATZ_GUI_SUBJECT_OK;
    for (size_t i = 0; i < count; i++) {
        if (!has_suffix(names[i], ".json")) {
            continue;
        }
        char other[ID_MAX_BYTES + 1];
        size_t other_len = strlen(names[i]) - strlen(".json");
        if (other_len > ID_MAX_BYTES) {
            continue;
        }
        memcpy(other, names[i], other_len);
        other[other_len] = '\0';
        cJSON *subject = NULL;
        bool changed = false;
        if (load_subject_locked(other, &subject) == URSATZ_GUI_SUBJECT_OK) {
            UrsatzGuiSubjectResult edited = edit_list(subject, name, ref, false, &changed);
            if (edited != URSATZ_GUI_SUBJECT_OK && edited != URSATZ_GUI_SUBJECT_ERR_NOT_LINKED) {
                result = edited;
            }
        }
        cJSON_Delete(subject);
    }
    free_names(names, count);
    return result;
}

static UrsatzGuiSubjectResult forget(const char *name, const char *ref) {
    if (!is_valid_id(ref)) {
        return URSATZ_GUI_SUBJECT_ERR_INVALID_ID;
    }
    LOCK();
    UrsatzGuiSubjectResult result =
        g_ready ? remove_from_all_locked(name, ref) : URSATZ_GUI_SUBJECT_ERR_NOT_INITIALIZED;
    UNLOCK();
    return result;
}

UrsatzGuiSubjectResult ursatz_gui_subject_forget_piece(const char *piece_id) {
    return forget("pieces", piece_id);
}

UrsatzGuiSubjectResult ursatz_gui_subject_forget_item(const char *item_id) {
    return forget("items", item_id);
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
    } else if (!ursatz_gui_path_exists(path)) {
        result = URSATZ_GUI_SUBJECT_ERR_NOT_FOUND;
    } else if (remove(path) != 0) {
        result = URSATZ_GUI_SUBJECT_ERR_FAILED;
    } else {
        remove_from_all_locked("related", id);
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
    ursatz_gui_iso_utc_now(now, sizeof(now));
    return ursatz_gui_json_set_string(subject, "updated_at", now) ? URSATZ_GUI_SUBJECT_OK : URSATZ_GUI_SUBJECT_ERR_ALLOC;
}

/* Adds ref to the array member `name` of subject, or removes it when add is false. */
static UrsatzGuiSubjectResult edit_list(cJSON *subject, const char *name, const char *ref, bool add, bool *changed) {
    *changed = false;
    cJSON *list = list_member(subject, name);
    if (!list) {
        return URSATZ_GUI_SUBJECT_ERR_ALLOC;
    }
    bool present = list_has(list, ref);
    if (add == present) {
        return add ? URSATZ_GUI_SUBJECT_OK : URSATZ_GUI_SUBJECT_ERR_NOT_LINKED;
    }
    if (add) {
        cJSON *entry = cJSON_CreateString(ref);
        if (!entry) {
            return URSATZ_GUI_SUBJECT_ERR_ALLOC;
        }
        cJSON_AddItemToArray(list, entry);
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
    *changed = true;
    UrsatzGuiSubjectResult result = touch_subject(subject);
    return result == URSATZ_GUI_SUBJECT_OK ? save_subject_locked(subject) : result;
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
    bool changed = false;
    UrsatzGuiSubjectResult result = load_subject_locked(id, &subject);
    if (result == URSATZ_GUI_SUBJECT_OK) {
        result = edit_list(subject, name, ref, add, &changed);
    }
    if (result == URSATZ_GUI_SUBJECT_OK && add && !changed && out_already) {
        *out_already = true;
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

/* --- relations ----------------------------------------------------------- */

UrsatzGuiSubjectResult ursatz_gui_subject_relate(const char *id, const char *other_id, bool *out_already,
                                                 cJSON **out_subject) {
    if (out_already) {
        *out_already = false;
    }
    if (out_subject) {
        *out_subject = NULL;
    }
    if (!is_valid_id(id) || !is_valid_id(other_id)) {
        return URSATZ_GUI_SUBJECT_ERR_INVALID_ID;
    }
    if (strcmp(id, other_id) == 0) {
        return URSATZ_GUI_SUBJECT_ERR_SELF;
    }
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_SUBJECT_ERR_NOT_INITIALIZED;
    }
    cJSON *subject = NULL;
    cJSON *other = NULL;
    bool changed_subject = false;
    bool changed_other = false;
    UrsatzGuiSubjectResult result = load_subject_locked(id, &subject);
    if (result == URSATZ_GUI_SUBJECT_OK) {
        result = load_subject_locked(other_id, &other);
    }
    if (result == URSATZ_GUI_SUBJECT_OK) {
        result = edit_list(other, "related", id, true, &changed_other);
    }
    if (result == URSATZ_GUI_SUBJECT_OK) {
        result = edit_list(subject, "related", other_id, true, &changed_subject);
    }
    if (result == URSATZ_GUI_SUBJECT_OK && !changed_subject && !changed_other && out_already) {
        *out_already = true;
    }
    if (result == URSATZ_GUI_SUBJECT_OK && out_subject) {
        *out_subject = subject;
    } else {
        cJSON_Delete(subject);
    }
    cJSON_Delete(other);
    UNLOCK();
    return result;
}

UrsatzGuiSubjectResult ursatz_gui_subject_unrelate(const char *id, const char *other_id, cJSON **out_subject) {
    if (out_subject) {
        *out_subject = NULL;
    }
    if (!is_valid_id(id) || !is_valid_id(other_id)) {
        return URSATZ_GUI_SUBJECT_ERR_INVALID_ID;
    }
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_SUBJECT_ERR_NOT_INITIALIZED;
    }
    cJSON *subject = NULL;
    cJSON *other = NULL;
    bool changed = false;
    UrsatzGuiSubjectResult result = load_subject_locked(id, &subject);
    if (result == URSATZ_GUI_SUBJECT_OK) {
        result = edit_list(subject, "related", other_id, false, &changed);
        if (result == URSATZ_GUI_SUBJECT_ERR_NOT_LINKED) {
            result = URSATZ_GUI_SUBJECT_ERR_NOT_RELATED;
        }
    }
    /* The other side is cleaned even when it is gone or already clean. */
    if (result == URSATZ_GUI_SUBJECT_OK && load_subject_locked(other_id, &other) == URSATZ_GUI_SUBJECT_OK) {
        bool other_changed = false;
        edit_list(other, "related", id, false, &other_changed);
    }
    if (result == URSATZ_GUI_SUBJECT_OK && out_subject) {
        *out_subject = subject;
    } else {
        cJSON_Delete(subject);
    }
    cJSON_Delete(other);
    UNLOCK();
    return result;
}

/* --- groups -------------------------------------------------------------- */

static const char *const kKinds[] = {"character", "place", "idea", "note"};

/* A panel row for subject: id, name, item_count, piece_count, link_count. */
static cJSON *group_row(const cJSON *subject) {
    const cJSON *item_count = cJSON_GetObjectItemCaseSensitive(subject, "item_count");
    const cJSON *piece_count = cJSON_GetObjectItemCaseSensitive(subject, "piece_count");
    int items = cJSON_IsNumber(item_count) ? item_count->valueint : 0;
    int pieces = cJSON_IsNumber(piece_count) ? piece_count->valueint : 0;
    cJSON *row = cJSON_CreateObject();
    bool ok = row && cJSON_AddStringToObject(row, "id", ursatz_gui_json_string(subject, "id")) &&
              cJSON_AddStringToObject(row, "name", ursatz_gui_json_string(subject, "name")) &&
              cJSON_AddNumberToObject(row, "item_count", items) && cJSON_AddNumberToObject(row, "piece_count", pieces) &&
              cJSON_AddNumberToObject(row, "link_count", items + pieces);
    if (!ok) {
        cJSON_Delete(row);
        return NULL;
    }
    return row;
}

UrsatzGuiSubjectResult ursatz_gui_subject_groups(cJSON **out_groups) {
    *out_groups = NULL;
    cJSON *subjects = NULL;
    UrsatzGuiSubjectResult result = ursatz_gui_subject_list(&subjects);
    if (result != URSATZ_GUI_SUBJECT_OK) {
        return result;
    }
    cJSON *groups = cJSON_CreateArray();
    for (size_t k = 0; groups && k < sizeof(kKinds) / sizeof(kKinds[0]); k++) {
        cJSON *group = cJSON_CreateObject();
        cJSON *rows = group ? cJSON_AddArrayToObject(group, "subjects") : NULL;
        if (!rows || !cJSON_AddStringToObject(group, "kind", kKinds[k])) {
            cJSON_Delete(group);
            cJSON_Delete(groups);
            groups = NULL;
            break;
        }
        const cJSON *subject;
        cJSON_ArrayForEach(subject, subjects) {
            if (strcmp(ursatz_gui_json_string(subject, "kind"), kKinds[k]) != 0) {
                continue;
            }
            cJSON *row = group_row(subject);
            if (!row) {
                cJSON_Delete(group);
                cJSON_Delete(groups);
                groups = NULL;
                break;
            }
            cJSON_AddItemToArray(rows, row);
        }
        if (groups) {
            cJSON_AddItemToArray(groups, group);
        }
    }
    cJSON_Delete(subjects);
    if (!groups) {
        return URSATZ_GUI_SUBJECT_ERR_ALLOC;
    }
    *out_groups = groups;
    return URSATZ_GUI_SUBJECT_OK;
}
