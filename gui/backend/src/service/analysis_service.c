#include "analysis_service.h"
#include "util/fs_util.h"
#include "id_service.h"
#include "piece_metadata.h"
#include "piece_lock.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cJSON.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/notation_reading.h>
#include <ursatz_analyzer/playback.h>
#include <ursatz_analyzer/score_export.h>
#include <ursatz_analyzer/version.h>

static bool path_exists(const char *path) {
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}

#define PIECE_ID_MAX_TRIES 8

/* Creates one directory. Returns 1 if created, 0 if it already exists, -1 on
   any other failure. */
/* Removes every file in a piece directory, then the directory. */
static void remove_piece_dir(const char *dir_path) {
#ifdef _WIN32
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir_path);
    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = FindFirstFileA(pattern, &find_data);
    if (find_handle != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(find_data.cFileName, ".") != 0 && strcmp(find_data.cFileName, "..") != 0) {
                char file_path[400];
                snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, find_data.cFileName);
                remove(file_path);
            }
        } while (FindNextFileA(find_handle, &find_data) != 0);
        FindClose(find_handle);
    }
    _rmdir(dir_path);
#else
    DIR *dir = opendir(dir_path);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                char file_path[400];
                snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);
                remove(file_path);
            }
        }
        closedir(dir);
    }
    rmdir(dir_path);
#endif
}

static const char *format_label(InputFormat format) {
    return format == INPUT_FORMAT_MIDI ? "midi" : "musicxml";
}

static const char *analyzer_version_string(void) {
    return ursatz_analyzer_version();
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
    cJSON_AddNumberToObject(meta, "document_schema_version", URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION);
    char *text = cJSON_Print(meta);
    cJSON_Delete(meta);
    if (!text) {
        return false;
    }
    char meta_path[400];
    snprintf(meta_path, sizeof(meta_path), "%s/meta.json", dir_path);
    bool ok = ursatz_gui_write_file_atomic(meta_path, text, strlen(text));
    free(text);
    return ok;
}

/* Parses <dir_path>/meta.json; NULL when it is missing or not an object. */
static cJSON *read_meta_file(const char *dir_path) {
    char meta_path[400];
    snprintf(meta_path, sizeof(meta_path), "%s/meta.json", dir_path);
    char *text = ursatz_gui_read_file(meta_path, NULL);
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
    return ursatz_gui_dup_string(cJSON_IsString(item) && item->valuestring ? item->valuestring : "");
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
        tags[count] = ursatz_gui_dup_string(item->valuestring);
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
    NotationReading reading = notation_reading_none();
    return (n->meter[0] == '\0' || notation_reading_parse_meter(n->meter, &reading)) &&
           (n->value_scale[0] == '\0' || notation_reading_parse_scale(n->value_scale, &reading)) &&
           (n->pickup[0] == '\0' || notation_reading_parse_pickup(n->pickup, &reading));
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
static const char *source_file_name(InputFormat format, const char *original_filename) {
    if (format == INPUT_FORMAT_MIDI) {
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
static bool import_piece(const char *dir_path, InputFormat format, ImportedNotes *out);

/* Imports the piece's source (with its notation override, if any) and runs it
   through the Analyzer. On OK, *out_json is the analysis document (caller frees). */
static UrsatzGuiAnalysisResult analyze_source_file(const char *dir_path, InputFormat format, char **out_json) {
    char source_path[400];
    ImportedNotes notes;
    if (!find_source_path(dir_path, source_path, sizeof(source_path)) || !import_piece(dir_path, format, &notes)) {
        return URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED;
    }

    AnalysisReport report;
    if (!run_analysis_pipeline(&notes, &report)) {
        imported_notes_destroy(&notes);
        return URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED;
    }

    AnalysisDocument *document = analysis_document_build(source_path, &notes, &report);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    if (!document) {
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }
    char *json = analysis_document_to_json(document);
    analysis_document_destroy(document);
    if (!json) {
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }
    *out_json = json;
    return URSATZ_GUI_ANALYSIS_OK;
}

/* Creates pieces_dir/<id>/ with a fresh id, re-drawing while the directory
   exists. Returns OK, ERR_ALLOC or ERR_PERSISTENCE_FAILED. */
static UrsatzGuiAnalysisResult create_piece_dir(const char *pieces_dir, char **out_id) {
    for (int attempt = 0; attempt < PIECE_ID_MAX_TRIES; attempt++) {
        char *id = ursatz_gui_new_id("piece");
        if (!id) {
            return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
        }
        char dir_path[320];
        snprintf(dir_path, sizeof(dir_path), "%s/%s", pieces_dir, id);
        bool made = ursatz_gui_make_dir(dir_path);
        int made_errno = errno;
        if (made) {
            *out_id = id;
            return URSATZ_GUI_ANALYSIS_OK;
        }
        free(id);
        if (made_errno != EEXIST) {
            return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
        }
    }
    return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
}

/* Writes the upload into the new piece directory pieces_dir/<id>/ and analyzes it. On failure the directory is
   removed. The caller holds the piece lock. */
static UrsatzGuiAnalysisResult save_new_piece(const char *pieces_dir, const char *id, const unsigned char *data,
                                              size_t size, const char *original_filename) {
    UrsatzGuiAnalysisResult result;
    char dir_path[320];
    snprintf(dir_path, sizeof(dir_path), "%s/%s", pieces_dir, id);

    /* The bytes go to disk unchanged under a neutral name first: the format
       is detected from the content, and the final name depends on it. */
    char upload_path[400];
    snprintf(upload_path, sizeof(upload_path), "%s/source.upload", dir_path);
    if (!ursatz_gui_write_file_atomic(upload_path, data, size)) {
        remove_piece_dir(dir_path);
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }

    InputFormat format = input_format_detect(upload_path);
    if (format == INPUT_FORMAT_AUTO) {
        remove_piece_dir(dir_path);
        return URSATZ_GUI_ANALYSIS_ERR_UNSUPPORTED_FORMAT;
    }

    char source_path[400];
    snprintf(source_path, sizeof(source_path), "%s/%s", dir_path, source_file_name(format, original_filename));
    if (rename(upload_path, source_path) != 0) {
        remove_piece_dir(dir_path);
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }

    char *document_json = NULL;
    result = analyze_source_file(dir_path, format, &document_json);
    if (result != URSATZ_GUI_ANALYSIS_OK) {
        remove_piece_dir(dir_path);
        return result;
    }

    char analysis_path[400];
    snprintf(analysis_path, sizeof(analysis_path), "%s/analysis.json", dir_path);
    bool written = ursatz_gui_write_file_atomic(analysis_path, document_json, strlen(document_json));
    free(document_json);
    if (!written) {
        remove_piece_dir(dir_path);
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }

    char uploaded_at[32];
    ursatz_gui_iso_utc_now(uploaded_at, sizeof(uploaded_at));
    if (!write_meta_file(dir_path, original_filename, format_label(format), uploaded_at)) {
        remove_piece_dir(dir_path);
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }

    return URSATZ_GUI_ANALYSIS_OK;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_import_and_save(const char *pieces_dir, const unsigned char *data,
                                                              size_t size, const char *original_filename,
                                                              char **out_id) {
    if (!ursatz_gui_ensure_dir(pieces_dir)) {
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }

    char *id = NULL;
    UrsatzGuiAnalysisResult result = create_piece_dir(pieces_dir, &id);
    if (result != URSATZ_GUI_ANALYSIS_OK) {
        return result;
    }

    /* The directory exists from here on, so a reader that lists pieces may open it before it is complete. */
    if (!ursatz_gui_piece_lock(id)) {
        free(id);
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }
    result = save_new_piece(pieces_dir, id, data, size, original_filename);
    ursatz_gui_piece_unlock(id);
    if (result != URSATZ_GUI_ANALYSIS_OK) {
        free(id);
        return result;
    }
    *out_id = id;
    return URSATZ_GUI_ANALYSIS_OK;
}

static void free_ids(char **ids, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(ids[i]);
    }
    free(ids);
}

/* Names of the entries of dir_path (without "." and ".."). A missing
   directory yields none. Returns false only on allocation failure. */
static bool list_dir_names(const char *dir_path, char ***out_names, size_t *out_count) {
    char **names = NULL;
    size_t count = 0;
    size_t capacity = 0;
    *out_names = NULL;
    *out_count = 0;

#ifdef _WIN32
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir_path);
    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = FindFirstFileA(pattern, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        return true;
    }
    bool more = true;
    const char *name = NULL;
    while (more) {
        name = find_data.cFileName;
#else
    DIR *dir = opendir(dir_path);
    if (!dir) {
        return true;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
#endif
        if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
            if (count == capacity) {
                size_t new_capacity = capacity == 0 ? 8 : capacity * 2;
                char **grown = realloc(names, sizeof(char *) * new_capacity);
                if (!grown) {
                    free_ids(names, count);
#ifdef _WIN32
                    FindClose(find_handle);
#else
                    closedir(dir);
#endif
                    return false;
                }
                names = grown;
                capacity = new_capacity;
            }
            names[count] = ursatz_gui_dup_string(name);
            if (!names[count]) {
                free_ids(names, count);
#ifdef _WIN32
                FindClose(find_handle);
#else
                closedir(dir);
#endif
                return false;
            }
            count++;
        }
#ifdef _WIN32
        more = FindNextFileA(find_handle, &find_data) != 0;
    }
    FindClose(find_handle);
#else
    }
    closedir(dir);
#endif
    *out_names = names;
    *out_count = count;
    return true;
}

/* Newest upload first, then id. */
static int compare_pieces(const void *left, const void *right) {
    const UrsatzGuiPieceSummary *a = left;
    const UrsatzGuiPieceSummary *b = right;
    int by_time = strcmp(b->uploaded_at, a->uploaded_at);
    return by_time != 0 ? by_time : strcmp(a->id, b->id);
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_list(const char *pieces_dir, UrsatzGuiPieceSummary **out_pieces,
                                                  size_t *out_count) {
    *out_pieces = NULL;
    *out_count = 0;

    char **names = NULL;
    size_t name_count = 0;
    if (!list_dir_names(pieces_dir, &names, &name_count)) {
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }

    UrsatzGuiPieceSummary *pieces = NULL;
    if (name_count > 0) {
        pieces = calloc(name_count, sizeof(UrsatzGuiPieceSummary));
        if (!pieces) {
            free_ids(names, name_count);
            return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
        }
    }

    /* Anything that is not a directory with a readable meta.json (an old
       "<id>.db" piece, a stray file) is not a piece. */
    size_t count = 0;
    bool failed = false;
    for (size_t i = 0; i < name_count && !failed; i++) {
        char dir_path[320];
        snprintf(dir_path, sizeof(dir_path), "%s/%s", pieces_dir, names[i]);
        if (!ursatz_gui_path_is_dir(dir_path)) {
            continue;
        }
        cJSON *meta = read_meta_file(dir_path);
        if (!meta) {
            continue;
        }
        UrsatzGuiPieceSummary *piece = &pieces[count];
        piece->id = ursatz_gui_dup_string(names[i]);
        piece->filename = meta_string_copy(meta, "filename");
        piece->format = meta_string_copy(meta, "format");
        piece->uploaded_at = meta_string_copy(meta, "uploaded_at");
        piece->composer = meta_string_copy(meta, "composer");
        piece->source = meta_string_copy(meta, "source");
        bool tags_ok = meta_tags_copy(meta, &piece->tags, &piece->tag_count);
        piece->has_notation = meta_notation_read(meta, &piece->notation);
        cJSON_Delete(meta);
        if (!piece->id || !piece->filename || !piece->format || !piece->uploaded_at || !piece->composer ||
            !piece->source || !tags_ok) {
            piece_summary_destroy_contents(piece);
            failed = true;
            break;
        }
        count++;
    }
    free_ids(names, name_count);

    if (failed) {
        for (size_t i = 0; i < count; i++) {
            piece_summary_destroy_contents(&pieces[i]);
        }
        free(pieces);
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }
    if (count == 0) {
        free(pieces);
        return URSATZ_GUI_ANALYSIS_OK;
    }

    qsort(pieces, count, sizeof(UrsatzGuiPieceSummary), compare_pieces);
    *out_pieces = pieces;
    *out_count = count;
    return URSATZ_GUI_ANALYSIS_OK;
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
    return ursatz_gui_path_is_dir(out);
}

static InputFormat format_from_label(const char *label) {
    if (label && strcmp(label, "midi") == 0) {
        return INPUT_FORMAT_MIDI;
    }
    if (label && strcmp(label, "musicxml") == 0) {
        return INPUT_FORMAT_MUSICXML;
    }
    return INPUT_FORMAT_AUTO;
}

/* Writes the path of the piece's stored source file into out. */
static bool find_source_path(const char *dir_path, char *out, size_t out_size) {
    static const char *const kNames[] = {"source.mid", "source.musicxml", "source.xml", "source.mxl"};
    for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); i++) {
        snprintf(out, out_size, "%s/%s", dir_path, kNames[i]);
        if (path_exists(out)) {
            return true;
        }
    }
    return false;
}

/* Imports the piece's stored source and applies the notation override kept in
   its meta.json, if any (R-110). The analysis, the score and playback all
   read the piece through this, so they agree on its meter and note values. */
static bool import_piece(const char *dir_path, InputFormat format, ImportedNotes *out) {
    char source_path[400];
    if (!find_source_path(dir_path, source_path, sizeof(source_path)) ||
        input_pipeline_import_file(source_path, format, out) != INPUT_PIPELINE_OK) {
        return false;
    }
    cJSON *meta = read_meta_file(dir_path);
    UrsatzGuiNotation notation;
    bool has_notation = meta && meta_notation_read(meta, &notation);
    cJSON_Delete(meta);
    if (!has_notation) {
        return true;
    }
    NotationReading reading = notation_reading_none();
    if ((notation.meter[0] != '\0' && !notation_reading_parse_meter(notation.meter, &reading)) ||
        (notation.value_scale[0] != '\0' && !notation_reading_parse_scale(notation.value_scale, &reading)) ||
        (notation.pickup[0] != '\0' && !notation_reading_parse_pickup(notation.pickup, &reading)) ||
        notation_reading_apply(out, &reading) != NOTATION_READING_OK) {
        imported_notes_destroy(out);
        return false;
    }
    return true;
}

static UrsatzGuiAnalysisResult get_piece_unlocked(const char *pieces_dir, const char *id,
                                                        UrsatzGuiPieceDetail **out_detail) {
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }
    cJSON *meta = read_meta_file(dir_path);
    if (!meta) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }

    UrsatzGuiPieceDetail *detail = calloc(1, sizeof(UrsatzGuiPieceDetail));
    if (!detail) {
        cJSON_Delete(meta);
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }
    detail->id = ursatz_gui_dup_string(id);
    detail->filename = meta_string_copy(meta, "filename");
    detail->format = meta_string_copy(meta, "format");
    detail->uploaded_at = meta_string_copy(meta, "uploaded_at");
    detail->has_notation = meta_notation_read(meta, &detail->notation);
    cJSON_Delete(meta);

    char analysis_path[400];
    snprintf(analysis_path, sizeof(analysis_path), "%s/analysis.json", dir_path);
    detail->document_json = ursatz_gui_read_file(analysis_path, NULL);

    if (!detail->id || !detail->filename || !detail->format || !detail->uploaded_at || !detail->document_json) {
        ursatz_gui_piece_detail_destroy(detail);
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }

    /* The note ids come from importing the stored source again: they are the
       ids of the source's own NoteEvents (a notation override keeps them), in
       the Analyzer's event order. */
    ImportedNotes notes;
    if (!import_piece(dir_path, format_from_label(detail->format), &notes)) {
        ursatz_gui_piece_detail_destroy(detail);
        return URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED;
    }
    char **ids = NULL;
    size_t id_count = 0;
    AnalyzerStatus ids_status = analyzer_score_note_xml_ids(&notes, &ids, &id_count);
    imported_notes_destroy(&notes);
    if (ids_status != ANALYZER_OK) {
        ursatz_gui_piece_detail_destroy(detail);
        return ids_status == ANALYZER_ERR_ALLOC ? URSATZ_GUI_ANALYSIS_ERR_ALLOC : URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED;
    }
    detail->note_xml_ids = id_count > 0 ? calloc(id_count, sizeof(char *)) : NULL;
    bool ok = id_count == 0 || detail->note_xml_ids != NULL;
    for (size_t i = 0; ok && i < id_count; i++) {
        detail->note_xml_ids[i] = ursatz_gui_dup_string(ids[i]);
        if (!detail->note_xml_ids[i]) {
            ok = false;
        }
        detail->note_count = i + 1;
    }
    analyzer_score_note_xml_ids_destroy(ids, id_count);
    if (!ok) {
        ursatz_gui_piece_detail_destroy(detail);
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }

    *out_detail = detail;
    return URSATZ_GUI_ANALYSIS_OK;
}

static UrsatzGuiAnalysisResult delete_piece_unlocked(const char *pieces_dir, const char *id) {
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }
    remove_piece_dir(dir_path);
    return ursatz_gui_path_is_dir(dir_path) ? URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED : URSATZ_GUI_ANALYSIS_OK;
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

static void set_message(char *message, size_t message_size, const char *text) {
    if (message && message_size > 0) {
        snprintf(message, message_size, "%s", text);
    }
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
static UrsatzGuiAnalysisResult meta_apply_value(cJSON *meta, const char *key, const char *text, char *message,
                                                size_t message_size) {
    bool too_long = false;
    char *value = ursatz_gui_meta_normalize_value(text, &too_long);
    if (!value) {
        if (too_long) {
            char reason[48];
            snprintf(reason, sizeof(reason), "%s is too long", key);
            set_message(message, message_size, reason);
            return URSATZ_GUI_ANALYSIS_ERR_INVALID_METADATA;
        }
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }
    bool ok = meta_set_string(meta, key, value);
    free(value);
    return ok ? URSATZ_GUI_ANALYSIS_OK : URSATZ_GUI_ANALYSIS_ERR_ALLOC;
}

/* Normalizes each of the given tags and adds it to (or removes it from) the list. */
static UrsatzGuiAnalysisResult tags_apply(char ***tags, size_t *count, const char *const *items, size_t item_count,
                                          bool remove, char *message, size_t message_size) {
    for (size_t i = 0; i < item_count; i++) {
        bool invalid = false;
        char *tag = ursatz_gui_meta_normalize_tag(items[i], &invalid);
        if (!tag) {
            if (invalid) {
                set_message(message, message_size, "invalid tag");
                return URSATZ_GUI_ANALYSIS_ERR_INVALID_METADATA;
            }
            return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
        }
        UrsatzGuiMetaResult result =
            remove ? ursatz_gui_meta_tags_remove(tags, count, tag) : ursatz_gui_meta_tags_add(tags, count, tag);
        free(tag);
        if (result == URSATZ_GUI_META_ERR_TOO_MANY_TAGS) {
            set_message(message, message_size, "too many tags");
            return URSATZ_GUI_ANALYSIS_ERR_INVALID_METADATA;
        }
        if (result != URSATZ_GUI_META_OK) {
            return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
        }
    }
    return URSATZ_GUI_ANALYSIS_OK;
}

static UrsatzGuiAnalysisResult update_metadata_unlocked(const char *pieces_dir, const char *id,
                                                             const UrsatzGuiMetaUpdate *update, char *message,
                                                             size_t message_size) {
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }
    cJSON *meta = read_meta_file(dir_path);
    if (!meta) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }

    UrsatzGuiAnalysisResult result = URSATZ_GUI_ANALYSIS_OK;
    if (update->filename && !meta_set_string(meta, "filename", update->filename)) {
        result = URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }
    if (result == URSATZ_GUI_ANALYSIS_OK && update->composer) {
        result = meta_apply_value(meta, "composer", update->composer, message, message_size);
    }
    if (result == URSATZ_GUI_ANALYSIS_OK && update->source) {
        result = meta_apply_value(meta, "source", update->source, message, message_size);
    }

    bool tags_changed = update->has_tags || update->add_tag_count > 0 || update->remove_tag_count > 0;
    if (result == URSATZ_GUI_ANALYSIS_OK && tags_changed) {
        char **tags = NULL;
        size_t tag_count = 0;
        if (!update->has_tags && !meta_tags_copy(meta, &tags, &tag_count)) {
            result = URSATZ_GUI_ANALYSIS_ERR_ALLOC;
        }
        if (result == URSATZ_GUI_ANALYSIS_OK && update->has_tags) {
            result = tags_apply(&tags, &tag_count, update->tags, update->tag_count, false, message, message_size);
        }
        if (result == URSATZ_GUI_ANALYSIS_OK) {
            result = tags_apply(&tags, &tag_count, update->add_tags, update->add_tag_count, false, message,
                                message_size);
        }
        if (result == URSATZ_GUI_ANALYSIS_OK) {
            result = tags_apply(&tags, &tag_count, update->remove_tags, update->remove_tag_count, true, message,
                                message_size);
        }
        if (result == URSATZ_GUI_ANALYSIS_OK && !meta_set_tags(meta, tags, tag_count)) {
            result = URSATZ_GUI_ANALYSIS_ERR_ALLOC;
        }
        ursatz_gui_meta_tags_free(tags, tag_count);
    }
    if (result == URSATZ_GUI_ANALYSIS_OK && update->has_notation) {
        if (update->notation && !notation_is_valid(update->notation)) {
            set_message(message, message_size, "invalid notation");
            result = URSATZ_GUI_ANALYSIS_ERR_INVALID_METADATA;
        } else if (!meta_set_notation(meta, update->notation)) {
            result = URSATZ_GUI_ANALYSIS_ERR_ALLOC;
        }
    }
    if (result != URSATZ_GUI_ANALYSIS_OK) {
        cJSON_Delete(meta);
        return result;
    }

    char *text = cJSON_Print(meta);
    cJSON_Delete(meta);
    if (!text) {
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }
    char meta_path[400];
    snprintf(meta_path, sizeof(meta_path), "%s/meta.json", dir_path);
    bool ok = ursatz_gui_write_file_atomic(meta_path, text, strlen(text));
    free(text);
    return ok ? URSATZ_GUI_ANALYSIS_OK : URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_get_notation(const char *pieces_dir, const char *id, bool *out_has,
                                                          UrsatzGuiNotation *out) {
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }
    cJSON *meta = read_meta_file(dir_path);
    if (!meta) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }
    *out_has = meta_notation_read(meta, out);
    cJSON_Delete(meta);
    return URSATZ_GUI_ANALYSIS_OK;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_rename_piece(const char *pieces_dir, const char *id,
                                                          const char *new_filename) {
    UrsatzGuiMetaUpdate update = {0};
    update.filename = new_filename;
    return ursatz_gui_analysis_update_metadata(pieces_dir, id, &update, NULL, 0);
}

static UrsatzGuiAnalysisResult reanalyze_unlocked(const char *pieces_dir, const char *id) {
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }
    cJSON *meta = read_meta_file(dir_path);
    if (!meta) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }
    const cJSON *format_item = cJSON_GetObjectItemCaseSensitive(meta, "format");
    InputFormat format = format_from_label(cJSON_IsString(format_item) ? format_item->valuestring : NULL);

    char *document_json = NULL;
    UrsatzGuiAnalysisResult result = analyze_source_file(dir_path, format, &document_json);
    if (result != URSATZ_GUI_ANALYSIS_OK) {
        cJSON_Delete(meta);
        return result;
    }

    char analysis_path[400];
    snprintf(analysis_path, sizeof(analysis_path), "%s/analysis.json", dir_path);
    bool ok = ursatz_gui_write_file_atomic(analysis_path, document_json, strlen(document_json));
    free(document_json);

    ok = ok && meta_set_string(meta, "analyzer_version", analyzer_version_string()) &&
         meta_set_number(meta, "document_schema_version", URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION);
    char *text = ok ? cJSON_Print(meta) : NULL;
    cJSON_Delete(meta);
    if (!text) {
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }
    char meta_path[400];
    snprintf(meta_path, sizeof(meta_path), "%s/meta.json", dir_path);
    ok = ursatz_gui_write_file_atomic(meta_path, text, strlen(text));
    free(text);
    return ok ? URSATZ_GUI_ANALYSIS_OK : URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
}

/* Appends text to *out (grown with realloc) with the XML characters escaped. */
static bool append_escaped(char **out, size_t *length, size_t *capacity, const char *text) {
    for (const char *c = text; *c != '\0'; c++) {
        const char *piece = NULL;
        char single[2] = {*c, '\0'};
        switch (*c) {
            case '&': piece = "&amp;"; break;
            case '<': piece = "&lt;"; break;
            case '>': piece = "&gt;"; break;
            case '"': piece = "&quot;"; break;
            case '\'': piece = "&apos;"; break;
            default: piece = single; break;
        }
        size_t piece_length = strlen(piece);
        if (*length + piece_length + 1 > *capacity) {
            size_t grown = (*capacity + piece_length + 1) * 2;
            char *resized = realloc(*out, grown);
            if (!resized) {
                return false;
            }
            *out = resized;
            *capacity = grown;
        }
        memcpy(*out + *length, piece, piece_length + 1);
        *length += piece_length;
    }
    return true;
}

/* Adds the piece's title and composer to the score the Analyzer wrote (it leaves both out, they are kept with the
   piece): <work><work-title> after <score-partwise>, <creator type="composer"> first in <identification>, which is
   created before <part-list> when the score has none. Returns a new string and frees xml, or NULL (xml is freed). */
static char *score_add_title_composer(char *xml, const char *title, const char *composer) {
    bool has_title = title && title[0] != '\0';
    bool has_composer = composer && composer[0] != '\0';
    if (!has_title && !has_composer) {
        return xml;
    }
    static const char kRoot[] = "<score-partwise version=\"3.1\">\n";
    static const char kIdentification[] = "  <identification>\n";
    static const char kPartList[] = "  <part-list>\n";
    char *root = strstr(xml, kRoot);
    char *identification = strstr(xml, kIdentification);
    char *part_list = strstr(xml, kPartList);
    if (!root || !part_list) {
        free(xml);
        return NULL;
    }
    root += sizeof(kRoot) - 1;

    size_t capacity = strlen(xml) + 1024;
    size_t length = 0;
    char *out = malloc(capacity);
    bool ok = out != NULL;
#define APPEND_RAW(text) \
    do { \
        const char *raw_ = (text); \
        size_t raw_length_ = strlen(raw_); \
        if (ok && length + raw_length_ + 1 > capacity) { \
            capacity = (capacity + raw_length_ + 1) * 2; \
            char *resized_ = realloc(out, capacity); \
            if (resized_) { out = resized_; } else { ok = false; } \
        } \
        if (ok) { memcpy(out + length, raw_, raw_length_ + 1); length += raw_length_; } \
    } while (0)
#define APPEND_RANGE(from, to) \
    do { \
        size_t range_length_ = (size_t)((to) - (from)); \
        char *range_copy_ = ok ? malloc(range_length_ + 1) : NULL; \
        if (range_copy_) { memcpy(range_copy_, (from), range_length_); range_copy_[range_length_] = '\0'; APPEND_RAW(range_copy_); free(range_copy_); } else { ok = false; } \
    } while (0)

    if (ok) {
        out[0] = '\0';
    }
    APPEND_RANGE(xml, root);
    if (has_title) {
        APPEND_RAW("  <work>\n    <work-title>");
        ok = ok && append_escaped(&out, &length, &capacity, title);
        APPEND_RAW("</work-title>\n  </work>\n");
    }
    if (has_composer && identification && identification < part_list) {
        char *after = identification + sizeof(kIdentification) - 1;
        APPEND_RANGE(root, after);
        APPEND_RAW("    <creator type=\"composer\">");
        ok = ok && append_escaped(&out, &length, &capacity, composer);
        APPEND_RAW("</creator>\n");
        APPEND_RAW(after);
    } else if (has_composer) {
        APPEND_RANGE(root, part_list);
        APPEND_RAW("  <identification>\n    <creator type=\"composer\">");
        ok = ok && append_escaped(&out, &length, &capacity, composer);
        APPEND_RAW("</creator>\n  </identification>\n");
        APPEND_RAW(part_list);
    } else {
        APPEND_RAW(root);
    }
#undef APPEND_RANGE
#undef APPEND_RAW
    free(xml);
    if (!ok) {
        free(out);
        return NULL;
    }
    return out;
}

static UrsatzGuiAnalysisResult get_piece_musicxml_unlocked(const char *pieces_dir, const char *id,
                                                                char **out_xml, bool *out_meter_defaulted,
                                                                char message[256]) {
    message[0] = '\0';

    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }
    cJSON *meta = read_meta_file(dir_path);
    if (!meta) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }
    const cJSON *format_item = cJSON_GetObjectItemCaseSensitive(meta, "format");
    InputFormat format = format_from_label(cJSON_IsString(format_item) ? format_item->valuestring : NULL);
    /* The Library shows the filename without its extension as the title. */
    char *title = meta_string_copy(meta, "filename");
    char *composer = meta_string_copy(meta, "composer");
    cJSON_Delete(meta);
    if (!title || !composer) {
        free(title);
        free(composer);
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }
    char *title_dot = strrchr(title, '.');
    if (title_dot && title_dot != title) {
        *title_dot = '\0';
    }

    ImportedNotes notes;
    if (!import_piece(dir_path, format, &notes)) {
        free(title);
        free(composer);
        return URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED;
    }

    char *xml = NULL;
    AnalyzerStatus status = analyzer_score_export_musicxml(&notes, &xml);
    bool meter_defaulted = analyzer_imported_meter_defaulted(&notes);
    imported_notes_destroy(&notes);

    if (status != ANALYZER_OK) {
        fprintf(stderr, "ursatz_gui_analysis_get_piece_musicxml: analyzer_score_export_musicxml failed for piece '%s' "
                         "with AnalyzerStatus=%d\n",
                id, (int)status);
        const char *name = status == ANALYZER_ERR_ALLOC      ? "ANALYZER_ERR_ALLOC"
                           : status == ANALYZER_ERR_ARGUMENT ? "ANALYZER_ERR_ARGUMENT"
                                                             : "ANALYZER_ERR_LIBRARY";
        snprintf(message, 256, "%s", name);
        free(title);
        free(composer);
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }

    xml = score_add_title_composer(xml, title, composer);
    free(title);
    free(composer);
    if (!xml) {
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }

    *out_xml = xml;
    *out_meter_defaulted = meter_defaulted;
    return URSATZ_GUI_ANALYSIS_OK;
}

static UrsatzGuiAnalysisResult get_piece_playback_unlocked(const char *pieces_dir, const char *id,
                                                                char **out_json) {
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }
    cJSON *meta = read_meta_file(dir_path);
    if (!meta) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }
    const cJSON *format_item = cJSON_GetObjectItemCaseSensitive(meta, "format");
    InputFormat format = format_from_label(cJSON_IsString(format_item) ? format_item->valuestring : NULL);
    cJSON_Delete(meta);

    ImportedNotes notes;
    if (!import_piece(dir_path, format, &notes)) {
        return URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED;
    }

    AnalyzerPlayback playback;
    AnalyzerStatus status = analyzer_playback_build(&notes, &playback);
    char *json = NULL;
    if (status == ANALYZER_OK) {
        status = analyzer_playback_to_json(&playback, &notes, &json);
        analyzer_playback_destroy(&playback);
    }
    imported_notes_destroy(&notes);
    if (status == ANALYZER_ERR_ALLOC) {
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }
    if (status != ANALYZER_OK) {
        /* A source that imported a moment ago and now has no readable tempo is a failed import. */
        return URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED;
    }
    *out_json = json;
    return URSATZ_GUI_ANALYSIS_OK;
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

static UrsatzGuiAnalysisResult get_document_unlocked(const char *pieces_dir, const char *id,
                                                          const char *const *names, size_t count, char **out_json) {
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }
    char analysis_path[400];
    snprintf(analysis_path, sizeof(analysis_path), "%s/analysis.json", dir_path);
    char *document_json = ursatz_gui_read_file(analysis_path, NULL);
    if (!document_json) {
        return path_exists(dir_path) ? URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED : URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }
    bool ok = ursatz_gui_document_subset(document_json, names, count, out_json, NULL);
    free(document_json);
    return ok ? URSATZ_GUI_ANALYSIS_OK : URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
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

static bool piece_lock_enter(const char *id, UrsatzGuiAnalysisResult *failure) {
    if (!is_valid_piece_id(id)) {
        *failure = URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
        return false;
    }
    if (!ursatz_gui_piece_lock(id)) {
        *failure = URSATZ_GUI_ANALYSIS_ERR_ALLOC;
        return false;
    }
    return true;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_get_piece(const char *pieces_dir, const char *id,
                                                        UrsatzGuiPieceDetail **out_detail) {
    UrsatzGuiAnalysisResult result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    result = get_piece_unlocked(pieces_dir, id, out_detail);
    ursatz_gui_piece_unlock(id);
    return result;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_get_document(const char *pieces_dir, const char *id,
                                                          const char *const *names, size_t count, char **out_json) {
    UrsatzGuiAnalysisResult result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    result = get_document_unlocked(pieces_dir, id, names, count, out_json);
    ursatz_gui_piece_unlock(id);
    return result;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_delete_piece(const char *pieces_dir, const char *id) {
    UrsatzGuiAnalysisResult result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    result = delete_piece_unlocked(pieces_dir, id);
    ursatz_gui_piece_unlock(id);
    return result;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_update_metadata(const char *pieces_dir, const char *id,
                                                             const UrsatzGuiMetaUpdate *update, char *message,
                                                             size_t message_size) {
    UrsatzGuiAnalysisResult result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    result = update_metadata_unlocked(pieces_dir, id, update, message, message_size);
    ursatz_gui_piece_unlock(id);
    return result;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_reanalyze(const char *pieces_dir, const char *id) {
    UrsatzGuiAnalysisResult result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    result = reanalyze_unlocked(pieces_dir, id);
    ursatz_gui_piece_unlock(id);
    return result;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_get_piece_musicxml(const char *pieces_dir, const char *id,
                                                                char **out_xml, bool *out_meter_defaulted,
                                                                char message[256]) {
    UrsatzGuiAnalysisResult result;
    message[0] = '\0';
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    result = get_piece_musicxml_unlocked(pieces_dir, id, out_xml, out_meter_defaulted, message);
    ursatz_gui_piece_unlock(id);
    return result;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_get_piece_playback(const char *pieces_dir, const char *id,
                                                                char **out_json) {
    UrsatzGuiAnalysisResult result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    result = get_piece_playback_unlocked(pieces_dir, id, out_json);
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

UrsatzGuiAnalysisResult ursatz_gui_analysis_update_piece(const char *pieces_dir, const char *id,
                                                          const UrsatzGuiMetaUpdate *update, char *message,
                                                          size_t message_size) {
    UrsatzGuiAnalysisResult result;
    if (!piece_lock_enter(id, &result)) {
        return result;
    }
    bool had_notation = false;
    UrsatzGuiNotation previous;
    memset(&previous, 0, sizeof(previous));
    if (update->has_notation) {
        ursatz_gui_analysis_get_notation(pieces_dir, id, &had_notation, &previous);
    }
    result = update_metadata_unlocked(pieces_dir, id, update, message, message_size);
    if (result == URSATZ_GUI_ANALYSIS_OK && update->has_notation &&
        !notation_equal(had_notation, &previous, update->notation != NULL,
                        update->notation ? update->notation : &previous)) {
        result = reanalyze_unlocked(pieces_dir, id);
        if (result != URSATZ_GUI_ANALYSIS_OK) {
            /* The piece keeps its old analysis; put the old reading back to match it. */
            UrsatzGuiMetaUpdate undo = {0};
            undo.has_notation = true;
            undo.notation = had_notation ? &previous : NULL;
            update_metadata_unlocked(pieces_dir, id, &undo, NULL, 0);
            if (result == URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED) {
                set_message(message, message_size, "invalid notation");
                result = URSATZ_GUI_ANALYSIS_ERR_INVALID_METADATA;
            }
        }
    }
    ursatz_gui_piece_unlock(id);
    return result;
}
