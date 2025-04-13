#include "collection_service.h"

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

#include <cJSON.h>

#include "id_service.h"
#include "piece_metadata.h"

static const char *kPiecesDir = "pieces";

#define DESCRIPTION_MAX_BYTES 500
#define ID_MAX_BYTES 64
#define ID_MAX_TRIES 8
#define PATH_BYTES 400

static char g_dir[200];
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

/* The helpers below mirror analysis_service.c's file helpers; each store
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

bool ursatz_gui_collection_is_valid_id(const char *id) {
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

bool ursatz_gui_collection_piece_exists(const char *piece_id) {
    char path[PATH_BYTES];
    if (!ursatz_gui_collection_is_valid_id(piece_id) ||
        snprintf(path, sizeof(path), "%s/%s/meta.json", kPiecesDir, piece_id) >= (int)sizeof(path)) {
        return false;
    }
    return path_exists(path);
}

static bool collection_path(const char *id, char *out, size_t out_size) {
    return ursatz_gui_collection_is_valid_id(id) && snprintf(out, out_size, "%s/%s.json", g_dir, id) < (int)out_size;
}

/* --- in-memory collection ------------------------------------------------ */

static void collection_destroy_contents(UrsatzGuiCollection *collection) {
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

void ursatz_gui_collection_destroy(UrsatzGuiCollection *collection) {
    if (collection) {
        collection_destroy_contents(collection);
        free(collection);
    }
}

static char *string_member(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return dup_string(cJSON_IsString(item) ? item->valuestring : "");
}

/* Loads <dir>/<id>.json into collection (zeroed by the caller). */
static UrsatzGuiCollectionResult load_collection(const char *id, UrsatzGuiCollection *collection) {
    char path[PATH_BYTES];
    if (!collection_path(id, path, sizeof(path))) {
        return URSATZ_GUI_COLLECTION_ERR_INVALID_ID;
    }
    char *text = read_file(path);
    if (!text) {
        return path_exists(path) ? URSATZ_GUI_COLLECTION_ERR_FAILED : URSATZ_GUI_COLLECTION_ERR_NOT_FOUND;
    }
    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return URSATZ_GUI_COLLECTION_ERR_FAILED;
    }

    /* The file name is the id; a copy of it inside the file is not trusted. */
    collection->id = dup_string(id);
    collection->name = string_member(root, "name");
    collection->description = string_member(root, "description");
    collection->created_at = string_member(root, "created_at");
    collection->updated_at = string_member(root, "updated_at");
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
            if (!cJSON_IsString(item) || !ursatz_gui_collection_is_valid_id(item->valuestring)) {
                continue;
            }
            collection->pieces[collection->piece_count] = dup_string(item->valuestring);
            ok = collection->pieces[collection->piece_count] != NULL;
            if (ok) {
                collection->piece_count++;
            }
        }
    }
    cJSON_Delete(root);

    if (!ok) {
        collection_destroy_contents(collection);
        return URSATZ_GUI_COLLECTION_ERR_ALLOC;
    }
    return URSATZ_GUI_COLLECTION_OK;
}

static UrsatzGuiCollectionResult save_collection(const UrsatzGuiCollection *collection) {
    char path[PATH_BYTES];
    if (!collection_path(collection->id, path, sizeof(path))) {
        return URSATZ_GUI_COLLECTION_ERR_INVALID_ID;
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
        return URSATZ_GUI_COLLECTION_ERR_ALLOC;
    }
    ok = write_file_atomic(path, text, strlen(text));
    free(text);
    return ok ? URSATZ_GUI_COLLECTION_OK : URSATZ_GUI_COLLECTION_ERR_FAILED;
}

static int piece_index(const UrsatzGuiCollection *collection, const char *piece_id) {
    for (size_t i = 0; i < collection->piece_count; i++) {
        if (strcmp(collection->pieces[i], piece_id) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* Appends a copy of piece_id to the collection's pieces. */
static bool append_piece(UrsatzGuiCollection *collection, const char *piece_id) {
    char *copy = dup_string(piece_id);
    char **grown = copy ? realloc(collection->pieces, (collection->piece_count + 1) * sizeof(char *)) : NULL;
    if (!grown) {
        free(copy);
        return false;
    }
    collection->pieces = grown;
    collection->pieces[collection->piece_count++] = copy;
    return true;
}

static void touch(UrsatzGuiCollection *collection) {
    char now[32];
    iso_utc_now(now, sizeof(now));
    char *copy = dup_string(now);
    if (copy) {
        free(collection->updated_at);
        collection->updated_at = copy;
    }
}

/* --- names and descriptions ---------------------------------------------- */

/* A collection name: normalized like a piece metadata value, never empty. */
static UrsatzGuiCollectionResult normalize_name(const char *text, char **out) {
    bool too_long = false;
    char *name = ursatz_gui_meta_normalize_value(text ? text : "", &too_long);
    if (!name) {
        return too_long ? URSATZ_GUI_COLLECTION_ERR_INVALID_NAME : URSATZ_GUI_COLLECTION_ERR_ALLOC;
    }
    if (name[0] == '\0') {
        free(name);
        return URSATZ_GUI_COLLECTION_ERR_INVALID_NAME;
    }
    *out = name;
    return URSATZ_GUI_COLLECTION_OK;
}

/* A description: trimmed, whitespace collapsed to one space, control
 * characters dropped, at most DESCRIPTION_MAX_BYTES; "" means none. */
static UrsatzGuiCollectionResult normalize_description(const char *text, char **out) {
    size_t length = text ? strlen(text) : 0;
    char *description = malloc(length + 1);
    if (!description) {
        return URSATZ_GUI_COLLECTION_ERR_ALLOC;
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
        return URSATZ_GUI_COLLECTION_ERR_INVALID_DESCRIPTION;
    }
    *out = description;
    return URSATZ_GUI_COLLECTION_OK;
}

/* --- directory listing --------------------------------------------------- */

static void free_ids(char **ids, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(ids[i]);
    }
    free(ids);
}

/* "<id>.json" to "<id>" when it names a collection file (not hidden, valid id). */
static bool file_name_to_id(const char *name, char *id, size_t id_size) {
    size_t length = strlen(name);
    if (name[0] == '.' || length <= 5 || strcmp(name + length - 5, ".json") != 0 || length - 5 >= id_size) {
        return false;
    }
    memcpy(id, name, length - 5);
    id[length - 5] = '\0';
    return ursatz_gui_collection_is_valid_id(id);
}

static bool push_id(char ***ids, size_t *count, size_t *capacity, const char *id) {
    if (*count == *capacity) {
        size_t grown_capacity = *capacity == 0 ? 16 : *capacity * 2;
        char **grown = realloc(*ids, grown_capacity * sizeof(char *));
        if (!grown) {
            return false;
        }
        *ids = grown;
        *capacity = grown_capacity;
    }
    (*ids)[*count] = dup_string(id);
    if (!(*ids)[*count]) {
        return false;
    }
    (*count)++;
    return true;
}

/* Every collection id in the store directory, in no particular order. */
static bool list_collection_ids(char ***out_ids, size_t *out_count) {
    char **ids = NULL;
    size_t count = 0;
    size_t capacity = 0;
    char id[ID_MAX_BYTES + 8];
    bool ok = true;

#ifdef _WIN32
    char pattern[PATH_BYTES];
    snprintf(pattern, sizeof(pattern), "%s\\*.json", g_dir);
    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = FindFirstFileA(pattern, &find_data);
    if (find_handle != INVALID_HANDLE_VALUE) {
        do {
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                file_name_to_id(find_data.cFileName, id, sizeof(id))) {
                ok = push_id(&ids, &count, &capacity, id);
            }
        } while (ok && FindNextFileA(find_handle, &find_data) != 0);
        FindClose(find_handle);
    }
#else
    DIR *dir = opendir(g_dir);
    if (!dir) {
        return false;
    }
    struct dirent *entry;
    while (ok && (entry = readdir(dir)) != NULL) {
        if (file_name_to_id(entry->d_name, id, sizeof(id))) {
            ok = push_id(&ids, &count, &capacity, id);
        }
    }
    closedir(dir);
#endif

    if (!ok) {
        free_ids(ids, count);
        return false;
    }
    *out_ids = ids;
    *out_count = count;
    return true;
}

/* --- lifecycle ----------------------------------------------------------- */

bool ursatz_gui_collection_service_init(const char *dir, const char *legacy_db_path) {
    (void)legacy_db_path;
    if (!dir || strlen(dir) >= sizeof(g_dir)) {
        return false;
    }
    LOCK();
    bool ok = path_is_dir(dir) || make_dir(dir);
    if (ok) {
        strcpy(g_dir, dir);
        g_ready = true;
    }
    UNLOCK();
    return ok;
}

void ursatz_gui_collection_service_shutdown(void) {
    LOCK();
    g_ready = false;
    g_dir[0] = '\0';
    UNLOCK();
}

/* --- operations ---------------------------------------------------------- */

/* Builds and stores a new collection; the lock is held. */
static UrsatzGuiCollectionResult create_locked(const char *id, const char *name, const char *description) {
    char path[PATH_BYTES];
    if (!collection_path(id, path, sizeof(path))) {
        return URSATZ_GUI_COLLECTION_ERR_INVALID_ID;
    }
    if (path_exists(path)) {
        return URSATZ_GUI_COLLECTION_ERR_ALREADY_EXISTS;
    }

    UrsatzGuiCollection collection;
    memset(&collection, 0, sizeof(collection));
    UrsatzGuiCollectionResult result = normalize_name(name, &collection.name);
    if (result == URSATZ_GUI_COLLECTION_OK) {
        result = normalize_description(description, &collection.description);
    }
    if (result != URSATZ_GUI_COLLECTION_OK) {
        collection_destroy_contents(&collection);
        return result;
    }

    char now[32];
    iso_utc_now(now, sizeof(now));
    collection.id = dup_string(id);
    collection.created_at = dup_string(now);
    collection.updated_at = dup_string(now);
    if (!collection.id || !collection.created_at || !collection.updated_at) {
        collection_destroy_contents(&collection);
        return URSATZ_GUI_COLLECTION_ERR_ALLOC;
    }
    result = save_collection(&collection);
    collection_destroy_contents(&collection);
    return result;
}

UrsatzGuiCollectionResult ursatz_gui_collection_create_with_id(const char *id, const char *name,
                                                                const char *description) {
    if (!ursatz_gui_collection_is_valid_id(id)) {
        return URSATZ_GUI_COLLECTION_ERR_INVALID_ID;
    }
    LOCK();
    UrsatzGuiCollectionResult result =
        g_ready ? create_locked(id, name, description) : URSATZ_GUI_COLLECTION_ERR_NOT_INITIALIZED;
    UNLOCK();
    return result;
}

UrsatzGuiCollectionResult ursatz_gui_collection_create(const char *name, const char *description, char **out_id) {
    LOCK();
    UrsatzGuiCollectionResult result = URSATZ_GUI_COLLECTION_ERR_NOT_INITIALIZED;
    if (g_ready) {
        result = URSATZ_GUI_COLLECTION_ERR_ALREADY_EXISTS;
        for (int attempt = 0; attempt < ID_MAX_TRIES && result == URSATZ_GUI_COLLECTION_ERR_ALREADY_EXISTS;
             attempt++) {
            char *id = ursatz_gui_new_id("col");
            if (!id) {
                result = URSATZ_GUI_COLLECTION_ERR_ALLOC;
                break;
            }
            result = create_locked(id, name, description);
            if (result == URSATZ_GUI_COLLECTION_OK) {
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
    const UrsatzGuiCollectionEntry *left = a;
    const UrsatzGuiCollectionEntry *right = b;
    const unsigned char *x = (const unsigned char *)left->name;
    const unsigned char *y = (const unsigned char *)right->name;
    while (*x && *y && tolower(*x) == tolower(*y)) {
        x++;
        y++;
    }
    int order = tolower(*x) - tolower(*y);
    return order != 0 ? order : strcmp(left->id, right->id);
}

void ursatz_gui_collection_free_entries(UrsatzGuiCollectionEntry *entries, size_t count) {
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

UrsatzGuiCollectionResult ursatz_gui_collection_list(UrsatzGuiCollectionEntry **out_entries, size_t *out_count) {
    *out_entries = NULL;
    *out_count = 0;
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_COLLECTION_ERR_NOT_INITIALIZED;
    }

    char **ids = NULL;
    size_t id_count = 0;
    if (!list_collection_ids(&ids, &id_count)) {
        UNLOCK();
        return URSATZ_GUI_COLLECTION_ERR_FAILED;
    }

    UrsatzGuiCollectionEntry *entries = id_count > 0 ? calloc(id_count, sizeof(*entries)) : NULL;
    UrsatzGuiCollectionResult result = (id_count > 0 && !entries) ? URSATZ_GUI_COLLECTION_ERR_ALLOC
                                                                   : URSATZ_GUI_COLLECTION_OK;
    size_t count = 0;
    for (size_t i = 0; result == URSATZ_GUI_COLLECTION_OK && i < id_count; i++) {
        UrsatzGuiCollection collection;
        memset(&collection, 0, sizeof(collection));
        UrsatzGuiCollectionResult loaded = load_collection(ids[i], &collection);
        if (loaded == URSATZ_GUI_COLLECTION_ERR_ALLOC) {
            result = loaded;
        } else if (loaded == URSATZ_GUI_COLLECTION_OK) {
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
    free_ids(ids, id_count);

    if (result != URSATZ_GUI_COLLECTION_OK) {
        ursatz_gui_collection_free_entries(entries, count);
        return result;
    }
    if (count == 0) {
        free(entries);
        return URSATZ_GUI_COLLECTION_OK;
    }
    qsort(entries, count, sizeof(*entries), compare_entries);
    *out_entries = entries;
    *out_count = count;
    return URSATZ_GUI_COLLECTION_OK;
}

bool ursatz_gui_collection_exists(const char *id) {
    char path[PATH_BYTES];
    LOCK();
    bool exists = g_ready && collection_path(id, path, sizeof(path)) && path_exists(path);
    UNLOCK();
    return exists;
}

UrsatzGuiCollectionResult ursatz_gui_collection_get(const char *id, UrsatzGuiCollection **out_collection) {
    UrsatzGuiCollection *collection = calloc(1, sizeof(*collection));
    if (!collection) {
        return URSATZ_GUI_COLLECTION_ERR_ALLOC;
    }
    LOCK();
    UrsatzGuiCollectionResult result =
        g_ready ? load_collection(id, collection) : URSATZ_GUI_COLLECTION_ERR_NOT_INITIALIZED;
    UNLOCK();
    if (result != URSATZ_GUI_COLLECTION_OK) {
        free(collection);
        return result;
    }
    *out_collection = collection;
    return URSATZ_GUI_COLLECTION_OK;
}

UrsatzGuiCollectionResult ursatz_gui_collection_update(const char *id, const char *name, const char *description) {
    char *new_name = NULL;
    char *new_description = NULL;
    UrsatzGuiCollectionResult result = URSATZ_GUI_COLLECTION_OK;
    if (name) {
        result = normalize_name(name, &new_name);
    }
    if (result == URSATZ_GUI_COLLECTION_OK && description) {
        result = normalize_description(description, &new_description);
    }
    if (result != URSATZ_GUI_COLLECTION_OK) {
        free(new_name);
        free(new_description);
        return result;
    }

    LOCK();
    UrsatzGuiCollection collection;
    memset(&collection, 0, sizeof(collection));
    result = g_ready ? load_collection(id, &collection) : URSATZ_GUI_COLLECTION_ERR_NOT_INITIALIZED;
    if (result == URSATZ_GUI_COLLECTION_OK) {
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

UrsatzGuiCollectionResult ursatz_gui_collection_delete(const char *id) {
    char path[PATH_BYTES];
    LOCK();
    UrsatzGuiCollectionResult result;
    if (!g_ready) {
        result = URSATZ_GUI_COLLECTION_ERR_NOT_INITIALIZED;
    } else if (!collection_path(id, path, sizeof(path))) {
        result = URSATZ_GUI_COLLECTION_ERR_INVALID_ID;
    } else if (!path_exists(path)) {
        result = URSATZ_GUI_COLLECTION_ERR_NOT_FOUND;
    } else {
        result = remove(path) == 0 ? URSATZ_GUI_COLLECTION_OK : URSATZ_GUI_COLLECTION_ERR_FAILED;
    }
    UNLOCK();
    return result;
}

UrsatzGuiCollectionResult ursatz_gui_collection_add_pieces(const char *id, const char *const *piece_ids, size_t count,
                                                            size_t *added, size_t *already) {
    for (size_t i = 0; i < count; i++) {
        if (!ursatz_gui_collection_is_valid_id(piece_ids[i])) {
            return URSATZ_GUI_COLLECTION_ERR_INVALID_ID;
        }
    }

    LOCK();
    UrsatzGuiCollection collection;
    memset(&collection, 0, sizeof(collection));
    UrsatzGuiCollectionResult result =
        g_ready ? load_collection(id, &collection) : URSATZ_GUI_COLLECTION_ERR_NOT_INITIALIZED;
    if (result == URSATZ_GUI_COLLECTION_OK) {
        size_t added_count = 0;
        size_t already_count = 0;
        for (size_t i = 0; i < count && result == URSATZ_GUI_COLLECTION_OK; i++) {
            if (piece_index(&collection, piece_ids[i]) >= 0) {
                already_count++;
            } else if (append_piece(&collection, piece_ids[i])) {
                added_count++;
            } else {
                result = URSATZ_GUI_COLLECTION_ERR_ALLOC;
            }
        }
        if (result == URSATZ_GUI_COLLECTION_OK && added_count > 0) {
            touch(&collection);
            result = save_collection(&collection);
        }
        if (result == URSATZ_GUI_COLLECTION_OK) {
            *added = added_count;
            *already = already_count;
        }
        collection_destroy_contents(&collection);
    }
    UNLOCK();
    return result;
}

/* Drops piece_id from the loaded collection; false if it was not a member. */
static bool drop_piece(UrsatzGuiCollection *collection, const char *piece_id) {
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

UrsatzGuiCollectionResult ursatz_gui_collection_remove_piece(const char *id, const char *piece_id) {
    if (!ursatz_gui_collection_is_valid_id(piece_id)) {
        return URSATZ_GUI_COLLECTION_ERR_INVALID_ID;
    }
    LOCK();
    UrsatzGuiCollection collection;
    memset(&collection, 0, sizeof(collection));
    UrsatzGuiCollectionResult result =
        g_ready ? load_collection(id, &collection) : URSATZ_GUI_COLLECTION_ERR_NOT_INITIALIZED;
    if (result == URSATZ_GUI_COLLECTION_OK) {
        if (drop_piece(&collection, piece_id)) {
            touch(&collection);
            result = save_collection(&collection);
        } else {
            result = URSATZ_GUI_COLLECTION_ERR_NOT_MEMBER;
        }
        collection_destroy_contents(&collection);
    }
    UNLOCK();
    return result;
}

UrsatzGuiCollectionResult ursatz_gui_collection_remove_piece_everywhere(const char *piece_id) {
    if (!ursatz_gui_collection_is_valid_id(piece_id)) {
        return URSATZ_GUI_COLLECTION_ERR_INVALID_ID;
    }
    LOCK();
    if (!g_ready) {
        UNLOCK();
        return URSATZ_GUI_COLLECTION_ERR_NOT_INITIALIZED;
    }
    char **ids = NULL;
    size_t id_count = 0;
    if (!list_collection_ids(&ids, &id_count)) {
        UNLOCK();
        return URSATZ_GUI_COLLECTION_ERR_FAILED;
    }

    /* Keep going after a failure so one bad file does not leave the piece
     * in every collection after it. */
    UrsatzGuiCollectionResult result = URSATZ_GUI_COLLECTION_OK;
    for (size_t i = 0; i < id_count; i++) {
        UrsatzGuiCollection collection;
        memset(&collection, 0, sizeof(collection));
        UrsatzGuiCollectionResult loaded = load_collection(ids[i], &collection);
        if (loaded != URSATZ_GUI_COLLECTION_OK) {
            result = URSATZ_GUI_COLLECTION_ERR_FAILED;
            continue;
        }
        if (drop_piece(&collection, piece_id)) {
            touch(&collection);
            if (save_collection(&collection) != URSATZ_GUI_COLLECTION_OK) {
                result = URSATZ_GUI_COLLECTION_ERR_FAILED;
            }
        }
        collection_destroy_contents(&collection);
    }
    UNLOCK();
    free_ids(ids, id_count);
    return result;
}
