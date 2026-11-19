#include "collections.h"
#include "formats.h"
#include "txn.h"
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

#include <cJSON.h>

#include <common/files.h>
#include <common/strings.h>
#include <common/json.h>
#include <common/status.h>

#include "library_pieces.h"
#include "ids.h"
#include "library_metadata.h"

static const char *kPiecesDir = WORKSHOP_DIR_PIECES;

#define DESCRIPTION_MAX_BYTES 500
#define ID_MAX_BYTES 64
#define ID_MAX_TRIES 8
#define PATH_BYTES 400

static bool g_ready = false;

/* The transaction the writer of the moment stages into (set by the public writers below, which begin it before the lock of this file). */
static __thread WorkshopTxn *t_txn = NULL;

static WorkshopLock g_lock = WORKSHOP_LOCK_INIT;
#define LOCK() workshop_lock_acquire(&g_lock)
#define UNLOCK() workshop_lock_release(&g_lock)

/* The helpers below mirror analysis.c's file helpers; each store
 * keeps its own copy so neither depends on the other's internals. */


bool workshop_collection_is_valid_id(const char *id) {
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

bool workshop_collection_piece_exists(const char *piece_id) {
    return workshop_collection_is_valid_id(piece_id) && workshop_library_piece_exists(kPiecesDir, piece_id);
}

static bool collection_path(const char *id, char *out, size_t out_size) {
    return workshop_collection_is_valid_id(id) && workshop_layout_collection_file(id, out, out_size);
}

/* --- in-memory collection ------------------------------------------------ */

static void collection_destroy_contents(WorkshopCollection *collection) {
    free(collection->id);
    free(collection->name);
    free(collection->description);
    for (size_t i = 0; i < collection->piece_count; i++) {
        free(collection->pieces[i]);
    }
    free(collection->pieces);
    free(collection->created_at);
    free(collection->updated_at);
    memset(collection, 0, sizeof(*collection));
}

void workshop_collection_destroy(WorkshopCollection *collection) {
    if (collection) {
        collection_destroy_contents(collection);
        free(collection);
    }
}


/* Loads <dir>/<id>.json into collection (zeroed by the caller). */
static UrsatzStatus load_collection(const char *id, WorkshopCollection *collection) {
    char path[PATH_BYTES];
    if (!collection_path(id, path, sizeof(path))) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    cJSON *root = NULL;
    if (workshop_format_read(path, WORKSHOP_FORMAT_COLLECTION, true, &root, NULL) != URSATZ_OK) {
        return common_path_exists(path) ? workshop_fail(URSATZ_ERR_INTERNAL, "collection operation failed") : workshop_fail(URSATZ_ERR_NOT_FOUND, "collection not found");
    }

    /* The file name is the id; a copy of it inside the file is not trusted. */
    collection->id = common_strdup(id);
    collection->name = common_json_string_copy(root, "name");
    collection->description = common_json_string_copy(root, "description");
    collection->created_at = common_json_string_copy(root, "created_at");
    collection->updated_at = common_json_string_copy(root, "updated_at");
    bool ok = collection->id && collection->name && collection->description && collection->created_at &&
              collection->updated_at;

    const cJSON *pieces = cJSON_GetObjectItemCaseSensitive(root, "pieces");
    int length = cJSON_IsArray(pieces) ? cJSON_GetArraySize(pieces) : 0;
    if (ok && length > 0) {
        collection->pieces = calloc((size_t)length, sizeof(char *));
        ok = collection->pieces != NULL;
        const cJSON *item = NULL;
        cJSON_ArrayForEach(item, pieces) {
            if (!ok) {
                break;
            }
            if (!cJSON_IsString(item) || !workshop_collection_is_valid_id(item->valuestring)) {
                continue;
            }
            collection->pieces[collection->piece_count] = common_strdup(item->valuestring);
            ok = collection->pieces[collection->piece_count] != NULL;
            if (ok) {
                collection->piece_count++;
            }
        }
    }
    cJSON_Delete(root);

    if (!ok) {
        collection_destroy_contents(collection);
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    return URSATZ_OK;
}

static UrsatzStatus save_collection(const WorkshopCollection *collection) {
    char path[PATH_BYTES];
    if (!collection_path(collection->id, path, sizeof(path))) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *pieces = root ? cJSON_AddArrayToObject(root, "pieces") : NULL;
    bool ok = pieces && cJSON_AddStringToObject(root, "id", collection->id) &&
              cJSON_AddStringToObject(root, "name", collection->name) &&
              cJSON_AddStringToObject(root, "description", collection->description) &&
              cJSON_AddStringToObject(root, "created_at", collection->created_at) &&
              cJSON_AddStringToObject(root, "updated_at", collection->updated_at);
    for (size_t i = 0; ok && i < collection->piece_count; i++) {
        cJSON *piece = cJSON_CreateString(collection->pieces[i]);
        ok = piece != NULL;
        if (ok) {
            cJSON_AddItemToArray(pieces, piece);
        }
    }
    if (!ok) {
        cJSON_Delete(root);
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    ok = t_txn && workshop_txn_stage_json(t_txn, path, WORKSHOP_FORMAT_COLLECTION, root) == URSATZ_OK;
    cJSON_Delete(root);
    return ok ? URSATZ_OK : workshop_fail(URSATZ_ERR_INTERNAL, "collection operation failed");
}

static int piece_index(const WorkshopCollection *collection, const char *piece_id) {
    for (size_t i = 0; i < collection->piece_count; i++) {
        if (strcmp(collection->pieces[i], piece_id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* Appends a copy of piece_id to the collection's pieces. */
static bool append_piece(WorkshopCollection *collection, const char *piece_id) {
    char *copy = common_strdup(piece_id);
    char **grown = copy ? realloc(collection->pieces, (collection->piece_count + 1) * sizeof(char *)) : NULL;
    if (!grown) {
        free(copy);
        return false;
    }
    collection->pieces = grown;
    collection->pieces[collection->piece_count++] = copy;
    return true;
}

static void touch(WorkshopCollection *collection) {
    char now[32];
    common_utc_now(now);
    char *copy = common_strdup(now);
    if (copy) {
        free(collection->updated_at);
        collection->updated_at = copy;
    }
}

/* --- names and descriptions ---------------------------------------------- */

/* A collection name: normalized like a piece metadata value, never empty. */
static UrsatzStatus normalize_name(const char *text, char **out) {
    bool too_long = false;
    char *name = workshop_meta_normalize_value(text ? text : "", &too_long);
    if (!name) {
        return too_long ? workshop_fail(URSATZ_ERR_REQUEST, "invalid name") : workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    if (name[0] == '\0') {
        free(name);
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid name");
    }
    *out = name;
    return URSATZ_OK;
}

/* A description: trimmed, whitespace collapsed to one space, control
 * characters dropped, at most DESCRIPTION_MAX_BYTES; "" means none. */
static UrsatzStatus normalize_description(const char *text, char **out) {
    size_t length = text ? strlen(text) : 0;
    char *description = malloc(length + 1);
    if (!description) {
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    size_t used = 0;
    bool pending_space = false;
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        if (isspace(c)) {
            pending_space = true;
        } else if (c >= 0x20 && c != 0x7f) {
            if (pending_space && used > 0) {
                description[used++] = ' ';
            }
            pending_space = false;
            description[used++] = (char)c;
        }
    }
    description[used] = '\0';
    if (used > DESCRIPTION_MAX_BYTES) {
        free(description);
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid description");
    }
    *out = description;
    return URSATZ_OK;
}

/* --- directory listing --------------------------------------------------- */

/* "<id>.json" to "<id>" when it names a collection file (not hidden, valid id). */
static bool file_name_to_id(const char *name, char *id, size_t id_size) {
    size_t length = strlen(name);
    if (name[0] == '.' || length <= 5 || strcmp(name + length - 5, ".json") != 0 || length - 5 >= id_size) {
        return false;
    }
    memcpy(id, name, length - 5);
    id[length - 5] = '\0';
    return workshop_collection_is_valid_id(id);
}

/* Every collection id in the store directory, sorted. */
static bool list_collection_ids(CommonNames *ids) {
    CommonNames files = {0};
    if (common_dir_list(WORKSHOP_DIR_COLLECTIONS, &files, NULL) != URSATZ_OK) {
        return false;
    }
    char id[ID_MAX_BYTES + 8];
    bool ok = true;
    for (size_t i = 0; ok && i < files.count; i++) {
        if (file_name_to_id(files.items[i], id, sizeof(id))) {
            ok = common_names_add(ids, id);
        }
    }
    common_names_free(&files);
    if (!ok) {
        common_names_free(ids);
    }
    return ok;
}

/* --- lifecycle ----------------------------------------------------------- */

bool ursatz_gui_collection_service_init(void) {
    LOCK();
    bool ok = (common_dir_make(WORKSHOP_DIR_COLLECTIONS, NULL) == URSATZ_OK);
    g_ready = ok;
    UNLOCK();
    return ok;
}

void workshop_collection_service_shutdown(void) {
    LOCK();
    g_ready = false;
    UNLOCK();
}

/* --- operations ---------------------------------------------------------- */

/* Builds and stores a new collection; the lock is held. */
static UrsatzStatus create_locked(const char *id, const char *name, const char *description) {
    char path[PATH_BYTES];
    if (!collection_path(id, path, sizeof(path))) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    if (common_path_exists(path)) {
        return workshop_fail(URSATZ_ERR_CONFLICT, "collection already exists");
    }

    WorkshopCollection collection;
    memset(&collection, 0, sizeof(collection));
    UrsatzStatus result = normalize_name(name, &collection.name);
    if (result == URSATZ_OK) {
        result = normalize_description(description, &collection.description);
    }
    if (result != URSATZ_OK) {
        collection_destroy_contents(&collection);
        return result;
    }

    char now[32];
    common_utc_now(now);
    collection.id = common_strdup(id);
    collection.created_at = common_strdup(now);
    collection.updated_at = common_strdup(now);
    if (!collection.id || !collection.created_at || !collection.updated_at) {
        collection_destroy_contents(&collection);
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    result = save_collection(&collection);
    collection_destroy_contents(&collection);
    return result;
}

static UrsatzStatus collection_create_with_id_impl(const char *id, const char *name,
                                                                const char *description) {
    if (!workshop_collection_is_valid_id(id)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    LOCK();
    UrsatzStatus result =
        g_ready ? create_locked(id, name, description) : workshop_fail(URSATZ_ERR_INTERNAL, "collection store is not available");
    UNLOCK();
    return result;
}

static UrsatzStatus collection_create_impl(const char *name, const char *description, char **out_id) {
    LOCK();
    UrsatzStatus result = workshop_fail(URSATZ_ERR_INTERNAL, "collection store is not available");
    if (g_ready) {
        result = workshop_fail(URSATZ_ERR_CONFLICT, "collection already exists");
        for (int attempt = 0; attempt < ID_MAX_TRIES && result == URSATZ_ERR_CONFLICT;
             attempt++) {
            char *id = workshop_new_id("col");
            if (!id) {
                result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
                break;
            }
            result = create_locked(id, name, description);
            if (result == URSATZ_OK) {
                *out_id = id;
            } else {
                free(id);
            }
        }
    }
    UNLOCK();
    return result;
}

static int compare_entries(const void *a, const void *b) {
    const WorkshopCollectionEntry *left = a;
    const WorkshopCollectionEntry *right = b;
    const unsigned char *x = (const unsigned char *)left->name;
    const unsigned char *y = (const unsigned char *)right->name;
    while (*x && *y && tolower(*x) == tolower(*y)) {
        x++;
        y++;
    }
    int order = tolower(*x) - tolower(*y);
    return order != 0 ? order : strcmp(left->id, right->id);
}

void workshop_collection_free_entries(WorkshopCollectionEntry *entries, size_t count) {
    if (!entries) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(entries[i].id);
        free(entries[i].name);
        free(entries[i].description);
        free(entries[i].updated_at);
    }
    free(entries);
}

UrsatzStatus workshop_collection_list(WorkshopCollectionEntry **out_entries, size_t *out_count) {
    *out_entries = NULL;
    *out_count = 0;
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "collection store is not available");
    }

    CommonNames ids = {0};
    if (!list_collection_ids(&ids)) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "collection operation failed");
    }

    WorkshopCollectionEntry *entries = ids.count > 0 ? calloc(ids.count, sizeof(*entries)) : NULL;
    UrsatzStatus result = (ids.count > 0 && !entries) ? workshop_fail(URSATZ_ERR_ALLOC, "out of memory")
                                                                   : URSATZ_OK;
    size_t count = 0;
    for (size_t i = 0; result == URSATZ_OK && i < ids.count; i++) {
        WorkshopCollection collection;
        memset(&collection, 0, sizeof(collection));
        UrsatzStatus loaded = load_collection(ids.items[i], &collection);
        if (loaded == URSATZ_ERR_ALLOC) {
            result = loaded;
        } else if (loaded == URSATZ_OK) {
            /* The entry takes over the loaded strings. */
            entries[count].id = collection.id;
            entries[count].name = collection.name;
            entries[count].description = collection.description;
            entries[count].piece_count = collection.piece_count;
            entries[count].updated_at = collection.updated_at;
            collection.id = collection.name = collection.description = collection.updated_at = NULL;
            collection_destroy_contents(&collection);
            count++;
        } /* an unreadable file is skipped, as list skips a piece without a meta.json */
    }
    UNLOCK();
    common_names_free(&ids);

    if (result != URSATZ_OK) {
        workshop_collection_free_entries(entries, count);
        return result;
    }
    if (count == 0) {
        free(entries);
        return URSATZ_OK;
    }
    qsort(entries, count, sizeof(*entries), compare_entries);
    *out_entries = entries;
    *out_count = count;
    return URSATZ_OK;
}

bool workshop_collection_exists(const char *id) {
    char path[PATH_BYTES];
    LOCK();
    bool exists = g_ready && collection_path(id, path, sizeof(path)) && common_path_exists(path);
    UNLOCK();
    return exists;
}

UrsatzStatus workshop_collection_get(const char *id, WorkshopCollection **out_collection) {
    WorkshopCollection *collection = calloc(1, sizeof(*collection));
    if (!collection) {
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    LOCK();
    UrsatzStatus result =
        g_ready ? load_collection(id, collection) : workshop_fail(URSATZ_ERR_INTERNAL, "collection store is not available");
    UNLOCK();
    if (result != URSATZ_OK) {
        free(collection);
        return result;
    }
    *out_collection = collection;
    return URSATZ_OK;
}

static UrsatzStatus collection_update_impl(const char *id, const char *name, const char *description) {
    char *new_name = NULL;
    char *new_description = NULL;
    UrsatzStatus result = URSATZ_OK;
    if (name) {
        result = normalize_name(name, &new_name);
    }
    if (result == URSATZ_OK && description) {
        result = normalize_description(description, &new_description);
    }
    if (result != URSATZ_OK) {
        free(new_name);
        free(new_description);
        return result;
    }

    LOCK();
    WorkshopCollection collection;
    memset(&collection, 0, sizeof(collection));
    result = g_ready ? load_collection(id, &collection) : workshop_fail(URSATZ_ERR_INTERNAL, "collection store is not available");
    if (result == URSATZ_OK) {
        if (new_name) {
            free(collection.name);
            collection.name = new_name;
            new_name = NULL;
        }
        if (new_description) {
            free(collection.description);
            collection.description = new_description;
            new_description = NULL;
        }
        touch(&collection);
        result = save_collection(&collection);
        collection_destroy_contents(&collection);
    }
    UNLOCK();
    free(new_name);
    free(new_description);
    return result;
}

static UrsatzStatus collection_delete_impl(const char *id) {
    char path[PATH_BYTES];
    LOCK();
    UrsatzStatus result;
    if (!g_ready) {
        result = workshop_fail(URSATZ_ERR_INTERNAL, "collection store is not available");
    } else if (!collection_path(id, path, sizeof(path))) {
        result = workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    } else if (!common_path_exists(path)) {
        result = workshop_fail(URSATZ_ERR_NOT_FOUND, "collection not found");
    } else {
        result = workshop_txn_remove(t_txn, path) == URSATZ_OK ? URSATZ_OK : workshop_fail(URSATZ_ERR_INTERNAL, "collection operation failed");
    }
    UNLOCK();
    return result;
}

static UrsatzStatus collection_add_pieces_impl(const char *id, const char *const *piece_ids, size_t count,
                                                            size_t *added, size_t *already) {
    for (size_t i = 0; i < count; i++) {
        if (!workshop_collection_is_valid_id(piece_ids[i])) {
            return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
        }
    }

    LOCK();
    WorkshopCollection collection;
    memset(&collection, 0, sizeof(collection));
    UrsatzStatus result =
        g_ready ? load_collection(id, &collection) : workshop_fail(URSATZ_ERR_INTERNAL, "collection store is not available");
    if (result == URSATZ_OK) {
        size_t added_count = 0;
        size_t already_count = 0;
        for (size_t i = 0; i < count && result == URSATZ_OK; i++) {
            if (piece_index(&collection, piece_ids[i]) >= 0) {
                already_count++;
            } else if (append_piece(&collection, piece_ids[i])) {
                added_count++;
            } else {
                result = workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
            }
        }
        if (result == URSATZ_OK && added_count > 0) {
            touch(&collection);
            result = save_collection(&collection);
        }
        if (result == URSATZ_OK) {
            *added = added_count;
            *already = already_count;
        }
        collection_destroy_contents(&collection);
    }
    UNLOCK();
    return result;
}

/* Drops piece_id from the loaded collection; false if it was not a member. */
static bool drop_piece(WorkshopCollection *collection, const char *piece_id) {
    int index = piece_index(collection, piece_id);
    if (index < 0) {
        return false;
    }
    free(collection->pieces[index]);
    memmove(collection->pieces + index, collection->pieces + index + 1,
            (collection->piece_count - (size_t)index - 1) * sizeof(char *));
    collection->piece_count--;
    return true;
}

static UrsatzStatus collection_remove_piece_impl(const char *id, const char *piece_id) {
    if (!workshop_collection_is_valid_id(piece_id)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    LOCK();
    WorkshopCollection collection;
    memset(&collection, 0, sizeof(collection));
    UrsatzStatus result =
        g_ready ? load_collection(id, &collection) : workshop_fail(URSATZ_ERR_INTERNAL, "collection store is not available");
    if (result == URSATZ_OK) {
        if (drop_piece(&collection, piece_id)) {
            touch(&collection);
            result = save_collection(&collection);
        } else {
            result = workshop_fail(URSATZ_ERR_REQUEST, "piece is not in this collection");
        }
        collection_destroy_contents(&collection);
    }
    UNLOCK();
    return result;
}

static UrsatzStatus remove_piece_everywhere_impl(const char *piece_id) {
    if (!workshop_collection_is_valid_id(piece_id)) {
        return workshop_fail(URSATZ_ERR_REQUEST, "invalid id");
    }
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "collection store is not available");
    }
    CommonNames ids = {0};
    if (!list_collection_ids(&ids)) {
        UNLOCK();
        return workshop_fail(URSATZ_ERR_INTERNAL, "collection operation failed");
    }

    /* Keep going after a failure so one bad file does not leave the piece
     * in every collection after it. */
    UrsatzStatus result = URSATZ_OK;
    for (size_t i = 0; i < ids.count; i++) {
        WorkshopCollection collection;
        memset(&collection, 0, sizeof(collection));
        UrsatzStatus loaded = load_collection(ids.items[i], &collection);
        if (loaded != URSATZ_OK) {
            result = workshop_fail(URSATZ_ERR_INTERNAL, "collection operation failed");
            continue;
        }
        if (drop_piece(&collection, piece_id)) {
            touch(&collection);
            if (save_collection(&collection) != URSATZ_OK) {
                result = workshop_fail(URSATZ_ERR_INTERNAL, "collection operation failed");
            }
        }
        collection_destroy_contents(&collection);
    }
    UNLOCK();
    common_names_free(&ids);
    return result;
}

/* ---- the collection routes' checks (A20261109-47) -------------------------- */

/* The failure of the last store call as error; the message the store left, or the fallback. */
static UrsatzStatus store_failure(UrsatzStatus status, UrsatzError *error) {
    const UrsatzError *last = workshop_last_error();
    return ursatz_error_set(error, status, NULL, "%s", last->message[0] != '\0' ? last->message : "collection operation failed");
}

UrsatzStatus workshop_collection_create_request(const cJSON *body, char **out_id, UrsatzError *error) {
    *out_id = NULL;
    const cJSON *id_item = cJSON_GetObjectItemCaseSensitive(body, "id");
    const cJSON *name_item = cJSON_GetObjectItemCaseSensitive(body, "name");
    const cJSON *description_item = cJSON_GetObjectItemCaseSensitive(body, "description");
    if (!cJSON_IsString(name_item)) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "invalid name");
    }
    if (id_item && !cJSON_IsString(id_item)) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "invalid id");
    }
    if (description_item && !cJSON_IsString(description_item)) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, NULL, "invalid description");
    }
    const char *description = description_item ? description_item->valuestring : "";
    char *id = NULL;
    UrsatzStatus result;
    if (id_item) {
        id = common_strdup(id_item->valuestring);
        if (!id) {
            return ursatz_error_set(error, URSATZ_ERR_ALLOC, NULL, "out of memory");
        }
        result = workshop_collection_create_with_id(id, name_item->valuestring, description);
    } else {
        result = workshop_collection_create(name_item->valuestring, description, &id);
    }
    if (result != URSATZ_OK) {
        free(id);
        return store_failure(result, error);
    }
    *out_id = id;
    return URSATZ_OK;
}

static UrsatzStatus add_refusal(int *http_status, int status, UrsatzStatus kind, UrsatzError *error, const char *message) {
    *http_status = status;
    return ursatz_error_set(error, kind, NULL, "%s", message);
}

UrsatzStatus workshop_collection_add_request(const char *corpus_id, const cJSON *body, size_t *added, size_t *already,
                                             int *http_status, UrsatzError *error) {
    const cJSON *single = cJSON_GetObjectItemCaseSensitive(body, "piece_id");
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(body, "piece_ids");
    size_t count = 0;
    const char **ids = NULL;
    UrsatzStatus result = URSATZ_OK;

    if (cJSON_IsArray(list)) {
        int length = cJSON_GetArraySize(list);
        if (length <= 0) {
            return add_refusal(http_status, 200, URSATZ_ERR_REQUEST, error, "invalid piece_ids");
        }
        if ((size_t)length > WORKSHOP_COLLECTION_MAX_ADD_PIECES) {
            return add_refusal(http_status, 400, URSATZ_ERR_REQUEST, error, "too many pieces");
        }
        ids = calloc((size_t)length, sizeof(*ids));
        if (!ids) {
            return add_refusal(http_status, 200, URSATZ_ERR_ALLOC, error, "out of memory");
        }
        const cJSON *item = NULL;
        cJSON_ArrayForEach(item, list) {
            if (!cJSON_IsString(item) || !workshop_collection_is_valid_id(item->valuestring)) {
                free(ids);
                return add_refusal(http_status, 200, URSATZ_ERR_REQUEST, error, "invalid piece_id");
            }
            ids[count++] = item->valuestring;
        }
    } else if (cJSON_IsString(single) && workshop_collection_is_valid_id(single->valuestring)) {
        ids = calloc(1, sizeof(*ids));
        if (!ids) {
            return add_refusal(http_status, 200, URSATZ_ERR_ALLOC, error, "out of memory");
        }
        ids[count++] = single->valuestring;
    } else {
        return add_refusal(http_status, 200, URSATZ_ERR_REQUEST, error, "invalid piece_id");
    }

    /* An unknown collection is a 404 before any piece is looked at. */
    if (!workshop_collection_exists(corpus_id)) {
        free(ids);
        return add_refusal(http_status, 404, URSATZ_ERR_NOT_FOUND, error, "collection not found");
    }
    for (size_t i = 0; i < count; i++) {
        if (!workshop_collection_piece_exists(ids[i])) {
            char message[128];
            snprintf(message, sizeof(message), "unknown piece: %s", ids[i]);
            free(ids);
            return add_refusal(http_status, 400, URSATZ_ERR_REQUEST, error, message);
        }
    }
    result = workshop_collection_add_pieces(corpus_id, ids, count, added, already);
    free(ids);
    if (result != URSATZ_OK) {
        *http_status = result == URSATZ_ERR_NOT_FOUND ? 404 : 200;
        return store_failure(result, error);
    }
    return URSATZ_OK;
}

/* --- the writers: each begins its transaction before the lock of this file and commits it after ---------------------------------------- */

static UrsatzStatus end_txn(WorkshopTxn *txn, UrsatzStatus result) {
    if (result != URSATZ_OK) {
        workshop_txn_abort(txn);
        return result;
    }
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    return workshop_txn_commit(txn, &error) == URSATZ_OK ? URSATZ_OK : workshop_fail(URSATZ_ERR_INTERNAL, "collection operation failed");
}

UrsatzStatus workshop_collection_remove_piece_everywhere(WorkshopTxn *txn, const char *piece_id) {
    t_txn = txn;
    UrsatzStatus result = remove_piece_everywhere_impl(piece_id);
    t_txn = NULL;
    return result;
}

UrsatzStatus workshop_collection_create_with_id(const char *id, const char *name, const char *description) {
    WorkshopTxn *txn = workshop_txn_begin(NULL, "collection.create", "michelle", id, 0);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "collection operation failed");
    }
    t_txn = txn;
    UrsatzStatus result = collection_create_with_id_impl(id, name, description);
    t_txn = NULL;
    result = end_txn(txn, result);
    return result;
}

UrsatzStatus workshop_collection_create(const char *name, const char *description, char **out_id) {
    WorkshopTxn *txn = workshop_txn_begin(NULL, "collection.create", "michelle", "", 0);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "collection operation failed");
    }
    t_txn = txn;
    UrsatzStatus result = collection_create_impl(name, description, out_id);
    t_txn = NULL;
    result = end_txn(txn, result);
    if (result != URSATZ_OK && out_id) {
        free(*out_id);
        *out_id = NULL;
    }
    return result;
}

UrsatzStatus workshop_collection_update(const char *id, const char *name, const char *description) {
    WorkshopTxn *txn = workshop_txn_begin(NULL, "collection.update", "michelle", id, 0);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "collection operation failed");
    }
    t_txn = txn;
    UrsatzStatus result = collection_update_impl(id, name, description);
    t_txn = NULL;
    result = end_txn(txn, result);
    return result;
}

UrsatzStatus workshop_collection_delete(const char *id) {
    WorkshopTxn *txn = workshop_txn_begin(NULL, "collection.delete", "michelle", id, 0);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "collection operation failed");
    }
    t_txn = txn;
    UrsatzStatus result = collection_delete_impl(id);
    t_txn = NULL;
    result = end_txn(txn, result);
    return result;
}

UrsatzStatus workshop_collection_add_pieces(const char *id, const char *const *piece_ids, size_t count, size_t *added, size_t *already) {
    WorkshopTxn *txn = workshop_txn_begin(NULL, "collection.add-pieces", "michelle", id, 0);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "collection operation failed");
    }
    t_txn = txn;
    UrsatzStatus result = collection_add_pieces_impl(id, piece_ids, count, added, already);
    t_txn = NULL;
    result = end_txn(txn, result);
    return result;
}

UrsatzStatus workshop_collection_remove_piece(const char *id, const char *piece_id) {
    WorkshopTxn *txn = workshop_txn_begin(NULL, "collection.remove-piece", "michelle", id, 0);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_INTERNAL, "collection operation failed");
    }
    t_txn = txn;
    UrsatzStatus result = collection_remove_piece_impl(id, piece_id);
    t_txn = NULL;
    result = end_txn(txn, result);
    return result;
}
