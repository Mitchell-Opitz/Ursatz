#include "style_service.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cJSON.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/style_profile.h>

#include "analysis_service.h"
#include "collection_service.h"
#include "id_service.h"
#include "piece_metadata.h"

static const char *kPiecesDir = "pieces";
static const char *kStylesDir = "styles";

#define PATH_BYTES 400
#define NAME_MAX_BYTES URSATZ_GUI_META_MAX_VALUE_BYTES
#define ID_MAX_TRIES 8

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

static bool make_dir(const char *path) {
#ifdef _WIN32
    return _mkdir(path) == 0;
#else
    return mkdir(path, 0755) == 0;
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

/* Creates the directory when it is missing. */
static bool ensure_dir(const char *path) {
    return path_is_dir(path) || make_dir(path) || path_is_dir(path);
}

/* Renames from over to, replacing it (Windows cannot rename over a file). */
static bool replace_file(const char *from, const char *to) {
#ifdef _WIN32
    remove(to);
#endif
    if (rename(from, to) != 0) {
        remove(from);
        return false;
    }
    return true;
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
    return replace_file(tmp_path, path);
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

/* Parses the JSON object in path; NULL when it is missing or not an object. */
static cJSON *read_json_object(const char *path) {
    char *text = read_file(path);
    if (!text) {
        return NULL;
    }
    cJSON *root = cJSON_Parse(text);
    free(text);
    if (root && !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

/* --- skipped pieces ------------------------------------------------------- */

void ursatz_gui_style_skipped_free(UrsatzGuiStyleSkipped *skipped, size_t count) {
    for (size_t i = 0; skipped && i < count; i++) {
        free(skipped[i].id);
        free(skipped[i].reason);
    }
    free(skipped);
}

typedef struct {
    UrsatzGuiStyleSkipped *items;
    size_t count;
    size_t capacity;
} SkippedList;

static bool skipped_add(SkippedList *list, const char *id, const char *reason) {
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 8;
        UrsatzGuiStyleSkipped *grown = realloc(list->items, capacity * sizeof(*grown));
        if (!grown) {
            return false;
        }
        list->items = grown;
        list->capacity = capacity;
    }
    list->items[list->count].id = dup_string(id);
    list->items[list->count].reason = dup_string(reason);
    if (!list->items[list->count].id || !list->items[list->count].reason) {
        free(list->items[list->count].id);
        free(list->items[list->count].reason);
        return false;
    }
    list->count++;
    return true;
}

/* --- id lists ------------------------------------------------------------- */

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} IdList;

static void id_list_free(IdList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i]);
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static bool id_list_contains(const IdList *list, const char *id) {
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->items[i], id) == 0) {
            return true;
        }
    }
    return false;
}

static bool id_list_add(IdList *list, const char *id) {
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 16;
        char **grown = realloc(list->items, capacity * sizeof(*grown));
        if (!grown) {
            return false;
        }
        list->items = grown;
        list->capacity = capacity;
    }
    list->items[list->count] = dup_string(id);
    if (!list->items[list->count]) {
        return false;
    }
    list->count++;
    return true;
}

/* --- names ---------------------------------------------------------------- */

/* Cuts text to at most max_bytes, never inside a UTF-8 sequence. */
static void clip_utf8(char *text, size_t max_bytes) {
    size_t length = strlen(text);
    if (length <= max_bytes) {
        return;
    }
    size_t cut = max_bytes;
    while (cut > 0 && ((unsigned char)text[cut] & 0xC0) == 0x80) {
        cut--;
    }
    text[cut] = '\0';
}

/* --- the source of a style ------------------------------------------------ */

static const char *source_kind_label(UrsatzGuiStyleSourceKind kind) {
    switch (kind) {
        case URSATZ_GUI_STYLE_SOURCE_FILTER:
            return "filter";
        case URSATZ_GUI_STYLE_SOURCE_COLLECTION:
            return "collection";
        case URSATZ_GUI_STYLE_SOURCE_PIECES:
        default:
            return "pieces";
    }
}

/* A source with its filter values normalized, ready to resolve and to store. */
typedef struct {
    UrsatzGuiStyleSourceKind kind;
    char *composer; /* "" when unset */
    char *source;
    char *tag;
    char *collection_id;
    IdList piece_ids;
} NormalizedSource;

static void normalized_source_free(NormalizedSource *source) {
    free(source->composer);
    free(source->source);
    free(source->tag);
    free(source->collection_id);
    id_list_free(&source->piece_ids);
    memset(source, 0, sizeof(*source));
}

/* Normalizes the filter values of a request; INVALID_SOURCE when none is given or a tag is invalid. */
static UrsatzGuiStyleResult normalized_source_make(const UrsatzGuiStyleSource *input, NormalizedSource *out) {
    memset(out, 0, sizeof(*out));
    out->kind = input->kind;

    if (input->kind == URSATZ_GUI_STYLE_SOURCE_FILTER) {
        bool too_long = false;
        bool invalid = false;
        out->composer = ursatz_gui_meta_normalize_value(input->composer, &too_long);
        out->source = out->composer ? ursatz_gui_meta_normalize_value(input->source, &too_long) : NULL;
        if (!out->composer || !out->source) {
            normalized_source_free(out);
            return too_long ? URSATZ_GUI_STYLE_ERR_INVALID_SOURCE : URSATZ_GUI_STYLE_ERR_ALLOC;
        }
        char *tag_text = ursatz_gui_meta_normalize_value(input->tag, &too_long);
        if (!tag_text) {
            normalized_source_free(out);
            return too_long ? URSATZ_GUI_STYLE_ERR_INVALID_SOURCE : URSATZ_GUI_STYLE_ERR_ALLOC;
        }
        out->tag = tag_text[0] != '\0' ? ursatz_gui_meta_normalize_tag(tag_text, &invalid) : dup_string("");
        free(tag_text);
        if (!out->tag) {
            normalized_source_free(out);
            return invalid ? URSATZ_GUI_STYLE_ERR_INVALID_SOURCE : URSATZ_GUI_STYLE_ERR_ALLOC;
        }
        if (out->composer[0] == '\0' && out->source[0] == '\0' && out->tag[0] == '\0') {
            normalized_source_free(out);
            return URSATZ_GUI_STYLE_ERR_INVALID_SOURCE;
        }
        return URSATZ_GUI_STYLE_OK;
    }

    if (input->kind == URSATZ_GUI_STYLE_SOURCE_COLLECTION) {
        if (!ursatz_gui_collection_is_valid_id(input->collection_id)) {
            return URSATZ_GUI_STYLE_ERR_INVALID_SOURCE;
        }
        out->collection_id = dup_string(input->collection_id);
        return out->collection_id ? URSATZ_GUI_STYLE_OK : URSATZ_GUI_STYLE_ERR_ALLOC;
    }

    if (input->piece_count == 0 || !input->piece_ids) {
        return URSATZ_GUI_STYLE_ERR_INVALID_SOURCE;
    }
    for (size_t i = 0; i < input->piece_count; i++) {
        if (!ursatz_gui_collection_is_valid_id(input->piece_ids[i])) {
            normalized_source_free(out);
            return URSATZ_GUI_STYLE_ERR_INVALID_SOURCE;
        }
        if (!id_list_contains(&out->piece_ids, input->piece_ids[i]) &&
            !id_list_add(&out->piece_ids, input->piece_ids[i])) {
            normalized_source_free(out);
            return URSATZ_GUI_STYLE_ERR_ALLOC;
        }
    }
    return URSATZ_GUI_STYLE_OK;
}

static void free_summaries(UrsatzGuiPieceSummary *pieces, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(pieces[i].id);
        free(pieces[i].filename);
        free(pieces[i].format);
        free(pieces[i].uploaded_at);
        free(pieces[i].composer);
        free(pieces[i].source);
        ursatz_gui_meta_tags_free(pieces[i].tags, pieces[i].tag_count);
    }
    free(pieces);
}

/*
 * The piece ids a source names, in the order the Library lists them (filter),
 * the collection's add order, or as given. ids that name no piece are not
 * dropped here: collect_pieces skips them with the reason "not found".
 * default_name receives the style's default name.
 */
static UrsatzGuiStyleResult resolve_piece_ids(const NormalizedSource *source, IdList *ids, char *default_name,
                                              size_t default_name_size) {
    if (source->kind == URSATZ_GUI_STYLE_SOURCE_FILTER) {
        UrsatzGuiMetaFilter filter = {0};
        filter.composer = source->composer[0] ? source->composer : NULL;
        filter.source = source->source[0] ? source->source : NULL;
        filter.tag = source->tag[0] ? source->tag : NULL;

        UrsatzGuiPieceSummary *pieces = NULL;
        size_t count = 0;
        if (ursatz_gui_analysis_list(kPiecesDir, &pieces, &count) != URSATZ_GUI_ANALYSIS_OK) {
            return URSATZ_GUI_STYLE_ERR_FAILED;
        }
        for (size_t i = 0; i < count; i++) {
            if (ursatz_gui_meta_matches(&pieces[i], &filter) && !id_list_add(ids, pieces[i].id)) {
                free_summaries(pieces, count);
                return URSATZ_GUI_STYLE_ERR_ALLOC;
            }
        }
        free_summaries(pieces, count);

        char name[NAME_MAX_BYTES * 3 + 32] = "";
        size_t used = 0;
        const char *labels[3] = {"Composer: ", "Source: ", "Tag: "};
        const char *values[3] = {source->composer, source->source, source->tag};
        for (int i = 0; i < 3; i++) {
            if (values[i][0] == '\0') {
                continue;
            }
            int n = snprintf(name + used, sizeof(name) - used, "%s%s%s", used > 0 ? " \xC2\xB7 " : "", labels[i],
                             values[i]);
            if (n < 0 || (size_t)n >= sizeof(name) - used) {
                return URSATZ_GUI_STYLE_ERR_FAILED;
            }
            used += (size_t)n;
        }
        clip_utf8(name, NAME_MAX_BYTES);
        snprintf(default_name, default_name_size, "%s", name);
        return URSATZ_GUI_STYLE_OK;
    }

    if (source->kind == URSATZ_GUI_STYLE_SOURCE_COLLECTION) {
        UrsatzGuiCollection *collection = NULL;
        UrsatzGuiCollectionResult result = ursatz_gui_collection_get(source->collection_id, &collection);
        if (result == URSATZ_GUI_COLLECTION_ERR_NOT_FOUND) {
            return URSATZ_GUI_STYLE_ERR_NOT_FOUND;
        }
        if (result != URSATZ_GUI_COLLECTION_OK) {
            return result == URSATZ_GUI_COLLECTION_ERR_ALLOC ? URSATZ_GUI_STYLE_ERR_ALLOC : URSATZ_GUI_STYLE_ERR_FAILED;
        }
        bool ok = true;
        for (size_t i = 0; ok && i < collection->piece_count; i++) {
            ok = id_list_add(ids, collection->pieces[i]);
        }
        char name[NAME_MAX_BYTES * 2 + 32];
        snprintf(name, sizeof(name), "Collection: %s", collection->name);
        clip_utf8(name, NAME_MAX_BYTES);
        snprintf(default_name, default_name_size, "%s", name);
        ursatz_gui_collection_destroy(collection);
        return ok ? URSATZ_GUI_STYLE_OK : URSATZ_GUI_STYLE_ERR_ALLOC;
    }

    for (size_t i = 0; i < source->piece_ids.count; i++) {
        if (!id_list_add(ids, source->piece_ids.items[i])) {
            return URSATZ_GUI_STYLE_ERR_ALLOC;
        }
    }
    snprintf(default_name, default_name_size, "Selected pieces");
    return URSATZ_GUI_STYLE_OK;
}

/* --- reading pieces into a profile ---------------------------------------- */

/* The pieces a build used and left out, with the profile they made. */
typedef struct {
    StyleProfile *profile;
    IdList used;
    SkippedList skipped;
    char default_name[NAME_MAX_BYTES + 1];
} Collected;

static void collected_free(Collected *collected) {
    style_profile_destroy(collected->profile);
    id_list_free(&collected->used);
    ursatz_gui_style_skipped_free(collected->skipped.items, collected->skipped.count);
    memset(collected, 0, sizeof(*collected));
}

/* The document_schema_version recorded in meta; 0 when absent. */
static int meta_schema_version(const cJSON *meta) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(meta, "document_schema_version");
    return cJSON_IsNumber(item) ? item->valueint : 0;
}

/*
 * Adds one piece's analysis document to the profile. false with *reason set
 * when the piece is skipped; *alloc_failed set when memory ran out.
 */
static bool add_piece(StyleProfile *profile, const char *id, const char **reason) {
    char dir_path[PATH_BYTES];
    char file_path[PATH_BYTES + 16];
    if (!ursatz_gui_collection_is_valid_id(id) || snprintf(dir_path, sizeof(dir_path), "%s/%s", kPiecesDir, id) >=
                                                     (int)sizeof(dir_path)) {
        *reason = "not found";
        return false;
    }
    snprintf(file_path, sizeof(file_path), "%s/meta.json", dir_path);
    cJSON *meta = read_json_object(file_path);
    if (!meta) {
        *reason = "not found";
        return false;
    }
    int version = meta_schema_version(meta);
    cJSON_Delete(meta);

    if (version != URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION &&
        ursatz_gui_analysis_reanalyze(kPiecesDir, id) != URSATZ_GUI_ANALYSIS_OK) {
        *reason = "re-analysis failed";
        return false;
    }

    snprintf(file_path, sizeof(file_path), "%s/analysis.json", dir_path);
    char *document = read_file(file_path);
    if (!document) {
        *reason = "unreadable analysis";
        return false;
    }
    StyleProfileResult result = style_profile_add_document_json(profile, document);
    free(document);
    if (result != STYLE_PROFILE_OK) {
        *reason = "unreadable analysis";
        return false;
    }
    return true;
}

/* Resolves the source and adds every piece it names. The caller holds the lock. */
static UrsatzGuiStyleResult collect_pieces(const NormalizedSource *source, Collected *collected) {
    memset(collected, 0, sizeof(*collected));

    IdList ids = {0};
    char default_name[NAME_MAX_BYTES + 1] = "";
    UrsatzGuiStyleResult resolved = resolve_piece_ids(source, &ids, default_name, sizeof(default_name));
    if (resolved != URSATZ_GUI_STYLE_OK) {
        id_list_free(&ids);
        return resolved;
    }
    snprintf(collected->default_name, sizeof(collected->default_name), "%s", default_name);

    collected->profile = style_profile_create();
    if (!collected->profile) {
        id_list_free(&ids);
        return URSATZ_GUI_STYLE_ERR_ALLOC;
    }

    UrsatzGuiStyleResult result = URSATZ_GUI_STYLE_OK;
    for (size_t i = 0; result == URSATZ_GUI_STYLE_OK && i < ids.count; i++) {
        const char *reason = NULL;
        if (add_piece(collected->profile, ids.items[i], &reason)) {
            if (!id_list_add(&collected->used, ids.items[i])) {
                result = URSATZ_GUI_STYLE_ERR_ALLOC;
            }
        } else if (!skipped_add(&collected->skipped, ids.items[i], reason)) {
            result = URSATZ_GUI_STYLE_ERR_ALLOC;
        }
    }
    id_list_free(&ids);

    if (result == URSATZ_GUI_STYLE_OK && collected->used.count == 0) {
        result = URSATZ_GUI_STYLE_ERR_NO_PIECES;
    }
    if (result != URSATZ_GUI_STYLE_OK) {
        collected_free(collected);
    }
    return result;
}

/* --- writing a style ------------------------------------------------------ */

static cJSON *string_or_null(const char *text) {
    return text && text[0] != '\0' ? cJSON_CreateString(text) : cJSON_CreateNull();
}

static cJSON *source_json(const NormalizedSource *source) {
    cJSON *object = cJSON_CreateObject();
    if (!object || !cJSON_AddStringToObject(object, "kind", source_kind_label(source->kind))) {
        cJSON_Delete(object);
        return NULL;
    }
    cJSON *filter = NULL;
    if (source->kind == URSATZ_GUI_STYLE_SOURCE_FILTER) {
        filter = cJSON_CreateObject();
        if (!filter || !cJSON_AddItemToObject(filter, "composer", string_or_null(source->composer)) ||
            !cJSON_AddItemToObject(filter, "source", string_or_null(source->source)) ||
            !cJSON_AddItemToObject(filter, "tag", string_or_null(source->tag))) {
            cJSON_Delete(filter);
            cJSON_Delete(object);
            return NULL;
        }
    } else {
        filter = cJSON_CreateNull();
    }
    cJSON *piece_ids = NULL;
    if (source->kind == URSATZ_GUI_STYLE_SOURCE_PIECES) {
        piece_ids = cJSON_CreateArray();
        for (size_t i = 0; piece_ids && i < source->piece_ids.count; i++) {
            cJSON *id = cJSON_CreateString(source->piece_ids.items[i]);
            if (!id) {
                cJSON_Delete(piece_ids);
                piece_ids = NULL;
            } else {
                cJSON_AddItemToArray(piece_ids, id);
            }
        }
    } else {
        piece_ids = cJSON_CreateNull();
    }
    if (!filter || !piece_ids || !cJSON_AddItemToObject(object, "filter", filter) ||
        !cJSON_AddItemToObject(object, "collection_id", string_or_null(source->collection_id)) ||
        !cJSON_AddItemToObject(object, "piece_ids", piece_ids)) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* The meta.json text of a style (caller frees), or NULL. */
static char *meta_text(const char *id, const char *name, const char *created_at, const char *updated_at,
                       const NormalizedSource *source, const Collected *collected) {
    cJSON *meta = cJSON_CreateObject();
    cJSON *source_object = source_json(source);
    cJSON *used = cJSON_CreateArray();
    cJSON *skipped = cJSON_CreateArray();
    bool ok = meta && source_object && used && skipped && cJSON_AddStringToObject(meta, "id", id) &&
              cJSON_AddStringToObject(meta, "name", name) && cJSON_AddStringToObject(meta, "created_at", created_at) &&
              cJSON_AddStringToObject(meta, "updated_at", updated_at);
    if (ok) {
        cJSON_AddItemToObject(meta, "source", source_object);
        source_object = NULL;
    }
    for (size_t i = 0; ok && i < collected->used.count; i++) {
        cJSON *piece = cJSON_CreateString(collected->used.items[i]);
        ok = piece != NULL;
        if (ok) {
            cJSON_AddItemToArray(used, piece);
        }
    }
    for (size_t i = 0; ok && i < collected->skipped.count; i++) {
        cJSON *entry = cJSON_CreateObject();
        ok = entry && cJSON_AddStringToObject(entry, "id", collected->skipped.items[i].id) &&
             cJSON_AddStringToObject(entry, "reason", collected->skipped.items[i].reason);
        if (ok) {
            cJSON_AddItemToArray(skipped, entry);
        } else {
            cJSON_Delete(entry);
        }
    }
    char *text = NULL;
    if (ok) {
        cJSON_AddItemToObject(meta, "pieces", used);
        cJSON_AddItemToObject(meta, "skipped", skipped);
        used = skipped = NULL;
        ok = cJSON_AddNumberToObject(meta, "piece_count", (double)collected->used.count) != NULL;
    }
    if (ok) {
        text = cJSON_Print(meta);
    }
    cJSON_Delete(source_object);
    cJSON_Delete(used);
    cJSON_Delete(skipped);
    cJSON_Delete(meta);
    return text;
}

/* Writes style-model.json, then meta.json, into dir by temp file and rename. */
static bool write_style_files(const char *dir, const char *id, const char *name, const char *created_at,
                              const char *updated_at, const NormalizedSource *source, const Collected *collected) {
    char model_path[PATH_BYTES];
    char tmp_path[PATH_BYTES + 8];
    char meta_path[PATH_BYTES];
    if (snprintf(model_path, sizeof(model_path), "%s/style-model.json", dir) >= (int)sizeof(model_path) ||
        snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", model_path) >= (int)sizeof(tmp_path) ||
        snprintf(meta_path, sizeof(meta_path), "%s/meta.json", dir) >= (int)sizeof(meta_path)) {
        return false;
    }
    char *text = meta_text(id, name, created_at, updated_at, source, collected);
    if (!text) {
        return false;
    }
    bool ok = style_model_file_write(collected->profile, name, tmp_path) == STYLE_MODEL_FILE_OK &&
              replace_file(tmp_path, model_path) && write_file_atomic(meta_path, text, strlen(text));
    free(text);
    if (!ok) {
        remove(tmp_path);
    }
    return ok;
}

/* Removes a style directory this module made: its two files and temp files. */
static void remove_style_dir(const char *dir) {
    const char *files[] = {"style-model.json", "style-model.json.tmp", "meta.json", "meta.json.tmp"};
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        char path[PATH_BYTES + 32];
        snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
        remove(path);
    }
#ifdef _WIN32
    _rmdir(dir);
#else
    rmdir(dir);
#endif
}

/* --- build ---------------------------------------------------------------- */

/* The style's name: the given one normalized, or the default when it is empty. */
static UrsatzGuiStyleResult style_name(const char *requested, const char *default_name, char *out, size_t out_size) {
    bool too_long = false;
    char *normalized = ursatz_gui_meta_normalize_value(requested, &too_long);
    if (!normalized) {
        return too_long ? URSATZ_GUI_STYLE_ERR_INVALID_NAME : URSATZ_GUI_STYLE_ERR_ALLOC;
    }
    snprintf(out, out_size, "%s", normalized[0] != '\0' ? normalized : default_name);
    free(normalized);
    return URSATZ_GUI_STYLE_OK;
}

UrsatzGuiStyleResult ursatz_gui_style_build(const char *name, const UrsatzGuiStyleSource *source, char **out_id,
                                            UrsatzGuiStyleSkipped **out_skipped, size_t *out_skipped_count) {
    if (out_id) {
        *out_id = NULL;
    }
    if (out_skipped) {
        *out_skipped = NULL;
    }
    if (out_skipped_count) {
        *out_skipped_count = 0;
    }
    if (!source || !out_id) {
        return URSATZ_GUI_STYLE_ERR_INVALID_SOURCE;
    }

    NormalizedSource normalized;
    UrsatzGuiStyleResult result = normalized_source_make(source, &normalized);
    if (result != URSATZ_GUI_STYLE_OK) {
        return result;
    }

    LOCK();
    Collected collected;
    char *id = NULL;
    char style_dir[PATH_BYTES];
    bool dir_made = false;

    result = collect_pieces(&normalized, &collected);
    char final_name[NAME_MAX_BYTES + 1];
    if (result == URSATZ_GUI_STYLE_OK) {
        result = style_name(name, collected.default_name, final_name, sizeof(final_name));
    }
    if (result == URSATZ_GUI_STYLE_OK) {
        result = URSATZ_GUI_STYLE_ERR_FAILED;
        if (ensure_dir(kStylesDir)) {
            for (int attempt = 0; attempt < ID_MAX_TRIES && !dir_made; attempt++) {
                id = ursatz_gui_new_id("sty");
                if (!id) {
                    result = URSATZ_GUI_STYLE_ERR_ALLOC;
                    break;
                }
                if (snprintf(style_dir, sizeof(style_dir), "%s/%s", kStylesDir, id) < (int)sizeof(style_dir)) {
                    dir_made = make_dir(style_dir);
                }
                if (!dir_made) {
                    free(id);
                    id = NULL;
                }
            }
        }
    }
    if (dir_made) {
        char now[32];
        iso_utc_now(now, sizeof(now));
        if (write_style_files(style_dir, id, final_name, now, now, &normalized, &collected)) {
            result = URSATZ_GUI_STYLE_OK;
        } else {
            remove_style_dir(style_dir);
        }
    }
    if (result == URSATZ_GUI_STYLE_OK) {
        *out_id = id;
        id = NULL;
        if (out_skipped) {
            *out_skipped = collected.skipped.items;
            collected.skipped.items = NULL;
            if (out_skipped_count) {
                *out_skipped_count = collected.skipped.count;
            }
        }
    }
    free(id);
    if (collected.profile || collected.used.items || collected.skipped.items) {
        collected_free(&collected);
    }
    UNLOCK();
    normalized_source_free(&normalized);
    return result;
}
