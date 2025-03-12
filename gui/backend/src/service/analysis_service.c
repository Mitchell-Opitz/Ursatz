#include "analysis_service.h"
#include "id_service.h"
#include "piece_metadata.h"

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

#include <events/note_event.h>
#include <export/musicxml_export.h>
#include <identity/entity_id.h>
#include <time/meter_map.h>
#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/input_pipeline.h>

static char *dup_string(const char *value) {
    if (!value) {
        return NULL;
    }
    size_t len = strlen(value) + 1;
    char *copy = malloc(len);
    if (copy) {
        memcpy(copy, value, len);
    }
    return copy;
}

static bool ensure_dir_exists(const char *path) {
#ifdef _WIN32
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
        return true;
    }
    return _mkdir(path) == 0;
#else
    struct stat st;
    if (stat(path, &st) == 0) {
        return true;
    }
    return mkdir(path, 0755) == 0;
#endif
}

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
static int make_dir(const char *path) {
#ifdef _WIN32
    int rc = _mkdir(path);
#else
    int rc = mkdir(path, 0755);
#endif
    if (rc == 0) {
        return 1;
    }
    return errno == EEXIST ? 0 : -1;
}

/* Writes data to "<path>.tmp", then renames it over path. */
static bool write_file_atomic(const char *path, const void *data, size_t size) {
    char tmp_path[400];
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

static bool path_is_dir(const char *path) {
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

/* Reads a whole file into a NUL-terminated buffer (caller frees). */
static char *read_whole_file(const char *path, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    size_t capacity = 4096;
    size_t length = 0;
    char *buffer = malloc(capacity);
    while (buffer) {
        size_t n = fread(buffer + length, 1, capacity - length - 1, file);
        length += n;
        if (n == 0 || feof(file)) {
            break;
        }
        if (length + 1 >= capacity) {
            capacity *= 2;
            char *grown = realloc(buffer, capacity);
            if (!grown) {
                free(buffer);
                buffer = NULL;
                break;
            }
            buffer = grown;
        }
    }
    bool failed = ferror(file) != 0;
    fclose(file);
    if (!buffer || failed) {
        free(buffer);
        return NULL;
    }
    buffer[length] = '\0';
    if (out_size) {
        *out_size = length;
    }
    return buffer;
}

static const char *format_label(InputFormat format) {
    return format == INPUT_FORMAT_MIDI ? "midi" : "musicxml";
}

/* The current time as "YYYY-MM-DDTHH:MM:SSZ". */
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

static const char *analyzer_version_string(void) {
#ifdef URSATZ_ANALYZER_VERSION
    return URSATZ_ANALYZER_VERSION;
#else
    return "unknown";
#endif
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
    bool ok = write_file_atomic(meta_path, text, strlen(text));
    free(text);
    return ok;
}

/* Parses <dir_path>/meta.json; NULL when it is missing or not an object. */
static cJSON *read_meta_file(const char *dir_path) {
    char meta_path[400];
    snprintf(meta_path, sizeof(meta_path), "%s/meta.json", dir_path);
    char *text = read_whole_file(meta_path, NULL);
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
    return dup_string(cJSON_IsString(item) && item->valuestring ? item->valuestring : "");
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
        tags[count] = dup_string(item->valuestring);
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

/* Imports the source file and runs it through the Analyzer. On OK,
   *out_json is the analysis document (caller frees). */
static UrsatzGuiAnalysisResult analyze_source_file(const char *source_path, InputFormat format, char **out_json) {
    ImportedNotes notes;
    if (input_pipeline_import_file(source_path, format, &notes) != INPUT_PIPELINE_OK) {
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
        int made = make_dir(dir_path);
        if (made > 0) {
            *out_id = id;
            return URSATZ_GUI_ANALYSIS_OK;
        }
        free(id);
        if (made < 0) {
            return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
        }
    }
    return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_import_and_save(const char *pieces_dir, const unsigned char *data,
                                                              size_t size, const char *original_filename,
                                                              char **out_id) {
    if (!ensure_dir_exists(pieces_dir)) {
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }

    char *id = NULL;
    UrsatzGuiAnalysisResult result = create_piece_dir(pieces_dir, &id);
    if (result != URSATZ_GUI_ANALYSIS_OK) {
        return result;
    }

    char dir_path[320];
    snprintf(dir_path, sizeof(dir_path), "%s/%s", pieces_dir, id);

    /* The bytes go to disk unchanged under a neutral name first: the format
       is detected from the content, and the final name depends on it. */
    char upload_path[400];
    snprintf(upload_path, sizeof(upload_path), "%s/source.upload", dir_path);
    if (!write_file_atomic(upload_path, data, size)) {
        remove_piece_dir(dir_path);
        free(id);
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }

    InputFormat format = input_format_detect(upload_path);
    if (format == INPUT_FORMAT_AUTO) {
        remove_piece_dir(dir_path);
        free(id);
        return URSATZ_GUI_ANALYSIS_ERR_UNSUPPORTED_FORMAT;
    }

    char source_path[400];
    snprintf(source_path, sizeof(source_path), "%s/%s", dir_path, source_file_name(format, original_filename));
    if (rename(upload_path, source_path) != 0) {
        remove_piece_dir(dir_path);
        free(id);
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }

    char *document_json = NULL;
    result = analyze_source_file(source_path, format, &document_json);
    if (result != URSATZ_GUI_ANALYSIS_OK) {
        remove_piece_dir(dir_path);
        free(id);
        return result;
    }

    char analysis_path[400];
    snprintf(analysis_path, sizeof(analysis_path), "%s/analysis.json", dir_path);
    bool written = write_file_atomic(analysis_path, document_json, strlen(document_json));
    free(document_json);
    if (!written) {
        remove_piece_dir(dir_path);
        free(id);
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }

    char uploaded_at[32];
    iso_utc_now(uploaded_at, sizeof(uploaded_at));
    if (!write_meta_file(dir_path, original_filename, format_label(format), uploaded_at)) {
        remove_piece_dir(dir_path);
        free(id);
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
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
            names[count] = dup_string(name);
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
        if (!path_is_dir(dir_path)) {
            continue;
        }
        cJSON *meta = read_meta_file(dir_path);
        if (!meta) {
            continue;
        }
        UrsatzGuiPieceSummary *piece = &pieces[count];
        piece->id = dup_string(names[i]);
        piece->filename = meta_string_copy(meta, "filename");
        piece->format = meta_string_copy(meta, "format");
        piece->uploaded_at = meta_string_copy(meta, "uploaded_at");
        piece->composer = meta_string_copy(meta, "composer");
        piece->source = meta_string_copy(meta, "source");
        bool tags_ok = meta_tags_copy(meta, &piece->tags, &piece->tag_count);
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
    return path_is_dir(out);
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

/* The id the score export writes as <note xml:id> for an event with this
   EntityID value: a leading 'n' unless it starts with a letter or '_', and
   every character outside [A-Za-z0-9_.-] replaced by '_'. */
static char *score_note_xml_id(const char *raw) {
    size_t length = strlen(raw);
    char *out = malloc(length + 2);
    if (!out) {
        return NULL;
    }
    size_t pos = 0;
    if (length == 0 || !(isalpha((unsigned char)raw[0]) || raw[0] == '_')) {
        out[pos++] = 'n';
    }
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)raw[i];
        out[pos++] = (isalnum(c) || c == '_' || c == '.' || c == '-') ? (char)c : '_';
    }
    out[pos] = '\0';
    return out;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_get_piece(const char *pieces_dir, const char *id,
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
    detail->id = dup_string(id);
    detail->filename = meta_string_copy(meta, "filename");
    detail->format = meta_string_copy(meta, "format");
    detail->uploaded_at = meta_string_copy(meta, "uploaded_at");
    cJSON_Delete(meta);

    char analysis_path[400];
    snprintf(analysis_path, sizeof(analysis_path), "%s/analysis.json", dir_path);
    detail->document_json = read_whole_file(analysis_path, NULL);

    if (!detail->id || !detail->filename || !detail->format || !detail->uploaded_at || !detail->document_json) {
        ursatz_gui_piece_detail_destroy(detail);
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }

    /* The note ids come from importing the stored source again: they are the
       ids of the source's own NoteEvents, in the Analyzer's event order. */
    char source_path[400];
    ImportedNotes notes;
    if (!find_source_path(dir_path, source_path, sizeof(source_path)) ||
        input_pipeline_import_file(source_path, format_from_label(detail->format), &notes) != INPUT_PIPELINE_OK) {
        ursatz_gui_piece_detail_destroy(detail);
        return URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED;
    }
    detail->note_xml_ids = notes.count > 0 ? calloc(notes.count, sizeof(char *)) : NULL;
    bool ok = notes.count == 0 || detail->note_xml_ids != NULL;
    for (size_t i = 0; ok && i < notes.count; i++) {
        detail->note_xml_ids[i] = score_note_xml_id(entity_id_value(note_event_id(notes.events[i])));
        if (!detail->note_xml_ids[i]) {
            ok = false;
        }
        detail->note_count = i + 1;
    }
    imported_notes_destroy(&notes);
    if (!ok) {
        ursatz_gui_piece_detail_destroy(detail);
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }

    *out_detail = detail;
    return URSATZ_GUI_ANALYSIS_OK;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_delete_piece(const char *pieces_dir, const char *id) {
    char dir_path[320];
    if (!piece_dir_path(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return URSATZ_GUI_ANALYSIS_ERR_NOT_FOUND;
    }
    remove_piece_dir(dir_path);
    return path_is_dir(dir_path) ? URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED : URSATZ_GUI_ANALYSIS_OK;
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

UrsatzGuiAnalysisResult ursatz_gui_analysis_update_metadata(const char *pieces_dir, const char *id,
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
    bool ok = write_file_atomic(meta_path, text, strlen(text));
    free(text);
    return ok ? URSATZ_GUI_ANALYSIS_OK : URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_rename_piece(const char *pieces_dir, const char *id,
                                                          const char *new_filename) {
    UrsatzGuiMetaUpdate update = {0};
    update.filename = new_filename;
    return ursatz_gui_analysis_update_metadata(pieces_dir, id, &update, NULL, 0);
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_reanalyze(const char *pieces_dir, const char *id) {
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

    char source_path[400];
    if (!find_source_path(dir_path, source_path, sizeof(source_path))) {
        cJSON_Delete(meta);
        return URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED;
    }
    char *document_json = NULL;
    UrsatzGuiAnalysisResult result = analyze_source_file(source_path, format, &document_json);
    if (result != URSATZ_GUI_ANALYSIS_OK) {
        cJSON_Delete(meta);
        return result;
    }

    char analysis_path[400];
    snprintf(analysis_path, sizeof(analysis_path), "%s/analysis.json", dir_path);
    bool ok = write_file_atomic(analysis_path, document_json, strlen(document_json));
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
    ok = write_file_atomic(meta_path, text, strlen(text));
    free(text);
    return ok ? URSATZ_GUI_ANALYSIS_OK : URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
}

static MusicXmlExportMode export_mode_from_hint(MidiModeHint hint) {
    switch (hint) {
        case MIDI_MODE_HINT_MAJOR:
            return MUSICXML_EXPORT_MODE_MAJOR;
        case MIDI_MODE_HINT_MINOR:
            return MUSICXML_EXPORT_MODE_MINOR;
        case MIDI_MODE_HINT_UNKNOWN:
        default:
            return MUSICXML_EXPORT_MODE_UNKNOWN;
    }
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_get_piece_musicxml(const char *pieces_dir, const char *id,
                                                                char **out_xml, bool *out_meter_defaulted,
                                                                MusicXmlExportResult *out_export_error) {
    *out_export_error = MUSICXML_EXPORT_OK;

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

    char source_path[400];
    ImportedNotes notes;
    if (!find_source_path(dir_path, source_path, sizeof(source_path)) ||
        input_pipeline_import_file(source_path, format, &notes) != INPUT_PIPELINE_OK) {
        return URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED;
    }

    /* The score is written from the imported notes: their parts and staves,
       the source's own meter, and its key signatures in part 0. Tempo
       markings and dynamics are left out. */
    MusicXmlExportPart *parts = NULL;
    if (notes.source.part_count > 0) {
        parts = calloc(notes.source.part_count, sizeof(MusicXmlExportPart));
    }
    MusicXmlExportKeySignature *keys = NULL;
    size_t key_count = notes.key_signature_events.count;
    if (key_count > 0) {
        keys = calloc(key_count, sizeof(MusicXmlExportKeySignature));
    }
    if ((notes.source.part_count > 0 && !parts) || (key_count > 0 && !keys)) {
        free(parts);
        free(keys);
        imported_notes_destroy(&notes);
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }
    for (size_t i = 0; i < notes.source.part_count; i++) {
        parts[i].id = notes.source.parts[i].id;
        parts[i].name = notes.source.parts[i].name;
        parts[i].staff_count = notes.source.parts[i].staff_count;
    }
    for (size_t i = 0; i < key_count; i++) {
        const MidiKeySignatureEvent *event = &notes.key_signature_events.events[i];
        keys[i].sharps_flats = event->sharps_flats;
        keys[i].mode = export_mode_from_hint(event->mode_hint);
        keys[i].position = event->position;
        keys[i].part_index = 0;
    }

    MusicXmlExportScore score;
    memset(&score, 0, sizeof(score));
    score.events = notes.events;
    score.pitch_values = notes.pitch_values;
    score.count = notes.count;
    score.part_index = notes.source.part_index;
    score.staff = notes.source.staff;
    score.parts = parts;
    score.part_count = notes.source.part_count;
    score.meter_map = notes.meter.meter_map;
    score.key_signatures = keys;
    score.key_signature_count = key_count;

    char *xml = NULL;
    MusicXmlExportResult result = musicxml_export_write_score(&score, &xml);
    bool meter_defaulted = notes.meter.meter_map ? meter_map_is_defaulted(notes.meter.meter_map) : true;
    free(parts);
    free(keys);
    imported_notes_destroy(&notes);

    if (result != MUSICXML_EXPORT_OK) {
        fprintf(stderr, "ursatz_gui_analysis_get_piece_musicxml: musicxml_export_write_score failed for piece '%s' "
                         "with MusicXmlExportResult=%d\n",
                id, (int)result);
        *out_export_error = result;
        return URSATZ_GUI_ANALYSIS_ERR_PERSISTENCE_FAILED;
    }

    *out_xml = xml;
    *out_meter_defaulted = meter_defaulted;
    return URSATZ_GUI_ANALYSIS_OK;
}

/* A non-negative-denominator rational, in whole notes. */
typedef struct {
    long long num;
    unsigned long long den;
} PlaybackRational;

typedef struct {
    PlaybackRational at;
    double qpm;
} PlaybackTempoRegion;

static unsigned long long gcd_ull(unsigned long long a, unsigned long long b) {
    while (b != 0) {
        unsigned long long t = a % b;
        a = b;
        b = t;
    }
    return a;
}

static PlaybackRational rational_make(long long num, unsigned long long den) {
    PlaybackRational r = {num, den == 0 ? 1 : den};
    unsigned long long magnitude = num < 0 ? (unsigned long long)(-num) : (unsigned long long)num;
    unsigned long long g = gcd_ull(magnitude, r.den);
    if (g > 1) {
        r.num /= (long long)g;
        r.den /= g;
    }
    return r;
}

static PlaybackRational rational_add(PlaybackRational a, PlaybackRational b) {
    unsigned long long g = gcd_ull(a.den, b.den);
    unsigned long long common = a.den / g * b.den;
    return rational_make(a.num * (long long)(common / a.den) + b.num * (long long)(common / b.den), common);
}

static int rational_compare(PlaybackRational a, PlaybackRational b) {
    unsigned long long g = gcd_ull(a.den, b.den);
    unsigned long long common = a.den / g * b.den;
    long long left = a.num * (long long)(common / a.den);
    long long right = b.num * (long long)(common / b.den);
    return left < right ? -1 : left > right ? 1 : 0;
}

static double round_to_milli(double value) {
    return floor(value * 1000.0 + 0.5) / 1000.0;
}

static cJSON *rational_json(PlaybackRational r) {
    cJSON *array = cJSON_CreateArray();
    if (!array) {
        return NULL;
    }
    cJSON *num = cJSON_CreateNumber((double)r.num);
    cJSON *den = cJSON_CreateNumber((double)r.den);
    if (!num || !den) {
        cJSON_Delete(num);
        cJSON_Delete(den);
        cJSON_Delete(array);
        return NULL;
    }
    cJSON_AddItemToArray(array, num);
    cJSON_AddItemToArray(array, den);
    return array;
}

/* The MIDI file's set-tempo regions, read from the stored bytes. */
static bool playback_midi_tempo(const char *source_path, PlaybackTempoRegion **out_regions, size_t *out_count,
                                bool *out_defaulted) {
    size_t size = 0;
    char *bytes = read_whole_file(source_path, &size);
    if (!bytes) {
        return false;
    }
    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    MidiTempoImport tempo;
    memset(&tempo, 0, sizeof(tempo));
    MidiImportResult imported =
        midi_importer_import_with_tempo((const unsigned char *)bytes, size, &events, &pitch_values, &count, &tempo);
    free(bytes);
    if (imported != MIDI_IMPORT_OK) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitch_values[i]);
    }
    free(events);
    free(pitch_values);

    size_t region_count = tempo_map_region_count(tempo.tempo_map);
    PlaybackTempoRegion *regions = calloc(region_count > 0 ? region_count : 1, sizeof(PlaybackTempoRegion));
    bool ok = regions != NULL;
    for (size_t i = 0; ok && i < region_count; i++) {
        const TempoMapRegion *region = tempo_map_region(tempo.tempo_map, i);
        const MusicalTime *start = tempo_map_region_start(region);
        unsigned long microseconds = tempo_map_region_microseconds_per_quarter(region);
        if (!start || microseconds == 0) {
            ok = false;
            break;
        }
        regions[i].at = rational_make(musical_time_numerator(start), musical_time_denominator(start));
        regions[i].qpm = round_to_milli(60000000.0 / (double)microseconds);
    }
    *out_defaulted = tempo_map_is_defaulted(tempo.tempo_map);
    midi_tempo_import_destroy(&tempo);
    if (!ok) {
        free(regions);
        return false;
    }
    *out_regions = regions;
    *out_count = region_count;
    return true;
}

/* A MusicXML score's tempo: its declared tempo at 0, then one region per
   metronome mark. A mark at the position of an earlier region replaces it. */
static bool playback_score_tempo(const ImportedSource *source, PlaybackTempoRegion **out_regions, size_t *out_count,
                                 bool *out_defaulted) {
    PlaybackTempoRegion *regions = calloc(source->tempo_marking_count + 1, sizeof(PlaybackTempoRegion));
    if (!regions) {
        return false;
    }
    size_t count = 0;
    if (!source->tempo_declared || !(source->initial_quarters_per_minute > 0.0)) {
        regions[0].at = rational_make(0, 1);
        regions[0].qpm = 120.0;
        *out_regions = regions;
        *out_count = 1;
        *out_defaulted = true;
        return true;
    }
    regions[count].at = rational_make(0, 1);
    regions[count].qpm = round_to_milli(source->initial_quarters_per_minute);
    count++;

    for (size_t i = 0; i < source->tempo_marking_count; i++) {
        const ImportedTempoMarking *marking = &source->tempo_markings[i];
        if (!marking->has_metronome || marking->bpm_denominator == 0 || marking->beat_unit_denominator == 0 ||
            marking->position_numerator < 0) {
            continue;
        }
        double qpm = ((double)marking->bpm_numerator / (double)marking->bpm_denominator) *
                     ((double)marking->beat_unit_numerator / (double)marking->beat_unit_denominator) * 4.0;
        if (!(qpm > 0.0)) {
            continue;
        }
        PlaybackRational at = rational_make(marking->position_numerator, marking->position_denominator);
        size_t slot = 0;
        while (slot < count && rational_compare(regions[slot].at, at) < 0) {
            slot++;
        }
        if (slot < count && rational_compare(regions[slot].at, at) == 0) {
            regions[slot].qpm = round_to_milli(qpm);
            continue;
        }
        memmove(&regions[slot + 1], &regions[slot], (count - slot) * sizeof(PlaybackTempoRegion));
        regions[slot].at = at;
        regions[slot].qpm = round_to_milli(qpm);
        count++;
    }
    *out_regions = regions;
    *out_count = count;
    *out_defaulted = false;
    return true;
}

static int compare_size_t(const void *left, const void *right) {
    size_t a = *(const size_t *)left;
    size_t b = *(const size_t *)right;
    return a < b ? -1 : a > b ? 1 : 0;
}

UrsatzGuiAnalysisResult ursatz_gui_analysis_get_piece_playback(const char *pieces_dir, const char *id,
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

    char source_path[400];
    ImportedNotes notes;
    if (!find_source_path(dir_path, source_path, sizeof(source_path)) ||
        input_pipeline_import_file(source_path, format, &notes) != INPUT_PIPELINE_OK) {
        return URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED;
    }

    UrsatzGuiAnalysisResult result = URSATZ_GUI_ANALYSIS_OK;
    PlaybackTempoRegion *regions = NULL;
    size_t region_count = 0;
    bool tempo_defaulted = false;
    bool is_midi = notes.source.format == INPUT_FORMAT_MIDI;
    bool tempo_ok = is_midi ? playback_midi_tempo(source_path, &regions, &region_count, &tempo_defaulted)
                            : playback_score_tempo(&notes.source, &regions, &region_count, &tempo_defaulted);
    if (!tempo_ok) {
        imported_notes_destroy(&notes);
        /* A source that imported a moment ago and now has no readable tempo
           is a failed import; an allocation failure is not told apart. */
        return is_midi ? URSATZ_GUI_ANALYSIS_ERR_IMPORT_FAILED : URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }

    size_t *part_ids = notes.count > 0 ? malloc(notes.count * sizeof(size_t)) : NULL;
    cJSON *root = cJSON_CreateObject();
    cJSON *tempo_array = cJSON_CreateArray();
    cJSON *parts_array = cJSON_CreateArray();
    cJSON *notes_array = cJSON_CreateArray();
    if ((notes.count > 0 && !part_ids) || !root || !tempo_array || !parts_array || !notes_array) {
        cJSON_Delete(tempo_array);
        cJSON_Delete(parts_array);
        cJSON_Delete(notes_array);
        cJSON_Delete(root);
        free(part_ids);
        free(regions);
        imported_notes_destroy(&notes);
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }
    cJSON_AddItemToObject(root, "ok", cJSON_CreateTrue());
    cJSON_AddStringToObject(root, "tempo_source", tempo_defaulted ? "default" : (is_midi ? "file" : "score"));

    for (size_t i = 0; result == URSATZ_GUI_ANALYSIS_OK && i < region_count; i++) {
        cJSON *entry = cJSON_CreateObject();
        cJSON *at = rational_json(regions[i].at);
        if (!entry || !at) {
            cJSON_Delete(entry);
            cJSON_Delete(at);
            result = URSATZ_GUI_ANALYSIS_ERR_ALLOC;
            break;
        }
        cJSON_AddItemToObject(entry, "at", at);
        cJSON_AddNumberToObject(entry, "qpm", regions[i].qpm);
        cJSON_AddItemToArray(tempo_array, entry);
    }
    cJSON_AddItemToObject(root, "tempo", tempo_array);

    PlaybackRational end = rational_make(0, 1);
    for (size_t i = 0; result == URSATZ_GUI_ANALYSIS_OK && i < notes.count; i++) {
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(notes.events[i]));
        const Duration *duration = note_event_duration(notes.events[i]);
        PlaybackRational on = rational_make(musical_time_numerator(position), musical_time_denominator(position));
        PlaybackRational len = rational_make(duration_numerator(duration), duration_denominator(duration));
        PlaybackRational note_end = rational_add(on, len);
        if (rational_compare(note_end, end) > 0) {
            end = note_end;
        }
        /* The MIDI note number, as the Ursatz MIDI renderer reads it. */
        const PitchNumericValue *value = notes.pitch_values[i];
        long long den = (long long)pitch_numeric_value_position_denominator(value);
        long long pitch = den > 0 ? pitch_numeric_value_position_numerator(value) / den : 0;
        pitch = pitch < 0 ? 0 : pitch > 127 ? 127 : pitch;

        cJSON *entry = cJSON_CreateObject();
        cJSON *on_json = rational_json(on);
        cJSON *len_json = rational_json(len);
        if (!entry || !on_json || !len_json) {
            cJSON_Delete(entry);
            cJSON_Delete(on_json);
            cJSON_Delete(len_json);
            result = URSATZ_GUI_ANALYSIS_ERR_ALLOC;
            break;
        }
        cJSON_AddNumberToObject(entry, "i", (double)i);
        cJSON_AddItemToObject(entry, "on", on_json);
        cJSON_AddItemToObject(entry, "len", len_json);
        cJSON_AddNumberToObject(entry, "pitch", (double)pitch);
        cJSON_AddNumberToObject(entry, "part", (double)notes.streams[i]);
        cJSON_AddItemToArray(notes_array, entry);
        part_ids[i] = notes.streams[i];
    }

    if (result == URSATZ_GUI_ANALYSIS_OK && notes.count > 0) {
        qsort(part_ids, notes.count, sizeof(size_t), compare_size_t);
        for (size_t i = 0; result == URSATZ_GUI_ANALYSIS_OK && i < notes.count; i++) {
            if (i > 0 && part_ids[i] == part_ids[i - 1]) {
                continue;
            }
            size_t index = part_ids[i];
            const char *name = "";
            if (!is_midi && index < notes.source.part_count && notes.source.parts[index].name) {
                name = notes.source.parts[index].name;
            }
            cJSON *entry = cJSON_CreateObject();
            if (!entry) {
                result = URSATZ_GUI_ANALYSIS_ERR_ALLOC;
                break;
            }
            cJSON_AddNumberToObject(entry, "index", (double)index);
            cJSON_AddStringToObject(entry, "name", name);
            cJSON_AddNumberToObject(entry, "program", 0);
            cJSON_AddItemToArray(parts_array, entry);
        }
    }
    cJSON_AddItemToObject(root, "parts", parts_array);

    cJSON *end_json = rational_json(end);
    if (!end_json) {
        result = URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    } else {
        cJSON_AddItemToObject(root, "end", end_json);
    }
    cJSON_AddItemToObject(root, "notes", notes_array);

    char *json = result == URSATZ_GUI_ANALYSIS_OK ? cJSON_PrintUnformatted(root) : NULL;
    cJSON_Delete(root);
    free(part_ids);
    free(regions);
    imported_notes_destroy(&notes);
    if (result != URSATZ_GUI_ANALYSIS_OK) {
        return result;
    }
    if (!json) {
        return URSATZ_GUI_ANALYSIS_ERR_ALLOC;
    }
    *out_json = json;
    return URSATZ_GUI_ANALYSIS_OK;
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
