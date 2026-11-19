#include "subjects.h"
#include "formats.h"
#include "layout.h"
#include "base/last_error.h"
#include "lock.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pthread.h>
#include <unistd.h>

#include <common/files.h>
#include <common/json.h>
#include <common/strings.h>

#include "collections.h"
#include "ids.h"
#include "store.h"
#include "txn.h"

#define NAME_MAX_CHARS 200
#define NOTE_MAX_CHARS 4000
#define ID_MAX_BYTES 64
#define ID_MAX_TRIES 8
#define PATH_BYTES 400

static bool g_ready = false;

/* The transaction the writer of the moment stages into (set by the public writers below, which begin it before the lock of this file). */
static __thread WorkshopTxn *t_txn = NULL;

static WorkshopLock g_lock = WORKSHOP_LOCK_INIT;
#define LOCK() workshop_lock_acquire(&g_lock)
#define UNLOCK() workshop_lock_release(&g_lock)


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

static bool valid_kind(const char *kind) {
    return kind && (strcmp(kind, "character") == 0 || strcmp(kind, "place") == 0 || strcmp(kind, "idea") == 0 ||
                    strcmp(kind, "note") == 0);
}

static bool subject_path(const char *id, char *out, size_t out_size) {
    return is_valid_id(id) && workshop_layout_subject_file(id, out, out_size);
}


static UrsatzStatus normalize_name(const char *text, char **out) {
    bool alloc_failed;
    *out = trim_text(text, NAME_MAX_CHARS, &alloc_failed);
    if (!*out) {
        return alloc_failed ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : workshop_fail(URSATZ_ERR_REQUEST, "invalid name or note");
    }
    if ((*out)[0] == '\0') {
        free(*out);
        *out = NULL;
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid name or note");
    }
    return URSATZ_OK;
}

static UrsatzStatus normalize_note(const char *text, char **out) {
    bool alloc_failed;
    *out = trim_text(text, NOTE_MAX_CHARS, &alloc_failed);
    if (!*out) {
        return alloc_failed ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : workshop_fail(URSATZ_ERR_REQUEST, "invalid name or note");
    }
    return URSATZ_OK;
}

/* Loads <id>.json; the file name is the id, a copy inside the file is not trusted. */
static UrsatzStatus load_subject_locked(const char *id, cJSON **out) {
    char path[PATH_BYTES];
    if (!subject_path(id, path, sizeof(path))) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    cJSON *subject = NULL;
    workshop_format_read(path, WORKSHOP_FORMAT_SUBJECT, true, &subject, NULL);
    if (!subject) {
        return common_path_exists(path) ? workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure") : workshop_fail(URSATZ_ERR_NOT_FOUND, "subject not found");
    }
    if (!common_json_set_string(subject, "id", id)) {
        cJSON_Delete(subject);
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    *out = subject;
    return URSATZ_OK;
}

static UrsatzStatus save_subject_locked(const cJSON *subject) {
    char path[PATH_BYTES];
    if (!subject_path(common_json_string(subject, "id"), path, sizeof(path))) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    cJSON *copy = cJSON_Duplicate(subject, 1);
    if (!copy) {
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    UrsatzStatus written = t_txn ? workshop_txn_stage_json(t_txn, path, WORKSHOP_FORMAT_SUBJECT, copy) : URSATZ_ERR_INTERNAL;
    cJSON_Delete(copy);
    return written == URSATZ_OK ? URSATZ_OK : workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
}

bool workshop_subject_service_init(void) {
    LOCK();
    g_ready = common_dir_make(WORKSHOP_DIR_SUBJECTS, NULL) == URSATZ_OK;
    bool ok = g_ready;
    UNLOCK();
    return ok;
}

void workshop_subject_service_shutdown(void) {
    LOCK();
    g_ready = false;
    UNLOCK();
}

static UrsatzStatus create_subject_locked(const char *name, const char *kind, const char *note,
                                                    cJSON **out_subject) {
    if (!valid_kind(kind)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid kind");
    }
    char *clean_name = NULL;
    char *clean_note = NULL;
    UrsatzStatus result = normalize_name(name, &clean_name);
    if (result == URSATZ_OK) {
        result = normalize_note(note, &clean_note);
    }
    if (result != URSATZ_OK) {
        free(clean_name);
        return result;
    }

    char *id = NULL;
    char path[PATH_BYTES];
    for (int attempt = 0; attempt < ID_MAX_TRIES; attempt++) {
        free(id);
        id = workshop_new_id("sub");
        if (!id) {
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            break;
        }
        if (subject_path(id, path, sizeof(path)) && !common_path_exists(path)) {
            break;
        }
        free(id);
        id = NULL;
    }
    if (result == URSATZ_OK && !id) {
        result = workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }

    cJSON *subject = NULL;
    if (result == URSATZ_OK) {
        char now[32];
        common_utc_now(now);
        subject = cJSON_CreateObject();
        bool ok = subject && cJSON_AddStringToObject(subject, "id", id) &&
                  cJSON_AddStringToObject(subject, "name", clean_name) &&
                  cJSON_AddStringToObject(subject, "kind", kind) && cJSON_AddStringToObject(subject, "note", clean_note) &&
                  cJSON_AddArrayToObject(subject, "items") && cJSON_AddArrayToObject(subject, "pieces") &&
                  cJSON_AddArrayToObject(subject, "related") && cJSON_AddStringToObject(subject, "created_at", now) &&
                  cJSON_AddStringToObject(subject, "updated_at", now);
        result = !ok ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory") : save_subject_locked(subject);
    }
    if (result == URSATZ_OK && out_subject) {
        *out_subject = subject;
    } else {
        cJSON_Delete(subject);
    }
    free(id);
    free(clean_name);
    free(clean_note);
    return result;
}

static UrsatzStatus subject_create_impl(const char *name, const char *kind, const char *note,
                                                 cJSON **out_subject) {
    if (out_subject) {
        *out_subject = NULL;
    }
    LOCK();
    UrsatzStatus result =
        g_ready ? create_subject_locked(name, kind, note, out_subject) : workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
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
    int by_name = common_compare_ci(common_json_string(left, "name"), common_json_string(right, "name"));
    return by_name != 0 ? by_name : strcmp(common_json_string(left, "id"), common_json_string(right, "id"));
}

UrsatzStatus workshop_subject_list(cJSON **out_subjects) {
    *out_subjects = NULL;
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }

    CommonNames listing = {0};
    if (common_dir_list(WORKSHOP_DIR_SUBJECTS, &listing, NULL) != URSATZ_OK) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }
    char **names = listing.items;
    size_t count = listing.count;

    UrsatzStatus result = URSATZ_OK;
    cJSON **subjects = count ? calloc(count, sizeof(cJSON *)) : NULL;
    size_t loaded = 0;
    if (count && !subjects) {
        result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    for (size_t i = 0; result == URSATZ_OK && i < count; i++) {
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
        if (load_subject_locked(id, &subject) != URSATZ_OK) {
            continue;
        }
        if (!add_count(subject, "items", "item_count") || !add_count(subject, "pieces", "piece_count") ||
            !add_count(subject, "related", "related_count")) {
            cJSON_Delete(subject);
            result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            break;
        }
        subjects[loaded++] = subject;
    }
    common_names_free(&listing);
    UNLOCK();

    cJSON *array = result == URSATZ_OK ? cJSON_CreateArray() : NULL;
    if (result == URSATZ_OK && !array) {
        result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    if (loaded > 1) {
        qsort(subjects, loaded, sizeof(cJSON *), compare_subjects);
    }
    for (size_t i = 0; i < loaded; i++) {
        if (result == URSATZ_OK) {
            cJSON_AddItemToArray(array, subjects[i]);
        } else {
            cJSON_Delete(subjects[i]);
        }
    }
    free(subjects);
    if (result == URSATZ_OK) {
        *out_subjects = array;
    }
    return result;
}

UrsatzStatus workshop_subject_get(const char *id, cJSON **out_subject) {
    *out_subject = NULL;
    LOCK();
    UrsatzStatus result =
        g_ready ? load_subject_locked(id, out_subject) : workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    UNLOCK();
    return result;
}

static UrsatzStatus subject_update_impl(const char *id, const char *name, const char *kind, const char *note,
                                                 cJSON **out_subject) {
    if (out_subject) {
        *out_subject = NULL;
    }
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }
    cJSON *subject = NULL;
    char *clean_name = NULL;
    char *clean_note = NULL;
    UrsatzStatus result = load_subject_locked(id, &subject);
    if (result == URSATZ_OK && kind && !valid_kind(kind)) {
        result = workshop_fail(URSATZ_ERR_REQUEST, "invalid kind");
    }
    if (result == URSATZ_OK && name) {
        result = normalize_name(name, &clean_name);
    }
    if (result == URSATZ_OK && note) {
        result = normalize_note(note, &clean_note);
    }
    if (result == URSATZ_OK) {
        char now[32];
        common_utc_now(now);
        bool ok = (!clean_name || common_json_set_string(subject, "name", clean_name)) &&
                  (!kind || common_json_set_string(subject, "kind", kind)) &&
                  (!clean_note || common_json_set_string(subject, "note", clean_note)) &&
                  common_json_set_string(subject, "updated_at", now);
        result = ok ? save_subject_locked(subject) : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    if (result == URSATZ_OK && out_subject) {
        *out_subject = subject;
    } else {
        cJSON_Delete(subject);
    }
    UNLOCK();
    free(clean_name);
    free(clean_note);
    return result;
}

static UrsatzStatus edit_list(cJSON *subject, const char *name, const char *ref, bool add, bool *changed);

/* Removes ref from the array member `name` of every subject that holds it. */
static UrsatzStatus remove_from_all_locked(const char *name, const char *ref) {
    CommonNames listing = {0};
    if (common_dir_list(WORKSHOP_DIR_SUBJECTS, &listing, NULL) != URSATZ_OK) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }
    char **names = listing.items;
    size_t count = listing.count;
    UrsatzStatus result = URSATZ_OK;
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
        if (load_subject_locked(other, &subject) == URSATZ_OK) {
            UrsatzStatus edited = edit_list(subject, name, ref, false, &changed);
            if (edited != URSATZ_OK && edited != URSATZ_ERR_NOT_FOUND) {
                result = edited;
            }
        }
        cJSON_Delete(subject);
    }
    common_names_free(&listing);
    return result;
}

static UrsatzStatus forget(const char *name, const char *ref) {
    if (!is_valid_id(ref)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    LOCK();
    UrsatzStatus result =
        g_ready ? remove_from_all_locked(name, ref) : workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    UNLOCK();
    return result;
}

UrsatzStatus workshop_subject_forget_piece(WorkshopTxn *txn, const char *piece_id) {
    t_txn = txn;
    UrsatzStatus result = forget("pieces", piece_id);
    t_txn = NULL;
    return result;
}

UrsatzStatus workshop_subject_forget_item(WorkshopTxn *txn, const char *item_id) {
    t_txn = txn;
    UrsatzStatus result = forget("items", item_id);
    t_txn = NULL;
    return result;
}

static UrsatzStatus subject_delete_impl(const char *id) {
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }
    char path[PATH_BYTES];
    UrsatzStatus result = URSATZ_OK;
    if (!subject_path(id, path, sizeof(path))) {
        result = workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    } else if (!common_path_exists(path)) {
        result = workshop_fail(URSATZ_ERR_NOT_FOUND, "subject not found");
    } else if (workshop_txn_remove(t_txn, path) != URSATZ_OK) {
        result = workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
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

static UrsatzStatus touch_subject(cJSON *subject) {
    char now[32];
    common_utc_now(now);
    return common_json_set_string(subject, "updated_at", now) ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
}

/* Adds ref to the array member `name` of subject, or removes it when add is false. */
static UrsatzStatus edit_list(cJSON *subject, const char *name, const char *ref, bool add, bool *changed) {
    *changed = false;
    cJSON *list = list_member(subject, name);
    if (!list) {
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    bool present = list_has(list, ref);
    if (add == present) {
        return add ? URSATZ_OK : workshop_fail(URSATZ_ERR_NOT_FOUND, "not linked");
    }
    if (add) {
        cJSON *entry = cJSON_CreateString(ref);
        if (!entry) {
            return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
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
    UrsatzStatus result = touch_subject(subject);
    return result == URSATZ_OK ? save_subject_locked(subject) : result;
}

/* Adds ref to the array member `name`, or removes it when add is false. */
static UrsatzStatus change_link(const char *id, const char *name, const char *ref, bool add,
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
        return workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }
    cJSON *subject = NULL;
    bool changed = false;
    UrsatzStatus result = load_subject_locked(id, &subject);
    if (result == URSATZ_OK) {
        result = edit_list(subject, name, ref, add, &changed);
    }
    if (result == URSATZ_OK && add && !changed && out_already) {
        *out_already = true;
    }
    if (result == URSATZ_OK && out_subject) {
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
static UrsatzStatus subject_link_item_impl(const char *id, const char *item_id, bool *out_already,
                                                    cJSON **out_subject) {
    cJSON *meta = NULL;
    UrsatzStatus found = workshop_material_item_get(item_id, &meta, NULL);
    cJSON_Delete(meta);
    if (found == URSATZ_ERR_REQUEST) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    if (found != URSATZ_OK) {
        return found == URSATZ_ERR_NOT_FOUND ? workshop_fail(URSATZ_ERR_NOT_FOUND, "item not found")
                                                          : workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }
    return change_link(id, "items", item_id, true, out_already, out_subject);
}

static UrsatzStatus subject_unlink_item_impl(const char *id, const char *item_id, cJSON **out_subject) {
    if (!is_valid_id(item_id)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    return change_link(id, "items", item_id, false, NULL, out_subject);
}

static UrsatzStatus subject_link_piece_impl(const char *id, const char *piece_id, bool *out_already,
                                                     cJSON **out_subject) {
    if (!is_valid_id(piece_id)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    if (!workshop_collection_piece_exists(piece_id)) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, "piece not found");
    }
    return change_link(id, "pieces", piece_id, true, out_already, out_subject);
}

static UrsatzStatus subject_unlink_piece_impl(const char *id, const char *piece_id, cJSON **out_subject) {
    if (!is_valid_id(piece_id)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    return change_link(id, "pieces", piece_id, false, NULL, out_subject);
}

/* --- relations ----------------------------------------------------------- */

static UrsatzStatus subject_relate_impl(const char *id, const char *other_id, bool *out_already,
                                                 cJSON **out_subject) {
    if (out_already) {
        *out_already = false;
    }
    if (out_subject) {
        *out_subject = NULL;
    }
    if (!is_valid_id(id) || !is_valid_id(other_id)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    if (strcmp(id, other_id) == 0) {
        return workshop_fail(URSATZ_ERR_REQUEST, "a subject cannot relate to itself");
    }
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }
    cJSON *subject = NULL;
    cJSON *other = NULL;
    bool changed_subject = false;
    bool changed_other = false;
    UrsatzStatus result = load_subject_locked(id, &subject);
    if (result == URSATZ_OK) {
        result = load_subject_locked(other_id, &other);
    }
    if (result == URSATZ_OK) {
        result = edit_list(other, "related", id, true, &changed_other);
    }
    if (result == URSATZ_OK) {
        result = edit_list(subject, "related", other_id, true, &changed_subject);
    }
    if (result == URSATZ_OK && !changed_subject && !changed_other && out_already) {
        *out_already = true;
    }
    if (result == URSATZ_OK && out_subject) {
        *out_subject = subject;
    } else {
        cJSON_Delete(subject);
    }
    cJSON_Delete(other);
    UNLOCK();
    return result;
}

static UrsatzStatus subject_unrelate_impl(const char *id, const char *other_id, cJSON **out_subject) {
    if (out_subject) {
        *out_subject = NULL;
    }
    if (!is_valid_id(id) || !is_valid_id(other_id)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }
    cJSON *subject = NULL;
    cJSON *other = NULL;
    bool changed = false;
    UrsatzStatus result = load_subject_locked(id, &subject);
    if (result == URSATZ_OK) {
        result = edit_list(subject, "related", other_id, false, &changed);
        if (result == URSATZ_ERR_NOT_FOUND) {
            result = workshop_fail(URSATZ_ERR_NOT_FOUND, "not related");
        }
    }
    /* The other side is cleaned even when it is gone or already clean. */
    if (result == URSATZ_OK && load_subject_locked(other_id, &other) == URSATZ_OK) {
        bool other_changed = false;
        edit_list(other, "related", id, false, &other_changed);
    }
    if (result == URSATZ_OK && out_subject) {
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
    bool ok = row && cJSON_AddStringToObject(row, "id", common_json_string(subject, "id")) &&
              cJSON_AddStringToObject(row, "name", common_json_string(subject, "name")) &&
              cJSON_AddNumberToObject(row, "item_count", items) && cJSON_AddNumberToObject(row, "piece_count", pieces) &&
              cJSON_AddNumberToObject(row, "link_count", items + pieces);
    if (!ok) {
        cJSON_Delete(row);
        return NULL;
    }
    return row;
}

UrsatzStatus workshop_subject_groups(cJSON **out_groups) {
    *out_groups = NULL;
    cJSON *subjects = NULL;
    UrsatzStatus result = workshop_subject_list(&subjects);
    if (result != URSATZ_OK) {
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
            if (strcmp(common_json_string(subject, "kind"), kKinds[k]) != 0) {
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
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    *out_groups = groups;
    return URSATZ_OK;
}

/* ---- the subjects that link a piece or an item ---------------------------- */

static cJSON *subjects_for(const char *kind, const char *id) {
  cJSON *rows = NULL;
  cJSON *found = cJSON_CreateArray();
  if (!found || workshop_subject_list(&rows) != URSATZ_OK) {
    cJSON_Delete(found);
    return NULL;
  }
  const cJSON *row = NULL;
  cJSON_ArrayForEach(row, rows) {
    const cJSON *row_id = cJSON_GetObjectItemCaseSensitive(row, "id");
    cJSON *subject = NULL;
    if (!cJSON_IsString(row_id) || workshop_subject_get(row_id->valuestring, &subject) != URSATZ_OK) {
      continue;
    }
    const cJSON *links = cJSON_GetObjectItemCaseSensitive(subject, kind);
    const cJSON *link = NULL;
    bool linked = false;
    cJSON_ArrayForEach(link, links) {
      if (cJSON_IsString(link) && strcmp(link->valuestring, id) == 0) {
        linked = true;
        break;
      }
    }
    cJSON *entry = linked ? cJSON_CreateObject() : NULL;
    if (entry) {
      const cJSON *name = cJSON_GetObjectItemCaseSensitive(subject, "name");
      const cJSON *subject_kind = cJSON_GetObjectItemCaseSensitive(subject, "kind");
      if (!cJSON_AddStringToObject(entry, "id", row_id->valuestring) ||
          !cJSON_AddStringToObject(entry, "name", cJSON_IsString(name) ? name->valuestring : "") ||
          !cJSON_AddStringToObject(entry, "kind", cJSON_IsString(subject_kind) ? subject_kind->valuestring : "")) {
        cJSON_Delete(entry);
        cJSON_Delete(subject);
        cJSON_Delete(rows);
        cJSON_Delete(found);
        return NULL;
      }
      cJSON_AddItemToArray(found, entry);
    }
    cJSON_Delete(subject);
  }
  cJSON_Delete(rows);
  return found;
}

cJSON *workshop_subjects_for_piece(const char *piece_id) {
  return subjects_for("pieces", piece_id);
}

cJSON *workshop_subjects_for_item(const char *item_id) {
  return subjects_for("items", item_id);
}

/* --- the writers: each begins its transaction before the lock of this file and commits it after ---------------------------------------- */

static UrsatzStatus end_txn(WorkshopTxn *txn, UrsatzStatus result) {
    if (result != URSATZ_OK) {
        workshop_txn_abort(txn);
        return result;
    }
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    return workshop_txn_commit(txn, &error) == URSATZ_OK ? URSATZ_OK : workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
}

UrsatzStatus workshop_subject_create(const char *name, const char *kind, const char *note, cJSON **out_subject) {
    WorkshopTxn *txn = workshop_txn_begin(NULL, "subject.create", "michelle", "", 0);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }
    t_txn = txn;
    UrsatzStatus result = subject_create_impl(name, kind, note, out_subject);
    t_txn = NULL;
    result = end_txn(txn, result);
    if (result != URSATZ_OK && out_subject) {
        cJSON_Delete(*out_subject);
        *out_subject = NULL;
    }
    return result;
}

UrsatzStatus workshop_subject_update(const char *id, const char *name, const char *kind, const char *note, cJSON **out_subject) {
    WorkshopTxn *txn = workshop_txn_begin(NULL, "subject.update", "michelle", id, 0);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }
    t_txn = txn;
    UrsatzStatus result = subject_update_impl(id, name, kind, note, out_subject);
    t_txn = NULL;
    result = end_txn(txn, result);
    if (result != URSATZ_OK && out_subject) {
        cJSON_Delete(*out_subject);
        *out_subject = NULL;
    }
    return result;
}

UrsatzStatus workshop_subject_delete(const char *id) {
    WorkshopTxn *txn = workshop_txn_begin(NULL, "subject.delete", "michelle", id, 0);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }
    t_txn = txn;
    UrsatzStatus result = subject_delete_impl(id);
    t_txn = NULL;
    result = end_txn(txn, result);
    return result;
}

UrsatzStatus workshop_subject_link_item(const char *id, const char *item_id, bool *out_already, cJSON **out_subject) {
    WorkshopTxn *txn = workshop_txn_begin(NULL, "subject.link-item", "michelle", id, 0);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }
    t_txn = txn;
    UrsatzStatus result = subject_link_item_impl(id, item_id, out_already, out_subject);
    t_txn = NULL;
    result = end_txn(txn, result);
    if (result != URSATZ_OK && out_subject) {
        cJSON_Delete(*out_subject);
        *out_subject = NULL;
    }
    return result;
}

UrsatzStatus workshop_subject_unlink_item(const char *id, const char *item_id, cJSON **out_subject) {
    WorkshopTxn *txn = workshop_txn_begin(NULL, "subject.unlink-item", "michelle", id, 0);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }
    t_txn = txn;
    UrsatzStatus result = subject_unlink_item_impl(id, item_id, out_subject);
    t_txn = NULL;
    result = end_txn(txn, result);
    if (result != URSATZ_OK && out_subject) {
        cJSON_Delete(*out_subject);
        *out_subject = NULL;
    }
    return result;
}

UrsatzStatus workshop_subject_link_piece(const char *id, const char *piece_id, bool *out_already, cJSON **out_subject) {
    WorkshopTxn *txn = workshop_txn_begin(NULL, "subject.link-piece", "michelle", id, 0);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }
    t_txn = txn;
    UrsatzStatus result = subject_link_piece_impl(id, piece_id, out_already, out_subject);
    t_txn = NULL;
    result = end_txn(txn, result);
    if (result != URSATZ_OK && out_subject) {
        cJSON_Delete(*out_subject);
        *out_subject = NULL;
    }
    return result;
}

UrsatzStatus workshop_subject_unlink_piece(const char *id, const char *piece_id, cJSON **out_subject) {
    WorkshopTxn *txn = workshop_txn_begin(NULL, "subject.unlink-piece", "michelle", id, 0);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }
    t_txn = txn;
    UrsatzStatus result = subject_unlink_piece_impl(id, piece_id, out_subject);
    t_txn = NULL;
    result = end_txn(txn, result);
    if (result != URSATZ_OK && out_subject) {
        cJSON_Delete(*out_subject);
        *out_subject = NULL;
    }
    return result;
}

UrsatzStatus workshop_subject_relate(const char *id, const char *other_id, bool *out_already, cJSON **out_subject) {
    WorkshopTxn *txn = workshop_txn_begin(NULL, "subject.relate", "michelle", id, 0);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }
    t_txn = txn;
    UrsatzStatus result = subject_relate_impl(id, other_id, out_already, out_subject);
    t_txn = NULL;
    result = end_txn(txn, result);
    if (result != URSATZ_OK && out_subject) {
        cJSON_Delete(*out_subject);
        *out_subject = NULL;
    }
    return result;
}

UrsatzStatus workshop_subject_unrelate(const char *id, const char *other_id, cJSON **out_subject) {
    WorkshopTxn *txn = workshop_txn_begin(NULL, "subject.unrelate", "michelle", id, 0);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "subject store failure");
    }
    t_txn = txn;
    UrsatzStatus result = subject_unrelate_impl(id, other_id, out_subject);
    t_txn = NULL;
    result = end_txn(txn, result);
    if (result != URSATZ_OK && out_subject) {
        cJSON_Delete(*out_subject);
        *out_subject = NULL;
    }
    return result;
}
