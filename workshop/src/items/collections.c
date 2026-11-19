#include "collections.h"
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

#include "analysis/analysis_service.h"
#include "ids.h"
#include "library_metadata.h"

static const char *kPiecesDir = "pieces";

#define DESCRIPTION_MAX_BYTES 500
#define ID_MAX_BYTES 64
#define ID_MAX_TRIES 8
#define PATH_BYTES 400

static char g_dir[200];
static bool g_ready = false;

static WorkshopLock g_lock = WORKSHOP_LOCK_INIT;
#define LOCK() workshop_lock_acquire(&g_lock)
#define UNLOCK() workshop_lock_release(&g_lock)

/* The helpers below mirror analysis_service.c's file helpers; each store
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
    return workshop_collection_is_valid_id(piece_id) && workshop_analysis_piece_exists(kPiecesDir, piece_id);
}

static bool collection_path(const char *id, char *out, size_t out_size) {
    return workshop_collection_is_valid_id(id) && snprintf(out, out_size, "%s/%s.json", g_dir, id) < (int)out_size;
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
    char *text = NULL;
    common_file_read(path, &text, NULL, NULL);
    if (!text) {
        return common_path_exists(path) ? workshop_fail(URSATZ_ERR_INTERNAL, "collection operation failed") : workshop_fail(URSATZ_ERR_NOT_FOUND, "collection not found");
    }
    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return workshop_fail(URSATZ_ERR_INTERNAL, "collection operation failed");
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
    char *text = ok ? cJSON_Print(root) : NULL;
    cJSON_Delete(root);
    if (!text) {
        return workshop_fail(URSATZ_ERR_ALLOC, "out of memory");
    }
    ok = (common_file_write_atomic(path, text, strlen(text), NULL) == URSATZ_OK);
    free(text);
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
    if (common_dir_list(g_dir, &files, NULL) != URSATZ_OK) {
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

bool ursatz_gui_collection_service_init(const char *dir) {
    if (!dir || strlen(dir) >= sizeof(g_dir)) {
        return false;
    }
    LOCK();
    bool ok = (common_dir_make(dir, NULL) == URSATZ_OK);
    if (ok) {
        strcpy(g_dir, dir);
    }
    g_ready = ok;
    UNLOCK();
    return ok;
}

void workshop_collection_service_shutdown(void) {
    LOCK();
    g_ready = false;
    g_dir[0] = '\0';
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

UrsatzStatus workshop_collection_create_with_id(const char *id, const char *name,
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

UrsatzStatus workshop_collection_create(const char *name, const char *description, char **out_id) {
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

UrsatzStatus workshop_collection_update(const char *id, const char *name, const char *description) {
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

UrsatzStatus workshop_collection_delete(const char *id) {
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
        result = remove(path) == 0 ? URSATZ_OK : workshop_fail(URSATZ_ERR_INTERNAL, "collection operation failed");
    }
    UNLOCK();
    return result;
}

UrsatzStatus workshop_collection_add_pieces(const char *id, const char *const *piece_ids, size_t count,
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

UrsatzStatus workshop_collection_remove_piece(const char *id, const char *piece_id) {
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

UrsatzStatus workshop_collection_remove_piece_everywhere(const char *piece_id) {
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
