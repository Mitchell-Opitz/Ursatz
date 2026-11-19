#include "library_pieces.h"
#include "base/last_error.h"
#include "ids.h"
#include "library_metadata.h"
#include "piece_lock.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cJSON.h>

#include <ursatz_analyzer/analyzer_api.h>

#include <common/files.h>
#include <common/strings.h>

#define PIECE_ID_MAX_TRIES 8

/* The name a status goes by in the routes and the stored flags. */
const char *workshop_library_status_name(AnalyzerDocumentStatus status) {
    switch (status) {
    case ANALYZER_DOCUMENT_CURRENT:
        return "current";
    case ANALYZER_DOCUMENT_STALE_SCHEMA:
        return "stale_schema";
    case ANALYZER_DOCUMENT_STALE_ANALYZER:
        return "stale_analyzer";
    case ANALYZER_DOCUMENT_STALE_URSATZ:
        return "stale_ursatz";
    default:
        return "unreadable";
    }
}

/* The status the three keys in meta.json give; a missing key is unreadable. */
static const char *meta_status_name(const cJSON *meta) {
    const cJSON *schema = cJSON_GetObjectItemCaseSensitive(meta, "document_schema_version");
    const cJSON *analyzer = cJSON_GetObjectItemCaseSensitive(meta, "analyzer_version");
    const cJSON *commit = cJSON_GetObjectItemCaseSensitive(meta, "ursatz_commit");
    if (!cJSON_IsNumber(schema) || !cJSON_IsString(analyzer) || !cJSON_IsString(commit)) {
        return workshop_library_status_name(ANALYZER_DOCUMENT_UNREADABLE);
    }
    return workshop_library_status_name(analyzer_document_status_of(schema->valueint, analyzer->valuestring, commit->valuestring));
}

/* Writes <dir_path>/meta.json. */
bool workshop_library_meta_write_new(const char *dir_path, const char *filename, const char *format, const char *uploaded_at) {
    cJSON *meta = cJSON_CreateObject();
    if (!meta) {
        return false;
    }
    cJSON_AddStringToObject(meta, "filename", filename ? filename : "");
    cJSON_AddStringToObject(meta, "format", format);
    cJSON_AddStringToObject(meta, "uploaded_at", uploaded_at);
    cJSON_AddStringToObject(meta, "analyzer_version", analyzer_version());
    cJSON_AddNumberToObject(meta, "document_schema_version", ANALYZER_DOCUMENT_SCHEMA_VERSION);
    cJSON_AddStringToObject(meta, "ursatz_commit", analyzer_producer_key());
    char *text = cJSON_Print(meta);
    cJSON_Delete(meta);
    if (!text) {
        return false;
    }
    char meta_path[400];
    snprintf(meta_path, sizeof(meta_path), "%s/meta.json", dir_path);
    bool ok = (common_file_write_atomic(meta_path, text, strlen(text), NULL) == URSATZ_OK);
    free(text);
    return ok;
}

/* Parses <dir_path>/meta.json; NULL when it is missing or not an object. */
cJSON *workshop_library_meta_read(const char *dir_path) {
    char meta_path[400];
    snprintf(meta_path, sizeof(meta_path), "%s/meta.json", dir_path);
    char *text = NULL;
    common_file_read(meta_path, &text, NULL, NULL);
    if (!text) {
        return NULL;
    }
    cJSON *meta = cJSON_Parse(text);
    free(text);
    if (meta && !cJSON_IsObject(meta)) {
        cJSON_Delete(meta);
        return NULL;
    }
    return meta;
}

/* A copy of a string member of meta, "" when it is absent. */
char *workshop_library_meta_string_copy(const cJSON *meta, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(meta, key);
    return common_strdup(cJSON_IsString(item) && item->valuestring ? item->valuestring : "");
}

/* Copies the string members of meta's "tags" array; *out_tags is NULL when there are none. */
static bool meta_tags_copy(const cJSON *meta, char ***out_tags, size_t *out_count) {
    *out_tags = NULL;
    *out_count = 0;
    const cJSON *array = cJSON_GetObjectItemCaseSensitive(meta, "tags");
    if (!cJSON_IsArray(array)) {
        return true;
    }
    int size = cJSON_GetArraySize(array);
    if (size <= 0) {
        return true;
    }
    char **tags = calloc((size_t)size, sizeof(char *));
    if (!tags) {
        return false;
    }
    size_t count = 0;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, array) {
        if (!cJSON_IsString(item) || !item->valuestring) {
            continue;
        }
        tags[count] = common_strdup(item->valuestring);
        if (!tags[count]) {
            workshop_meta_tags_free(tags, count);
            return false;
        }
        count++;
    }
    if (count == 0) {
        free(tags);
        return true;
    }
    *out_tags = tags;
    *out_count = count;
    return true;
}

/* Copies meta's "notation" object. False (and *out zeroed) when it is absent, null or not an object. */
bool workshop_library_meta_notation(const cJSON *meta, WorkshopNotation *out) {
    memset(out, 0, sizeof(*out));
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(meta, "notation");
    if (!cJSON_IsObject(item)) {
        return false;
    }
    const cJSON *meter = cJSON_GetObjectItemCaseSensitive(item, "meter");
    const cJSON *scale = cJSON_GetObjectItemCaseSensitive(item, "value_scale");
    const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(item, "pickup");
    if (cJSON_IsString(meter) && meter->valuestring) {
        snprintf(out->meter, sizeof(out->meter), "%s", meter->valuestring);
    }
    if (cJSON_IsString(scale) && scale->valuestring) {
        snprintf(out->value_scale, sizeof(out->value_scale), "%s", scale->valuestring);
    }
    if (cJSON_IsString(pickup) && pickup->valuestring) {
        snprintf(out->pickup, sizeof(out->pickup), "%s", pickup->valuestring);
    }
    return true;
}

/* True when every set member of n is text the Analyzer reads. */
static bool notation_is_valid(const WorkshopNotation *n) {
    ImportedReading reading = imported_reading_none();
    return (n->meter[0] == '\0' || imported_reading_parse_meter(n->meter, &reading)) &&
           (n->value_scale[0] == '\0' || imported_reading_parse_scale(n->value_scale, &reading)) &&
           (n->pickup[0] == '\0' || imported_reading_parse_pickup(n->pickup, &reading));
}

/* Sets (n) or removes (NULL) meta's "notation" object. */
static bool meta_set_notation(cJSON *meta, const WorkshopNotation *n) {
    if (!n) {
        cJSON_DeleteItemFromObjectCaseSensitive(meta, "notation");
        return true;
    }
    cJSON *object = cJSON_CreateObject();
    if (!object) {
        return false;
    }
    if (!cJSON_AddStringToObject(object, "meter", n->meter) ||
        !cJSON_AddStringToObject(object, "value_scale", n->value_scale) ||
        !cJSON_AddStringToObject(object, "pickup", n->pickup)) {
        cJSON_Delete(object);
        return false;
    }
    if (cJSON_ReplaceItemInObjectCaseSensitive(meta, "notation", object)) {
        return true;
    }
    cJSON_AddItemToObject(meta, "notation", object);
    return true;
}

static void piece_summary_destroy_contents(WorkshopPieceSummary *piece) {
    free(piece->id);
    free(piece->filename);
    free(piece->format);
    free(piece->uploaded_at);
    free(piece->composer);
    free(piece->source);
    workshop_meta_tags_free(piece->tags, piece->tag_count);
}

static bool has_extension(const char *filename, const char *extension) {
    if (!filename) {
        return false;
    }
    size_t name_len = strlen(filename);
    size_t ext_len = strlen(extension);
    if (name_len < ext_len) {
        return false;
    }
    for (size_t i = 0; i < ext_len; i++) {
        if (tolower((unsigned char)filename[name_len - ext_len + i]) != extension[i]) {
            return false;
        }
    }
    return true;
}

/* The stored source file's name: source.mid for MIDI; for MusicXML
   source.xml / source.mxl when the upload had that extension, else
   source.musicxml. */
const char *workshop_library_source_name(ImportedFormat format, const char *original_filename) {
    if (format == IMPORTED_FORMAT_MIDI) {
        return "source.mid";
    }
    if (has_extension(original_filename, ".xml")) {
        return "source.xml";
    }
    if (has_extension(original_filename, ".mxl")) {
        return "source.mxl";
    }
    return "source.musicxml";
}

/* Creates pieces_dir/<id>/ with a fresh id, re-drawing while the directory
   exists. Returns OK, ERR_ALLOC or ERR_PERSISTENCE_FAILED. */
UrsatzStatus workshop_library_piece_create(const char *pieces_dir, char **out_id) {
    for (int attempt = 0; attempt < PIECE_ID_MAX_TRIES; attempt++) {
        char *id = workshop_new_id("piece");
        if (!id) {
            return workshop_fail(URSATZ_ERR_ALLOC, NULL);
        }
        char dir_path[320];
        snprintf(dir_path, sizeof(dir_path), "%s/%s", pieces_dir, id);
        UrsatzStatus made = common_dir_create(dir_path, NULL);
        if (made == URSATZ_OK) {
            *out_id = id;
            return URSATZ_OK;
        }
        free(id);
        if (made != URSATZ_ERR_CONFLICT) {
            return workshop_fail(URSATZ_ERR_IO, NULL);
        }
    }
    return workshop_fail(URSATZ_ERR_IO, NULL);
}

/* Newest upload first, then id. */
static int compare_pieces(const void *left, const void *right) {
    const WorkshopPieceSummary *a = left;
    const WorkshopPieceSummary *b = right;
    int by_time = strcmp(b->uploaded_at, a->uploaded_at);
    return by_time != 0 ? by_time : strcmp(a->id, b->id);
}

UrsatzStatus workshop_library_list(const char *pieces_dir, WorkshopPieceSummary **out_pieces,
                                                  size_t *out_count) {
    *out_pieces = NULL;
    *out_count = 0;

    /* A missing folder lists as no pieces. */
    CommonNames names = {0};
    if (common_dir_list(pieces_dir, &names, NULL) == URSATZ_ERR_ALLOC) {
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }

    WorkshopPieceSummary *pieces = NULL;
    if (names.count > 0) {
        pieces = calloc(names.count, sizeof(WorkshopPieceSummary));
        if (!pieces) {
            common_names_free(&names);
            return workshop_fail(URSATZ_ERR_ALLOC, NULL);
        }
    }

    /* Anything that is not a directory with a readable meta.json (an old
       "<id>.db" piece, a stray file) is not a piece. */
    size_t count = 0;
    bool failed = false;
    for (size_t i = 0; i < names.count && !failed; i++) {
        char dir_path[320];
        snprintf(dir_path, sizeof(dir_path), "%s/%s", pieces_dir, names.items[i]);
        if (!common_path_is_dir(dir_path)) {
            continue;
        }
        cJSON *meta = workshop_library_meta_read(dir_path);
        if (!meta) {
            continue;
        }
        WorkshopPieceSummary *piece = &pieces[count];
        piece->id = common_strdup(names.items[i]);
        piece->filename = workshop_library_meta_string_copy(meta, "filename");
        piece->format = workshop_library_meta_string_copy(meta, "format");
        piece->uploaded_at = workshop_library_meta_string_copy(meta, "uploaded_at");
        piece->composer = workshop_library_meta_string_copy(meta, "composer");
        piece->source = workshop_library_meta_string_copy(meta, "source");
        bool tags_ok = meta_tags_copy(meta, &piece->tags, &piece->tag_count);
        piece->has_notation = workshop_library_meta_notation(meta, &piece->notation);
        piece->analysis_status = meta_status_name(meta);
        cJSON_Delete(meta);
        if (!piece->id || !piece->filename || !piece->format || !piece->uploaded_at || !piece->composer ||
            !piece->source || !tags_ok) {
            piece_summary_destroy_contents(piece);
            failed = true;
            break;
        }
        count++;
    }
    common_names_free(&names);

    if (failed) {
        for (size_t i = 0; i < count; i++) {
            piece_summary_destroy_contents(&pieces[i]);
        }
        free(pieces);
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    if (count == 0) {
        free(pieces);
        return URSATZ_OK;
    }

    qsort(pieces, count, sizeof(WorkshopPieceSummary), compare_pieces);
    *out_pieces = pieces;
    *out_count = count;
    return URSATZ_OK;
}

static bool is_valid_piece_id(const char *id) {
    if (!id || id[0] == '\0') {
        return false;
    }
    for (const char *c = id; *c; c++) {
        if (!(isalnum((unsigned char)*c) || *c == '-' || *c == '_')) {
            return false;
        }
    }
    return true;
}

/* pieces_dir/<id> when id is valid and that directory exists. */
bool workshop_library_piece_dir(const char *pieces_dir, const char *id, char *out, size_t out_size) {
    if (!is_valid_piece_id(id)) {
        return false;
    }
    if (snprintf(out, out_size, "%s/%s", pieces_dir, id) >= (int)out_size) {
        return false;
    }
    return common_path_is_dir(out);
}

/* Writes the path of the piece's stored source file into out. */
bool workshop_library_source_path(const char *dir_path, char *out, size_t out_size) {
    static const char *const kNames[] = {"source.mid", "source.musicxml", "source.xml", "source.mxl"};
    for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); i++) {
        snprintf(out, out_size, "%s/%s", dir_path, kNames[i]);
        if (common_path_exists(out)) {
            return true;
        }
    }
    return false;
}

UrsatzStatus workshop_library_delete_piece_unlocked(const char *pieces_dir, const char *id) {
    char dir_path[320];
    if (!workshop_library_piece_dir(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    common_dir_remove(dir_path, NULL);
    return common_path_is_dir(dir_path) ? workshop_fail(URSATZ_ERR_IO, NULL) : URSATZ_OK;
}

/* Sets a string member of meta in place (keeping its position), adding it
   when absent. */
bool workshop_library_meta_set_string(cJSON *meta, const char *key, const char *value) {
    cJSON *item = cJSON_CreateString(value);
    if (!item) {
        return false;
    }
    if (cJSON_ReplaceItemInObjectCaseSensitive(meta, key, item)) {
        return true;
    }
    cJSON_AddItemToObject(meta, key, item);
    return true;
}

bool workshop_library_meta_set_number(cJSON *meta, const char *key, double value) {
    cJSON *item = cJSON_CreateNumber(value);
    if (!item) {
        return false;
    }
    if (cJSON_ReplaceItemInObjectCaseSensitive(meta, key, item)) {
        return true;
    }
    cJSON_AddItemToObject(meta, key, item);
    return true;
}

/* Replaces meta's "tags" member with an array of the given strings. */
static bool meta_set_tags(cJSON *meta, char *const *tags, size_t count) {
    cJSON *array = cJSON_CreateArray();
    if (!array) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        cJSON *item = cJSON_CreateString(tags[i]);
        if (!item) {
            cJSON_Delete(array);
            return false;
        }
        cJSON_AddItemToArray(array, item);
    }
    if (cJSON_ReplaceItemInObjectCaseSensitive(meta, "tags", array)) {
        return true;
    }
    cJSON_AddItemToObject(meta, "tags", array);
    return true;
}

/* Normalizes composer or source and stores it in meta. */
static UrsatzStatus meta_apply_value(cJSON *meta, const char *key, const char *text, UrsatzError *error) {
    bool too_long = false;
    char *value = workshop_meta_normalize_value(text, &too_long);
    if (!value) {
        if (too_long) {
            char reason[48];
            snprintf(reason, sizeof(reason), "%s is too long", key);
            workshop_error_text(error, reason);
            return workshop_fail(URSATZ_ERR_REQUEST, NULL);
        }
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    bool ok = workshop_library_meta_set_string(meta, key, value);
    free(value);
    return ok ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, NULL);
}

/* Normalizes each of the given tags and adds it to (or removes it from) the list. */
static UrsatzStatus tags_apply(char ***tags, size_t *count, const char *const *items, size_t item_count,
                                          bool remove, UrsatzError *error) {
    for (size_t i = 0; i < item_count; i++) {
        bool invalid = false;
        char *tag = workshop_meta_normalize_tag(items[i], &invalid);
        if (!tag) {
            if (invalid) {
                workshop_error_text(error, "invalid tag");
                return workshop_fail(URSATZ_ERR_REQUEST, NULL);
            }
            return workshop_fail(URSATZ_ERR_ALLOC, NULL);
        }
        UrsatzStatus result =
            remove ? workshop_meta_tags_remove(tags, count, tag) : workshop_meta_tags_add(tags, count, tag);
        free(tag);
        if (result == URSATZ_ERR_REQUEST) {
            workshop_error_text(error, "too many tags");
            return workshop_fail(URSATZ_ERR_REQUEST, NULL);
        }
        if (result != URSATZ_OK) {
            return workshop_fail(URSATZ_ERR_ALLOC, NULL);
        }
    }
    return URSATZ_OK;
}

UrsatzStatus workshop_library_update_metadata_unlocked(const char *pieces_dir, const char *id,
                                                             const WorkshopMetaUpdate *update, UrsatzError *error) {
    char dir_path[320];
    if (!workshop_library_piece_dir(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = workshop_library_meta_read(dir_path);
    if (!meta) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }

    UrsatzStatus result = URSATZ_OK;
    if (update->filename && !workshop_library_meta_set_string(meta, "filename", update->filename)) {
        result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    if (result == URSATZ_OK && update->composer) {
        result = meta_apply_value(meta, "composer", update->composer, error);
    }
    if (result == URSATZ_OK && update->source) {
        result = meta_apply_value(meta, "source", update->source, error);
    }

    bool tags_changed = update->has_tags || update->add_tag_count > 0 || update->remove_tag_count > 0;
    if (result == URSATZ_OK && tags_changed) {
        char **tags = NULL;
        size_t tag_count = 0;
        if (!update->has_tags && !meta_tags_copy(meta, &tags, &tag_count)) {
            result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
        }
        if (result == URSATZ_OK && update->has_tags) {
            result = tags_apply(&tags, &tag_count, update->tags, update->tag_count, false, error);
        }
        if (result == URSATZ_OK) {
            result = tags_apply(&tags, &tag_count, update->add_tags, update->add_tag_count, false, error);
        }
        if (result == URSATZ_OK) {
            result = tags_apply(&tags, &tag_count, update->remove_tags, update->remove_tag_count, true, error);
        }
        if (result == URSATZ_OK && !meta_set_tags(meta, tags, tag_count)) {
            result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
        }
        workshop_meta_tags_free(tags, tag_count);
    }
    if (result == URSATZ_OK && update->has_notation) {
        if (update->notation && !notation_is_valid(update->notation)) {
            workshop_error_text(error, "invalid notation");
            result = workshop_fail(URSATZ_ERR_REQUEST, NULL);
        } else if (!meta_set_notation(meta, update->notation)) {
            result = workshop_fail(URSATZ_ERR_ALLOC, NULL);
        }
    }
    if (result != URSATZ_OK) {
        cJSON_Delete(meta);
        return result;
    }

    char *text = cJSON_Print(meta);
    cJSON_Delete(meta);
    if (!text) {
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    char meta_path[400];
    snprintf(meta_path, sizeof(meta_path), "%s/meta.json", dir_path);
    bool ok = (common_file_write_atomic(meta_path, text, strlen(text), NULL) == URSATZ_OK);
    free(text);
    return ok ? URSATZ_OK : workshop_fail(URSATZ_ERR_IO, NULL);
}

UrsatzStatus workshop_library_get_notation(const char *pieces_dir, const char *id, bool *out_has,
                                                          WorkshopNotation *out) {
    char dir_path[320];
    if (!workshop_library_piece_dir(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = workshop_library_meta_read(dir_path);
    if (!meta) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    *out_has = workshop_library_meta_notation(meta, out);
    cJSON_Delete(meta);
    return URSATZ_OK;
}

/* The public operations on one piece. Each takes the piece lock (piece_lock.h) around the whole operation, so the
   writers of a piece directory never interleave and a read never sees a half-written file. They call only the
   _unlocked functions above, never each other. */

bool workshop_library_piece_lock_enter(const char *id, UrsatzStatus *failure) {
    if (!is_valid_piece_id(id)) {
        *failure = URSATZ_ERR_NOT_FOUND;
        return false;
    }
    if (!workshop_piece_lock(id)) {
        *failure = URSATZ_ERR_ALLOC;
        return false;
    }
    return true;
}

bool workshop_library_piece_exists(const char *pieces_dir, const char *id) {
    char dir_path[320];
    char meta_path[400];
    return workshop_library_piece_dir(pieces_dir, id, dir_path, sizeof(dir_path)) &&
           snprintf(meta_path, sizeof(meta_path), "%s/meta.json", dir_path) < (int)sizeof(meta_path) &&
           common_path_exists(meta_path);
}

UrsatzStatus workshop_library_get_filename(const char *pieces_dir, const char *id, char **out_filename) {
    UrsatzStatus result;
    if (!workshop_library_piece_lock_enter(id, &result)) {
        return result;
    }
    char dir_path[320];
    cJSON *meta = workshop_library_piece_dir(pieces_dir, id, dir_path, sizeof(dir_path)) ? workshop_library_meta_read(dir_path) : NULL;
    if (!meta) {
        workshop_piece_unlock(id);
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    *out_filename = workshop_library_meta_string_copy(meta, "filename");
    cJSON_Delete(meta);
    workshop_piece_unlock(id);
    return *out_filename ? URSATZ_OK : workshop_fail(URSATZ_ERR_ALLOC, NULL);
}
