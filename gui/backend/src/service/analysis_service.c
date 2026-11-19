#include "analysis_service.h"
#include "last_error.h"
#include "id_service.h"
#include "piece_metadata.h"
#include "piece_lock.h"
#include "import_cache.h"
#include "workshop_internal.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/stat.h>
#include <unistd.h>

#include <cJSON.h>

#include <ursatz_analyzer/analyzer_api.h>

#include <common/files.h>
#include <common/strings.h>


#define PIECE_ID_MAX_TRIES 8

/* Creates one directory. Returns 1 if created, 0 if it already exists, -1 on
   any other failure. */
static const char *format_label(ImportedFormat format) {
    return format == IMPORTED_FORMAT_MIDI ? "midi" : "musicxml";
}

static const char *analyzer_version_string(void) {
    return analyzer_version();
}

/* The name a status goes by in the routes and the stored flags. */
static const char *status_name(AnalyzerDocumentStatus status) {
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
        return status_name(ANALYZER_DOCUMENT_UNREADABLE);
    }
    return status_name(analyzer_document_status_of(schema->valueint, analyzer->valuestring, commit->valuestring));
}

/* Writes <dir_path>/meta.json. */
static bool write_meta_file(const char *dir_path, const char *filename, const char *format, const char *uploaded_at) {
    cJSON *meta = cJSON_CreateObject();
    if (!meta) {
        return false;
    }
    cJSON_AddStringToObject(meta, "filename", filename ? filename : "");
    cJSON_AddStringToObject(meta, "format", format);
    cJSON_AddStringToObject(meta, "uploaded_at", uploaded_at);
    cJSON_AddStringToObject(meta, "analyzer_version", analyzer_version_string());
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
static cJSON *read_meta_file(const char *dir_path) {
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
static char *meta_string_copy(const cJSON *meta, const char *key) {
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
            ursatz_gui_meta_tags_free(tags, count);
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
static bool meta_notation_read(const cJSON *meta, UrsatzGuiNotation *out) {
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
static bool notation_is_valid(const UrsatzGuiNotation *n) {
    ImportedReading reading = imported_reading_none();
    return (n->meter[0] == '\0' || imported_reading_parse_meter(n->meter, &reading)) &&
           (n->value_scale[0] == '\0' || imported_reading_parse_scale(n->value_scale, &reading)) &&
           (n->pickup[0] == '\0' || imported_reading_parse_pickup(n->pickup, &reading));
}

/* Sets (n) or removes (NULL) meta's "notation" object. */
static bool meta_set_notation(cJSON *meta, const UrsatzGuiNotation *n) {
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

static void piece_summary_destroy_contents(UrsatzGuiPieceSummary *piece) {
    free(piece->id);
    free(piece->filename);
    free(piece->format);
    free(piece->uploaded_at);
    free(piece->composer);
    free(piece->source);
    ursatz_gui_meta_tags_free(piece->tags, piece->tag_count);
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
static const char *source_file_name(ImportedFormat format, const char *original_filename) {
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

static bool find_source_path(const char *dir_path, char *out, size_t out_size);
static bool import_piece(const char *dir_path, ImportedFormat format, ImportedScore *out);

/* Imports the piece's source (with its notation override, if any) and runs it
   through the Analyzer. On OK, *out_json is the analysis document (caller frees). */
static UrsatzStatus analyze_source_file(const char *dir_path, ImportedFormat format, char **out_json) {
    char source_path[400];
    ImportedScore notes;
    if (!find_source_path(dir_path, source_path, sizeof(source_path)) || !import_piece(dir_path, format, &notes)) {
        return ursatz_gui_fail(URSATZ_ERR_FORMAT, NULL);
    }

    AnalyzerOptions options = analyzer_options_default();
    options.knowledge = workshop_knowledge(NULL);
    AnalyzerResult *result = NULL;
    if (analyzer_analyze(&notes, &options, &result) != URSATZ_OK) {
        imported_score_destroy(&notes);
        return ursatz_gui_fail(URSATZ_ERR_FORMAT, NULL);
    }

    AnalysisDocument *document = analyzer_document_build(source_path, result);
    analyzer_result_destroy(result);
    imported_score_destroy(&notes);
    if (!document) {
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }
    char *json = analyzer_document_to_json(document);
    analyzer_document_destroy(document);
    if (!json) {
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }
    *out_json = json;
    return URSATZ_OK;
}

/* Creates pieces_dir/<id>/ with a fresh id, re-drawing while the directory
   exists. Returns OK, ERR_ALLOC or ERR_PERSISTENCE_FAILED. */
static UrsatzStatus create_piece_dir(const char *pieces_dir, char **out_id) {
    for (int attempt = 0; attempt < PIECE_ID_MAX_TRIES; attempt++) {
        char *id = ursatz_gui_new_id("piece");
        if (!id) {
            return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
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
            return ursatz_gui_fail(URSATZ_ERR_IO, NULL);
        }
    }
    return ursatz_gui_fail(URSATZ_ERR_IO, NULL);
}

/* Writes the upload into the new piece directory pieces_dir/<id>/ and analyzes it. On failure the directory is
   removed. The caller holds the piece lock. */
static UrsatzStatus save_new_piece(const char *pieces_dir, const char *id, const unsigned char *data,
                                              size_t size, const char *original_filename) {
    UrsatzStatus result;
    char dir_path[320];
    snprintf(dir_path, sizeof(dir_path), "%s/%s", pieces_dir, id);

    /* The bytes go to disk unchanged under a neutral name first: the format
       is detected from the content, and the final name depends on it. */
    char upload_path[400];
    snprintf(upload_path, sizeof(upload_path), "%s/source.upload", dir_path);
    if (!(common_file_write_atomic(upload_path, data, size, NULL) == URSATZ_OK)) {
        common_dir_remove(dir_path, NULL);
        return ursatz_gui_fail(URSATZ_ERR_IO, NULL);
    }

    ImportedFormat format = imported_format_detect(upload_path);
    if (format == IMPORTED_FORMAT_AUTO) {
        common_dir_remove(dir_path, NULL);
        return ursatz_gui_fail(URSATZ_ERR_REQUEST, NULL);
    }

    char source_path[400];
    snprintf(source_path, sizeof(source_path), "%s/%s", dir_path, source_file_name(format, original_filename));
    if (rename(upload_path, source_path) != 0) {
        common_dir_remove(dir_path, NULL);
        return ursatz_gui_fail(URSATZ_ERR_IO, NULL);
    }

    char *document_json = NULL;
    result = analyze_source_file(dir_path, format, &document_json);
    if (result != URSATZ_OK) {
        common_dir_remove(dir_path, NULL);
        return result;
    }

    char analysis_path[400];
    snprintf(analysis_path, sizeof(analysis_path), "%s/analysis.json", dir_path);
    bool written = (common_file_write_atomic(analysis_path, document_json, strlen(document_json), NULL) == URSATZ_OK);
    free(document_json);
    if (!written) {
        common_dir_remove(dir_path, NULL);
        return ursatz_gui_fail(URSATZ_ERR_IO, NULL);
    }

    char uploaded_at[32];
    common_utc_now(uploaded_at);
    if (!write_meta_file(dir_path, original_filename, format_label(format), uploaded_at)) {
        common_dir_remove(dir_path, NULL);
        return ursatz_gui_fail(URSATZ_ERR_IO, NULL);
    }

    return URSATZ_OK;
}

UrsatzStatus ursatz_gui_analysis_import_and_save(const char *pieces_dir, const unsigned char *data,
                                                              size_t size, const char *original_filename,
                                                              char **out_id) {
    if (!(common_dir_make(pieces_dir, NULL) == URSATZ_OK)) {
        return ursatz_gui_fail(URSATZ_ERR_IO, NULL);
    }

    char *id = NULL;
    UrsatzStatus result = create_piece_dir(pieces_dir, &id);
    if (result != URSATZ_OK) {
        return result;
    }

    /* The directory exists from here on, so a reader that lists pieces may open it before it is complete. */
    if (!ursatz_gui_piece_lock(id)) {
        free(id);
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }
    result = save_new_piece(pieces_dir, id, data, size, original_filename);
    ursatz_gui_piece_unlock(id);
    if (result != URSATZ_OK) {
        free(id);
        return result;
    }
    *out_id = id;
    return URSATZ_OK;
}

/* Newest upload first, then id. */
static int compare_pieces(const void *left, const void *right) {
    const UrsatzGuiPieceSummary *a = left;
    const UrsatzGuiPieceSummary *b = right;
    int by_time = strcmp(b->uploaded_at, a->uploaded_at);
    return by_time != 0 ? by_time : strcmp(a->id, b->id);
}

UrsatzStatus ursatz_gui_analysis_list(const char *pieces_dir, UrsatzGuiPieceSummary **out_pieces,
                                                  size_t *out_count) {
    *out_pieces = NULL;
    *out_count = 0;

    /* A missing folder lists as no pieces. */
    CommonNames names = {0};
    if (common_dir_list(pieces_dir, &names, NULL) == URSATZ_ERR_ALLOC) {
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }

    UrsatzGuiPieceSummary *pieces = NULL;
    if (names.count > 0) {
        pieces = calloc(names.count, sizeof(UrsatzGuiPieceSummary));
        if (!pieces) {
            common_names_free(&names);
            return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
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
        cJSON *meta = read_meta_file(dir_path);
        if (!meta) {
            continue;
        }
        UrsatzGuiPieceSummary *piece = &pieces[count];
        piece->id = common_strdup(names.items[i]);
        piece->filename = meta_string_copy(meta, "filename");
        piece->format = meta_string_copy(meta, "format");
        piece->uploaded_at = meta_string_copy(meta, "uploaded_at");
        piece->composer = meta_string_copy(meta, "composer");
        piece->source = meta_string_copy(meta, "source");
        bool tags_ok = meta_tags_copy(meta, &piece->tags, &piece->tag_count);
        piece->has_notation = meta_notation_read(meta, &piece->notation);
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
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }
    if (count == 0) {
        free(pieces);
        return URSATZ_OK;
    }

    qsort(pieces, count, sizeof(UrsatzGuiPieceSummary), compare_pieces);
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
static bool piece_dir_path(const char *pieces_dir, const char *id, char *out, size_t out_size) {
    if (!is_valid_piece_id(id)) {
        return false;
    }
    if (snprintf(out, out_size, "%s/%s", pieces_dir, id) >= (int)out_size) {
        return false;
    }
    return common_path_is_dir(out);
}

static ImportedFormat format_from_label(const char *label) {
    if (label && strcmp(label, "midi") == 0) {
        return IMPORTED_FORMAT_MIDI;
    }
    if (label && strcmp(label, "musicxml") == 0) {
        return IMPORTED_FORMAT_MUSICXML;
    }
    return IMPORTED_FORMAT_AUTO;
}

/* Writes the path of the piece's stored source file into out. */
static bool find_source_path(const char *dir_path, char *out, size_t out_size) {
    static const char *const kNames[] = {"source.mid", "source.musicxml", "source.xml", "source.mxl"};
    for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); i++) {
        snprintf(out, out_size, "%s/%s", dir_path, kNames[i]);
        if (common_path_exists(out)) {
            return true;
        }
    }
    return false;
}

/* Imports the piece's stored source and applies the notation override kept in
   its meta.json, if any (R-110). The analysis, the score and playback all
   read the piece through this, so they agree on its meter and note values. */
static bool import_piece(const char *dir_path, ImportedFormat format, ImportedScore *out) {
    char source_path[400];
    if (!find_source_path(dir_path, source_path, sizeof(source_path)) ||
        analyzer_import_file(source_path, format, out, NULL) != URSATZ_OK) {
        return false;
    }
    cJSON *meta = read_meta_file(dir_path);
    UrsatzGuiNotation notation;
    bool has_notation = meta && meta_notation_read(meta, &notation);
    cJSON_Delete(meta);
    if (!has_notation) {
        return true;
    }
    ImportedReading reading = imported_reading_none();
    if ((notation.meter[0] != '\0' && !imported_reading_parse_meter(notation.meter, &reading)) ||
        (notation.value_scale[0] != '\0' && !imported_reading_parse_scale(notation.value_scale, &reading)) ||
        (notation.pickup[0] != '\0' && !imported_reading_parse_pickup(notation.pickup, &reading)) ||
        imported_reading_apply(out, &reading) != IMPORTED_READING_OK) {
        imported_score_destroy(out);
        return false;
    }
    return true;
}

/* The size and modification time of a file, as text for a cache version. */
static bool file_stamp(const char *path, char *out, size_t out_size) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }
    long nanoseconds = 0;
#if defined(__linux__)
    nanoseconds = (long)st.st_mtim.tv_nsec;
#elif defined(__APPLE__)
    nanoseconds = (long)st.st_mtimespec.tv_nsec;
#endif
    snprintf(out, out_size, "%lld/%lld.%ld", (long long)st.st_size, (long long)st.st_mtime, nanoseconds);
    return true;
}

typedef struct {
    const char *dir_path;
    ImportedFormat format;
} ImportRequest;

static bool import_piece_for_cache(void *context, ImportedScore *out) {
    const ImportRequest *request = context;
    return import_piece(request->dir_path, request->format, out);
}

/* The cached import of the piece (import_cache.h), made when there is none for the current notation and source
   file. Called with the piece lock held; release the entry with ursatz_gui_import_cache_release. NULL when the
   source does not import (*out_failed true) or memory ran out. */
static UrsatzGuiImportEntry *acquire_import(const char *dir_path, const char *id, ImportedFormat format,
                                                bool *out_failed) {
    *out_failed = true;
    char source_path[400];
    char stamp[96];
    if (!find_source_path(dir_path, source_path, sizeof(source_path)) ||
        !file_stamp(source_path, stamp, sizeof(stamp))) {
        return NULL;
    }
    UrsatzGuiNotation notation;
    cJSON *meta = read_meta_file(dir_path);
    bool has_notation = meta && meta_notation_read(meta, &notation);
    cJSON_Delete(meta);
    if (!has_notation) {
        memset(&notation, 0, sizeof(notation));
    }
    char version[768];
    snprintf(version, sizeof(version), "%d|%s|%d|%s|%s|%s|%s", (int)format, source_path, has_notation ? 1 : 0,
             notation.meter, notation.value_scale, notation.pickup, stamp);
    ImportRequest request = {dir_path, format};
    UrsatzGuiImportEntry *entry = ursatz_gui_import_cache_acquire(id, version, import_piece_for_cache, &request,
                                                                   out_failed);
    return entry;
}

static UrsatzStatus ensure_current_unlocked(const char *pieces_dir, const char *id);

static UrsatzStatus get_piece_unlocked(const char *pieces_dir, const char *id,
                                                        UrsatzGuiPieceDetail **out_detail) {
    /* A stored analysis from another schema, Analyzer or Ursatz is analyzed again, once. When that fails the stored
       document is served and its status says why it is stale. */
    if (ensure_current_unlocked(pieces_dir, id) == URSATZ_ERR_NOT_FOUND) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = read_meta_file(dir_path);
    if (!meta) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }

    UrsatzGuiPieceDetail *detail = calloc(1, sizeof(UrsatzGuiPieceDetail));
    if (!detail) {
        cJSON_Delete(meta);
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }
    detail->id = common_strdup(id);
    detail->filename = meta_string_copy(meta, "filename");
    detail->format = meta_string_copy(meta, "format");
    detail->uploaded_at = meta_string_copy(meta, "uploaded_at");
    detail->has_notation = meta_notation_read(meta, &detail->notation);
    cJSON_Delete(meta);

    char analysis_path[400];
    snprintf(analysis_path, sizeof(analysis_path), "%s/analysis.json", dir_path);
    common_file_read(analysis_path, &detail->document_json, NULL, NULL);
    detail->analysis_status = status_name(detail->document_json ? analyzer_document_status(detail->document_json)
                                                                 : ANALYZER_DOCUMENT_UNREADABLE);

    if (!detail->id || !detail->filename || !detail->format || !detail->uploaded_at || !detail->document_json) {
        ursatz_gui_piece_detail_destroy(detail);
        return ursatz_gui_fail(URSATZ_ERR_IO, NULL);
    }

    /* The note ids come from importing the stored source again: they are the
       ids of the source's own NoteEvents (a notation override keeps them), in
       the Analyzer's event order. */
    bool import_failed = false;
    UrsatzGuiImportEntry *import = acquire_import(dir_path, id, format_from_label(detail->format), &import_failed);
    if (!import) {
        ursatz_gui_piece_detail_destroy(detail);
        return import_failed ? ursatz_gui_fail(URSATZ_ERR_FORMAT, NULL) : ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }
    char **ids = NULL;
    size_t id_count = 0;
    UrsatzStatus ids_status = analyzer_score_note_xml_ids(ursatz_gui_import_entry_notes(import), &ids, &id_count, NULL);
    ursatz_gui_import_cache_release(import);
    if (ids_status != URSATZ_OK) {
        ursatz_gui_piece_detail_destroy(detail);
        return ids_status == URSATZ_ERR_ALLOC ? ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL) : ursatz_gui_fail(URSATZ_ERR_FORMAT, NULL);
    }
    detail->note_xml_ids = id_count > 0 ? calloc(id_count, sizeof(char *)) : NULL;
    bool ok = id_count == 0 || detail->note_xml_ids != NULL;
    for (size_t i = 0; ok && i < id_count; i++) {
        detail->note_xml_ids[i] = common_strdup(ids[i]);
        if (!detail->note_xml_ids[i]) {
            ok = false;
        }
        detail->note_count = i + 1;
    }
    analyzer_score_note_xml_ids_destroy(ids, id_count);
    if (!ok) {
        ursatz_gui_piece_detail_destroy(detail);
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }

    *out_detail = detail;
    return URSATZ_OK;
}

static UrsatzStatus delete_piece_unlocked(const char *pieces_dir, const char *id) {
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    ursatz_gui_import_cache_invalidate(id);
    common_dir_remove(dir_path, NULL);
    return common_path_is_dir(dir_path) ? ursatz_gui_fail(URSATZ_ERR_IO, NULL) : URSATZ_OK;
}

/* Sets a string member of meta in place (keeping its position), adding it
   when absent. */
static bool meta_set_string(cJSON *meta, const char *key, const char *value) {
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

static bool meta_set_number(cJSON *meta, const char *key, double value) {
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
    char *value = ursatz_gui_meta_normalize_value(text, &too_long);
    if (!value) {
        if (too_long) {
            char reason[48];
            snprintf(reason, sizeof(reason), "%s is too long", key);
            ursatz_gui_error_text(error, reason);
            return ursatz_gui_fail(URSATZ_ERR_REQUEST, NULL);
        }
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }
    bool ok = meta_set_string(meta, key, value);
    free(value);
    return ok ? URSATZ_OK : ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
}

/* Normalizes each of the given tags and adds it to (or removes it from) the list. */
static UrsatzStatus tags_apply(char ***tags, size_t *count, const char *const *items, size_t item_count,
                                          bool remove, UrsatzError *error) {
    for (size_t i = 0; i < item_count; i++) {
        bool invalid = false;
        char *tag = ursatz_gui_meta_normalize_tag(items[i], &invalid);
        if (!tag) {
            if (invalid) {
                ursatz_gui_error_text(error, "invalid tag");
                return ursatz_gui_fail(URSATZ_ERR_REQUEST, NULL);
            }
            return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
        }
        UrsatzStatus result =
            remove ? ursatz_gui_meta_tags_remove(tags, count, tag) : ursatz_gui_meta_tags_add(tags, count, tag);
        free(tag);
        if (result == URSATZ_ERR_REQUEST) {
            ursatz_gui_error_text(error, "too many tags");
            return ursatz_gui_fail(URSATZ_ERR_REQUEST, NULL);
        }
        if (result != URSATZ_OK) {
            return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
        }
    }
    return URSATZ_OK;
}

static UrsatzStatus update_metadata_unlocked(const char *pieces_dir, const char *id,
                                                             const UrsatzGuiMetaUpdate *update, UrsatzError *error) {
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = read_meta_file(dir_path);
    if (!meta) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    ursatz_gui_import_cache_invalidate(id);

    UrsatzStatus result = URSATZ_OK;
    if (update->filename && !meta_set_string(meta, "filename", update->filename)) {
        result = ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
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
            result = ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
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
            result = ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
        }
        ursatz_gui_meta_tags_free(tags, tag_count);
    }
    if (result == URSATZ_OK && update->has_notation) {
        if (update->notation && !notation_is_valid(update->notation)) {
            ursatz_gui_error_text(error, "invalid notation");
            result = ursatz_gui_fail(URSATZ_ERR_REQUEST, NULL);
        } else if (!meta_set_notation(meta, update->notation)) {
            result = ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
        }
    }
    if (result != URSATZ_OK) {
        cJSON_Delete(meta);
        return result;
    }

    char *text = cJSON_Print(meta);
    cJSON_Delete(meta);
    if (!text) {
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }
    char meta_path[400];
    snprintf(meta_path, sizeof(meta_path), "%s/meta.json", dir_path);
    bool ok = (common_file_write_atomic(meta_path, text, strlen(text), NULL) == URSATZ_OK);
    free(text);
    return ok ? URSATZ_OK : ursatz_gui_fail(URSATZ_ERR_IO, NULL);
}

UrsatzStatus ursatz_gui_analysis_get_notation(const char *pieces_dir, const char *id, bool *out_has,
                                                          UrsatzGuiNotation *out) {
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = read_meta_file(dir_path);
    if (!meta) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    *out_has = meta_notation_read(meta, out);
    cJSON_Delete(meta);
    return URSATZ_OK;
}

static UrsatzStatus reanalyze_unlocked(const char *pieces_dir, const char *id) {
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = read_meta_file(dir_path);
    if (!meta) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    ursatz_gui_import_cache_invalidate(id);
    const cJSON *format_item = cJSON_GetObjectItemCaseSensitive(meta, "format");
    ImportedFormat format = format_from_label(cJSON_IsString(format_item) ? format_item->valuestring : NULL);

    char *document_json = NULL;
    UrsatzStatus result = analyze_source_file(dir_path, format, &document_json);
    if (result != URSATZ_OK) {
        cJSON_Delete(meta);
        return result;
    }

    char analysis_path[400];
    snprintf(analysis_path, sizeof(analysis_path), "%s/analysis.json", dir_path);
    bool ok = (common_file_write_atomic(analysis_path, document_json, strlen(document_json), NULL) == URSATZ_OK);
    free(document_json);

    ok = ok && meta_set_string(meta, "analyzer_version", analyzer_version_string()) &&
         meta_set_number(meta, "document_schema_version", ANALYZER_DOCUMENT_SCHEMA_VERSION) &&
         meta_set_string(meta, "ursatz_commit", analyzer_producer_key());
    char *text = ok ? cJSON_Print(meta) : NULL;
    cJSON_Delete(meta);
    if (!text) {
        return ursatz_gui_fail(URSATZ_ERR_IO, NULL);
    }
    char meta_path[400];
    snprintf(meta_path, sizeof(meta_path), "%s/meta.json", dir_path);
    ok = (common_file_write_atomic(meta_path, text, strlen(text), NULL) == URSATZ_OK);
    free(text);
    return ok ? URSATZ_OK : ursatz_gui_fail(URSATZ_ERR_IO, NULL);
}

/* Appends text to *out (grown with realloc) with the XML characters escaped. */
/* A meta.json without ursatz_commit gets it from the stored document's metadata.producer_key, written back (meta.json
   keeps the member name ursatz_commit). Without either, meta.json stays as it is. */
static void meta_backfill_ursatz_commit(const char *dir_path, cJSON *meta, const char *document_json) {
    if (!document_json || cJSON_IsString(cJSON_GetObjectItemCaseSensitive(meta, "ursatz_commit"))) {
        return;
    }
    cJSON *document = cJSON_Parse(document_json);
    const cJSON *metadata = document ? cJSON_GetObjectItemCaseSensitive(document, "metadata") : NULL;
    const cJSON *commit = metadata ? cJSON_GetObjectItemCaseSensitive(metadata, "producer_key") : NULL;
    if (cJSON_IsString(commit) && meta_set_string(meta, "ursatz_commit", commit->valuestring)) {
        char *text = cJSON_Print(meta);
        if (text) {
            char meta_path[400];
            snprintf(meta_path, sizeof(meta_path), "%s/meta.json", dir_path);
            (common_file_write_atomic(meta_path, text, strlen(text), NULL) == URSATZ_OK);
            free(text);
        }
    }
    cJSON_Delete(document);
}

/* Makes the stored analysis current: fills a missing ursatz_commit in meta.json, then analyzes the piece again when
   analyzer_document_status says the stored document is not CURRENT. OK when it is current now; the failure of the
   new analysis otherwise, with the stored files unchanged. */
static UrsatzStatus ensure_current_unlocked(const char *pieces_dir, const char *id) {
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = read_meta_file(dir_path);
    if (!meta) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    char analysis_path[400];
    snprintf(analysis_path, sizeof(analysis_path), "%s/analysis.json", dir_path);
    char *document = NULL;
    common_file_read(analysis_path, &document, NULL, NULL);
    meta_backfill_ursatz_commit(dir_path, meta, document);
    cJSON_Delete(meta);
    AnalyzerDocumentStatus status = document ? analyzer_document_status(document) : ANALYZER_DOCUMENT_UNREADABLE;
    free(document);
    if (status == ANALYZER_DOCUMENT_CURRENT) {
        return URSATZ_OK;
    }
    return reanalyze_unlocked(pieces_dir, id);
}

static UrsatzStatus get_piece_musicxml_unlocked(const char *pieces_dir, const char *id,
                                                                char **out_xml, bool *out_meter_defaulted) {
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = read_meta_file(dir_path);
    if (!meta) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    const cJSON *format_item = cJSON_GetObjectItemCaseSensitive(meta, "format");
    ImportedFormat format = format_from_label(cJSON_IsString(format_item) ? format_item->valuestring : NULL);
    /* The Library shows the filename without its extension as the title. */
    char *title = meta_string_copy(meta, "filename");
    char *composer = meta_string_copy(meta, "composer");
    cJSON_Delete(meta);
    if (!title || !composer) {
        free(title);
        free(composer);
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }
    char *title_dot = strrchr(title, '.');
    if (title_dot && title_dot != title) {
        *title_dot = '\0';
    }

    bool import_failed = false;
    UrsatzGuiImportEntry *import = acquire_import(dir_path, id, format, &import_failed);
    if (!import) {
        free(title);
        free(composer);
        return import_failed ? ursatz_gui_fail(URSATZ_ERR_FORMAT, NULL) : ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }

    bool meter_defaulted = false;
    const char *cached = ursatz_gui_import_entry_score(import, &meter_defaulted);
    char *xml = cached ? common_strdup(cached) : NULL;
    if (cached) {
        ursatz_gui_import_cache_release(import);
        free(title);
        free(composer);
        if (!xml) {
            return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
        }
    } else {
        const ImportedScore *notes = ursatz_gui_import_entry_notes(import);
        AnalyzerScoreMetadata metadata = {title, composer};
        UrsatzStatus status = analyzer_score_export_musicxml(notes, &metadata, &xml, NULL);
        meter_defaulted = analyzer_imported_meter_defaulted(notes);

        if (status != URSATZ_OK) {
            ursatz_gui_import_cache_release(import);
            fprintf(stderr, "ursatz_gui_analysis_get_piece_musicxml: analyzer_score_export_musicxml failed for piece "
                             "'%s' with UrsatzStatus=%d\n",
                    id, (int)status);
            free(title);
            free(composer);
            return ursatz_gui_fail(status == URSATZ_ERR_ALLOC || status == URSATZ_ERR_REQUEST ? status : URSATZ_ERR_INTERNAL, NULL);
        }

        free(title);
        free(composer);
        char *kept = xml ? common_strdup(xml) : NULL;
        if (kept) {
            ursatz_gui_import_entry_set_score(import, kept, meter_defaulted);
        }
        ursatz_gui_import_cache_release(import);
        if (!xml) {
            return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
        }
    }

    *out_xml = xml;
    *out_meter_defaulted = meter_defaulted;
    return URSATZ_OK;
}

static UrsatzStatus get_piece_meter_defaulted_unlocked(const char *pieces_dir, const char *id,
                                                                       bool *out_meter_defaulted) {
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = read_meta_file(dir_path);
    if (!meta) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    const cJSON *format_item = cJSON_GetObjectItemCaseSensitive(meta, "format");
    ImportedFormat format = format_from_label(cJSON_IsString(format_item) ? format_item->valuestring : NULL);
    cJSON_Delete(meta);

    bool import_failed = false;
    UrsatzGuiImportEntry *import = acquire_import(dir_path, id, format, &import_failed);
    if (!import) {
        return import_failed ? ursatz_gui_fail(URSATZ_ERR_FORMAT, NULL) : ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }
    *out_meter_defaulted = analyzer_imported_meter_defaulted(ursatz_gui_import_entry_notes(import));
    ursatz_gui_import_cache_release(import);
    return URSATZ_OK;
}

static UrsatzStatus get_piece_playback_unlocked(const char *pieces_dir, const char *id,
                                                                char **out_json) {
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = read_meta_file(dir_path);
    if (!meta) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    const cJSON *format_item = cJSON_GetObjectItemCaseSensitive(meta, "format");
    ImportedFormat format = format_from_label(cJSON_IsString(format_item) ? format_item->valuestring : NULL);
    cJSON_Delete(meta);

    bool import_failed = false;
    UrsatzGuiImportEntry *import = acquire_import(dir_path, id, format, &import_failed);
    if (!import) {
        return import_failed ? ursatz_gui_fail(URSATZ_ERR_FORMAT, NULL) : ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }
    const ImportedScore *notes = ursatz_gui_import_entry_notes(import);

    AnalyzerPlayback playback;
    UrsatzStatus status = analyzer_playback_build(notes, &playback);
    char *json = NULL;
    if (status == URSATZ_OK) {
        status = analyzer_playback_to_json(&playback, notes, &json);
        analyzer_playback_destroy(&playback);
    }
    ursatz_gui_import_cache_release(import);
    if (status == URSATZ_ERR_ALLOC) {
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }
    if (status != URSATZ_OK) {
        /* A source that imported a moment ago and now has no readable tempo is a failed import. */
        return ursatz_gui_fail(URSATZ_ERR_FORMAT, NULL);
    }
    *out_json = json;
    return URSATZ_OK;
}

static UrsatzStatus get_piece_export_unlocked(const char *pieces_dir, const char *id, int first_measure,
                                                              int last_measure, char **out_text) {
    UrsatzStatus current = ensure_current_unlocked(pieces_dir, id);
    if (current == URSATZ_ERR_NOT_FOUND) {
        return current;
    }
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = read_meta_file(dir_path);
    if (!meta) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    const cJSON *format_item = cJSON_GetObjectItemCaseSensitive(meta, "format");
    ImportedFormat format = format_from_label(cJSON_IsString(format_item) ? format_item->valuestring : NULL);
    char *title = meta_string_copy(meta, "filename");
    cJSON_Delete(meta);
    if (!title) {
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }
    char *title_dot = strrchr(title, '.');
    if (title_dot && title_dot != title) {
        *title_dot = '\0';
    }

    char analysis_path[400];
    snprintf(analysis_path, sizeof(analysis_path), "%s/analysis.json", dir_path);
    char *document_json = NULL;
    common_file_read(analysis_path, &document_json, NULL, NULL);
    if (!document_json) {
        free(title);
        return common_path_exists(dir_path) ? ursatz_gui_fail(URSATZ_ERR_IO, NULL)
                                                : ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }

    bool import_failed = false;
    UrsatzGuiImportEntry *import = acquire_import(dir_path, id, format, &import_failed);
    if (!import) {
        free(title);
        free(document_json);
        return import_failed ? ursatz_gui_fail(URSATZ_ERR_FORMAT, NULL) : ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }

    AnalyzerTextExportOptions options = analyzer_text_export_options_default();
    options.title = title;
    options.first_measure = first_measure;
    options.last_measure = last_measure;
    char *text = NULL;
    UrsatzStatus status =
        analyzer_text_export_piece(ursatz_gui_import_entry_notes(import), document_json, &options, &text, NULL);
    ursatz_gui_import_cache_release(import);
    free(title);
    free(document_json);
    if (status == URSATZ_ERR_ALLOC) {
        return ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
    }
    if (status != URSATZ_OK) {
        fprintf(stderr, "ursatz_gui_analysis_get_piece_export: analyzer_text_export_piece failed for piece '%s' "
                         "with UrsatzStatus=%d\n",
                id, (int)status);
        return ursatz_gui_fail(status == URSATZ_ERR_REQUEST ? URSATZ_ERR_REQUEST : URSATZ_ERR_INTERNAL, NULL);
    }
    *out_text = text;
    return URSATZ_OK;
}

bool ursatz_gui_document_subset(const char *document_json, const char *const *names, size_t count,
                                char **out_json, char **out_names_json) {
    cJSON *document = cJSON_Parse(document_json);
    if (!document || !cJSON_IsObject(document)) {
        cJSON_Delete(document);
        return false;
    }
    cJSON *subset = cJSON_CreateObject();
    cJSON *all_names = cJSON_CreateArray();
    bool ok = subset && all_names;
    for (const cJSON *item = document->child; ok && item; item = item->next) {
        ok = item->string && cJSON_AddItemToArray(all_names, cJSON_CreateString(item->string));
    }
    for (size_t i = 0; ok && i < count; i++) {
        cJSON *value = cJSON_DetachItemFromObjectCaseSensitive(document, names[i]);
        if (value && !cJSON_AddItemToObject(subset, names[i], value)) {
            cJSON_Delete(value);
            ok = false;
        }
    }
    char *json = ok ? cJSON_PrintUnformatted(subset) : NULL;
    char *names_json = ok && out_names_json ? cJSON_PrintUnformatted(all_names) : NULL;
    ok = json && (!out_names_json || names_json);
    cJSON_Delete(subset);
    cJSON_Delete(all_names);
    cJSON_Delete(document);
    if (!ok) {
        free(json);
        free(names_json);
        return false;
    }
    *out_json = json;
    if (out_names_json) {
        *out_names_json = names_json;
    }
    return true;
}

static UrsatzStatus get_document_unlocked(const char *pieces_dir, const char *id,
                                                          const char *const *names, size_t count, char **out_json) {
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    char analysis_path[400];
    snprintf(analysis_path, sizeof(analysis_path), "%s/analysis.json", dir_path);
    char *document_json = NULL;
    common_file_read(analysis_path, &document_json, NULL, NULL);
    if (!document_json) {
        return common_path_exists(dir_path) ? ursatz_gui_fail(URSATZ_ERR_IO, NULL) : ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    if (!names) {
        *out_json = document_json;
        return URSATZ_OK;
    }
    bool ok = ursatz_gui_document_subset(document_json, names, count, out_json, NULL);
    free(document_json);
    return ok ? URSATZ_OK : ursatz_gui_fail(URSATZ_ERR_IO, NULL);
}

void ursatz_gui_piece_detail_destroy(UrsatzGuiPieceDetail *detail) {
    if (!detail) {
        return;
    }
    for (size_t i = 0; i < detail->note_count; i++) {
        free(detail->note_xml_ids[i]);
    }
    free(detail->note_xml_ids);
    free(detail->document_json);
    free(detail->id);
    free(detail->filename);
    free(detail->format);
    free(detail->uploaded_at);
    free(detail);
}

/* The public operations on one piece. Each takes the piece lock (piece_lock.h) around the whole operation, so the
   writers of a piece directory never interleave and a read never sees a half-written file. They call only the
   _unlocked functions above, never each other. */

static bool piece_lock_enter(const char *id, UrsatzStatus *failure) {
    if (!is_valid_piece_id(id)) {
        *failure = URSATZ_ERR_NOT_FOUND;
        return false;
    }
    if (!ursatz_gui_piece_lock(id)) {
        *failure = URSATZ_ERR_ALLOC;
        return false;
    }
    return true;
}

UrsatzStatus ursatz_gui_analysis_get_piece(const char *pieces_dir, const char *id,
                                                        UrsatzGuiPieceDetail **out_detail) {
    UrsatzStatus result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    result = get_piece_unlocked(pieces_dir, id, out_detail);
    ursatz_gui_piece_unlock(id);
    return result;
}

UrsatzStatus ursatz_gui_analysis_get_document(const char *pieces_dir, const char *id,
                                                          const char *const *names, size_t count, char **out_json) {
    UrsatzStatus result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    result = get_document_unlocked(pieces_dir, id, names, count, out_json);
    ursatz_gui_piece_unlock(id);
    return result;
}

bool ursatz_gui_analysis_piece_exists(const char *pieces_dir, const char *id) {
    char dir_path[320];
    char meta_path[400];
    return piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path)) &&
           snprintf(meta_path, sizeof(meta_path), "%s/meta.json", dir_path) < (int)sizeof(meta_path) &&
           common_path_exists(meta_path);
}

UrsatzStatus ursatz_gui_analysis_get_filename(const char *pieces_dir, const char *id, char **out_filename) {
    UrsatzStatus result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    char dir_path[320];
    cJSON *meta = piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path)) ? read_meta_file(dir_path) : NULL;
    if (!meta) {
        ursatz_gui_piece_unlock(id);
        return ursatz_gui_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    *out_filename = meta_string_copy(meta, "filename");
    cJSON_Delete(meta);
    ursatz_gui_piece_unlock(id);
    return *out_filename ? URSATZ_OK : ursatz_gui_fail(URSATZ_ERR_ALLOC, NULL);
}

UrsatzStatus ursatz_gui_analysis_delete_piece(const char *pieces_dir, const char *id) {
    UrsatzStatus result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    result = delete_piece_unlocked(pieces_dir, id);
    ursatz_gui_piece_unlock(id);
    return result;
}

UrsatzStatus ursatz_gui_analysis_reanalyze(const char *pieces_dir, const char *id) {
    UrsatzStatus result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    result = reanalyze_unlocked(pieces_dir, id);
    ursatz_gui_piece_unlock(id);
    return result;
}

UrsatzStatus ursatz_gui_analysis_ensure_current(const char *pieces_dir, const char *id) {
    UrsatzStatus result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    result = ensure_current_unlocked(pieces_dir, id);
    ursatz_gui_piece_unlock(id);
    return result;
}

UrsatzStatus ursatz_gui_analysis_get_piece_musicxml(const char *pieces_dir, const char *id,
                                                                char **out_xml, bool *out_meter_defaulted) {
    UrsatzStatus result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    result = get_piece_musicxml_unlocked(pieces_dir, id, out_xml, out_meter_defaulted);
    ursatz_gui_piece_unlock(id);
    return result;
}

UrsatzStatus ursatz_gui_analysis_get_piece_meter_defaulted(const char *pieces_dir, const char *id,
                                                                       bool *out_meter_defaulted) {
    UrsatzStatus result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    result = get_piece_meter_defaulted_unlocked(pieces_dir, id, out_meter_defaulted);
    ursatz_gui_piece_unlock(id);
    return result;
}

UrsatzStatus ursatz_gui_analysis_get_piece_playback(const char *pieces_dir, const char *id,
                                                                char **out_json) {
    UrsatzStatus result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    result = get_piece_playback_unlocked(pieces_dir, id, out_json);
    ursatz_gui_piece_unlock(id);
    return result;
}

UrsatzStatus ursatz_gui_analysis_get_piece_export(const char *pieces_dir, const char *id,
                                                              int first_measure, int last_measure, char **out_text) {
    UrsatzStatus result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    result = get_piece_export_unlocked(pieces_dir, id, first_measure, last_measure, out_text);
    ursatz_gui_piece_unlock(id);
    return result;
}

static bool notation_equal(bool has_a, const UrsatzGuiNotation *a, bool has_b, const UrsatzGuiNotation *b) {
    if (has_a != has_b) {
        return false;
    }
    return !has_a || (strcmp(a->meter, b->meter) == 0 && strcmp(a->value_scale, b->value_scale) == 0 &&
                      strcmp(a->pickup, b->pickup) == 0);
}

UrsatzStatus ursatz_gui_analysis_update_piece(const char *pieces_dir, const char *id,
                                                          const UrsatzGuiMetaUpdate *update, UrsatzError *error) {
    UrsatzStatus result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    bool had_notation = false;
    UrsatzGuiNotation previous;
    memset(&previous, 0, sizeof(previous));
    if (update->has_notation) {
        ursatz_gui_analysis_get_notation(pieces_dir, id, &had_notation, &previous);
    }
    result = update_metadata_unlocked(pieces_dir, id, update, error);
    if (result == URSATZ_OK && update->has_notation &&
        !notation_equal(had_notation, &previous, update->notation != NULL,
                        update->notation ? update->notation : &previous)) {
        result = reanalyze_unlocked(pieces_dir, id);
        if (result != URSATZ_OK) {
            /* The piece keeps its old analysis; put the old reading back to match it. */
            UrsatzGuiMetaUpdate undo = {0};
            undo.has_notation = true;
            undo.notation = had_notation ? &previous : NULL;
            update_metadata_unlocked(pieces_dir, id, &undo, NULL);
            if (result == URSATZ_ERR_FORMAT) {
                ursatz_gui_error_text(error, "invalid notation");
                result = ursatz_gui_fail(URSATZ_ERR_REQUEST, NULL);
            }
        }
    }
    ursatz_gui_piece_unlock(id);
    return result;
}
