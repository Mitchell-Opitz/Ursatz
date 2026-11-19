#include "analysis.h"
#include "base/last_error.h"
#include "items/library_metadata.h"
#include "items/piece_lock.h"
#include "import_cache.h"
#include "base/knowledge.h"
#include "items/collections.h"
#include "items/subjects.h"
#include "items/txn.h"
#include "items/library_pieces.h"

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

static const char *format_label(ImportedFormat format) {
    return format == IMPORTED_FORMAT_MIDI ? "midi" : "musicxml";
}

bool workshop_library_source_path(const char *dir_path, char *out, size_t out_size);
static bool import_piece(const char *dir_path, ImportedFormat format, ImportedScore *out);

/* Imports the piece's source (with its notation override, if any) and runs it
   through the Analyzer. On OK, *out_json is the analysis document (caller frees). */
static UrsatzStatus analyze_source_file(const char *dir_path, ImportedFormat format, char **out_json) {
    char source_path[400];
    ImportedScore notes;
    if (!workshop_library_source_path(dir_path, source_path, sizeof(source_path)) || !import_piece(dir_path, format, &notes)) {
        return workshop_fail(URSATZ_ERR_FORMAT, NULL);
    }

    AnalyzerOptions options = analyzer_options_default();
    options.knowledge = workshop_knowledge_snapshot();
    AnalyzerResult *result = NULL;
    if (analyzer_analyze(&notes, &options, &result) != URSATZ_OK) {
        imported_score_destroy(&notes);
        return workshop_fail(URSATZ_ERR_FORMAT, NULL);
    }

    AnalysisDocument *document = analyzer_document_build(source_path, result);
    analyzer_result_destroy(result);
    imported_score_destroy(&notes);
    if (!document) {
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    char *json = analyzer_document_to_json(document);
    analyzer_document_destroy(document);
    if (!json) {
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    *out_json = json;
    return URSATZ_OK;
}

/* Writes the upload into the staging folder dir_path of a new piece and analyzes it. On failure the folder is removed. The transaction of the
   import adopts the folder once it is whole (workshop_analysis_import_and_save). */
static UrsatzStatus save_new_piece(const char *dir_path, const unsigned char *data, size_t size, const char *original_filename) {
    UrsatzStatus result;

    /* The bytes go to disk unchanged under a neutral name first: the format
       is detected from the content, and the final name depends on it. */
    char upload_path[400];
    snprintf(upload_path, sizeof(upload_path), "%s/source.upload", dir_path);
    if (!(common_file_write_atomic(upload_path, data, size, NULL) == URSATZ_OK)) {
        common_dir_remove(dir_path, NULL);
        return workshop_fail(URSATZ_ERR_IO, NULL);
    }

    ImportedFormat format = imported_format_detect(upload_path);
    if (format == IMPORTED_FORMAT_AUTO) {
        common_dir_remove(dir_path, NULL);
        return workshop_fail(URSATZ_ERR_REQUEST, NULL);
    }

    char source_path[400];
    snprintf(source_path, sizeof(source_path), "%s/%s", dir_path, workshop_library_source_name(format, original_filename));
    if (rename(upload_path, source_path) != 0) {
        common_dir_remove(dir_path, NULL);
        return workshop_fail(URSATZ_ERR_IO, NULL);
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
        return workshop_fail(URSATZ_ERR_IO, NULL);
    }

    char uploaded_at[32];
    common_utc_now(uploaded_at);
    if (!workshop_library_meta_write_new(dir_path, original_filename, format_label(format), uploaded_at)) {
        common_dir_remove(dir_path, NULL);
        return workshop_fail(URSATZ_ERR_IO, NULL);
    }

    return URSATZ_OK;
}

UrsatzStatus workshop_analysis_import_and_save(const char *pieces_dir, const unsigned char *data,
                                                              size_t size, const char *original_filename,
                                                              char **out_id) {
    if (!(common_dir_make(pieces_dir, NULL) == URSATZ_OK)) {
        return workshop_fail(URSATZ_ERR_IO, NULL);
    }

    char *id = NULL;
    UrsatzStatus result = workshop_library_piece_create(pieces_dir, &id);
    if (result != URSATZ_OK) {
        return result;
    }
    /* The piece is built in a folder of its own (no reader lists it) and becomes pieces_dir/<id>/ in one transaction, whole or not at all. */
    static unsigned serial = 0;
    char stage[400];
    char final_dir[320];
    snprintf(final_dir, sizeof(final_dir), "%s/%s", pieces_dir, id);
    snprintf(stage, sizeof(stage), "%s/%s.stage-%d-%u", pieces_dir, id, (int)getpid(), __atomic_add_fetch(&serial, 1, __ATOMIC_RELAXED));
    if (common_dir_create(stage, NULL) != URSATZ_OK) {
        free(id);
        return workshop_fail(URSATZ_ERR_IO, NULL);
    }
    result = save_new_piece(stage, data, size, original_filename);
    if (result == URSATZ_OK) {
        WorkshopTxn *txn = workshop_txn_begin(NULL, "library.import", "michelle", id, 0);
        if (!txn) {
            result = workshop_fail(URSATZ_ERR_IO, NULL);
        } else if (workshop_txn_adopt_folder(txn, stage, final_dir, NULL) != URSATZ_OK) {
            workshop_txn_abort(txn);
            result = workshop_fail(URSATZ_ERR_IO, NULL);
        } else {
            UrsatzError error;
            memset(&error, 0, sizeof(error));
            result = workshop_txn_commit(txn, &error) == URSATZ_OK ? URSATZ_OK : workshop_fail(URSATZ_ERR_IO, NULL);
        }
    }
    if (result != URSATZ_OK) {
        common_dir_remove(stage, NULL);
        free(id);
        return result;
    }
    *out_id = id;
    return URSATZ_OK;
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

/* Imports the piece's stored source and applies the notation override kept in
   its meta.json, if any (R-110). The analysis, the score and playback all
   read the piece through this, so they agree on its meter and note values. */
static bool import_piece(const char *dir_path, ImportedFormat format, ImportedScore *out) {
    char source_path[400];
    if (!workshop_library_source_path(dir_path, source_path, sizeof(source_path)) ||
        analyzer_import_file(source_path, format, out, NULL) != URSATZ_OK) {
        return false;
    }
    cJSON *meta = workshop_library_meta_read(dir_path);
    WorkshopNotation notation;
    bool has_notation = meta && workshop_library_meta_notation(meta, &notation);
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
   file. Called with the piece lock held; release the entry with workshop_import_cache_release. NULL when the
   source does not import (*out_failed true) or memory ran out. */
static WorkshopImportEntry *acquire_import(const char *dir_path, const char *id, ImportedFormat format,
                                                bool *out_failed) {
    *out_failed = true;
    char source_path[400];
    char stamp[96];
    if (!workshop_library_source_path(dir_path, source_path, sizeof(source_path)) ||
        !file_stamp(source_path, stamp, sizeof(stamp))) {
        return NULL;
    }
    WorkshopNotation notation;
    cJSON *meta = workshop_library_meta_read(dir_path);
    bool has_notation = meta && workshop_library_meta_notation(meta, &notation);
    cJSON_Delete(meta);
    if (!has_notation) {
        memset(&notation, 0, sizeof(notation));
    }
    char version[768];
    snprintf(version, sizeof(version), "%d|%s|%d|%s|%s|%s|%s", (int)format, source_path, has_notation ? 1 : 0,
             notation.meter, notation.value_scale, notation.pickup, stamp);
    ImportRequest request = {dir_path, format};
    WorkshopImportEntry *entry = workshop_import_cache_acquire(id, version, import_piece_for_cache, &request,
                                                                   out_failed);
    return entry;
}

static UrsatzStatus ensure_current_unlocked(const char *pieces_dir, const char *id);

static UrsatzStatus get_piece_unlocked(const char *pieces_dir, const char *id,
                                                        WorkshopPieceDetail **out_detail) {
    /* A stored analysis from another schema, Analyzer or Ursatz is analyzed again, once. When that fails the stored
       document is served and its status says why it is stale. */
    if (ensure_current_unlocked(pieces_dir, id) == URSATZ_ERR_NOT_FOUND) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    char dir_path[320];
    if (!workshop_library_piece_dir(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = workshop_library_meta_read(dir_path);
    if (!meta) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }

    WorkshopPieceDetail *detail = calloc(1, sizeof(WorkshopPieceDetail));
    if (!detail) {
        cJSON_Delete(meta);
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    detail->id = common_strdup(id);
    detail->filename = workshop_library_meta_string_copy(meta, "filename");
    detail->format = workshop_library_meta_string_copy(meta, "format");
    detail->uploaded_at = workshop_library_meta_string_copy(meta, "uploaded_at");
    detail->has_notation = workshop_library_meta_notation(meta, &detail->notation);
    cJSON_Delete(meta);

    char analysis_path[400];
    snprintf(analysis_path, sizeof(analysis_path), "%s/analysis.json", dir_path);
    common_file_read(analysis_path, &detail->document_json, NULL, NULL);
    detail->analysis_status = workshop_library_status_name(detail->document_json ? analyzer_document_status(detail->document_json)
                                                                 : ANALYZER_DOCUMENT_UNREADABLE);

    if (!detail->id || !detail->filename || !detail->format || !detail->uploaded_at || !detail->document_json) {
        workshop_piece_detail_destroy(detail);
        return workshop_fail(URSATZ_ERR_IO, NULL);
    }

    /* The note ids come from importing the stored source again: they are the
       ids of the source's own NoteEvents (a notation override keeps them), in
       the Analyzer's event order. */
    bool import_failed = false;
    WorkshopImportEntry *import = acquire_import(dir_path, id, format_from_label(detail->format), &import_failed);
    if (!import) {
        workshop_piece_detail_destroy(detail);
        return import_failed ? workshop_fail(URSATZ_ERR_FORMAT, NULL) : workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    char **ids = NULL;
    size_t id_count = 0;
    UrsatzStatus ids_status = analyzer_score_note_xml_ids(workshop_import_entry_notes(import), &ids, &id_count, NULL);
    workshop_import_cache_release(import);
    if (ids_status != URSATZ_OK) {
        workshop_piece_detail_destroy(detail);
        return ids_status == URSATZ_ERR_ALLOC ? workshop_fail(URSATZ_ERR_ALLOC, NULL) : workshop_fail(URSATZ_ERR_FORMAT, NULL);
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
        workshop_piece_detail_destroy(detail);
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }

    *out_detail = detail;
    return URSATZ_OK;
}

static UrsatzStatus reanalyze_unlocked(const char *pieces_dir, const char *id) {
    char dir_path[320];
    if (!workshop_library_piece_dir(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = workshop_library_meta_read(dir_path);
    if (!meta) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    workshop_import_cache_invalidate(id);
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
    bool ok = workshop_library_write_file(analysis_path, document_json, strlen(document_json));
    free(document_json);

    ok = ok && workshop_library_meta_set_string(meta, "analyzer_version", analyzer_version()) &&
         workshop_library_meta_set_number(meta, "document_schema_version", ANALYZER_DOCUMENT_SCHEMA_VERSION) &&
         workshop_library_meta_set_string(meta, "ursatz_commit", analyzer_producer_key());
    ok = ok && workshop_library_meta_write(dir_path, meta);
    cJSON_Delete(meta);
    return ok ? URSATZ_OK : workshop_fail(URSATZ_ERR_IO, NULL);
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
    if (cJSON_IsString(commit) && workshop_library_meta_set_string(meta, "ursatz_commit", commit->valuestring)) {
        (void)workshop_library_meta_write(dir_path, meta); /* best effort: the backfill is made again on the next read */
    }
    cJSON_Delete(document);
}

/* Makes the stored analysis current: fills a missing ursatz_commit in meta.json, then analyzes the piece again when
   analyzer_document_status says the stored document is not CURRENT. OK when it is current now; the failure of the
   new analysis otherwise, with the stored files unchanged. */
static UrsatzStatus ensure_current_unlocked(const char *pieces_dir, const char *id) {
    char dir_path[320];
    if (!workshop_library_piece_dir(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = workshop_library_meta_read(dir_path);
    if (!meta) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
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
    if (!workshop_library_piece_dir(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = workshop_library_meta_read(dir_path);
    if (!meta) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    const cJSON *format_item = cJSON_GetObjectItemCaseSensitive(meta, "format");
    ImportedFormat format = format_from_label(cJSON_IsString(format_item) ? format_item->valuestring : NULL);
    /* The Library shows the filename without its extension as the title. */
    char *title = workshop_library_meta_string_copy(meta, "filename");
    char *composer = workshop_library_meta_string_copy(meta, "composer");
    cJSON_Delete(meta);
    if (!title || !composer) {
        free(title);
        free(composer);
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    char *title_dot = strrchr(title, '.');
    if (title_dot && title_dot != title) {
        *title_dot = '\0';
    }

    bool import_failed = false;
    WorkshopImportEntry *import = acquire_import(dir_path, id, format, &import_failed);
    if (!import) {
        free(title);
        free(composer);
        return import_failed ? workshop_fail(URSATZ_ERR_FORMAT, NULL) : workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }

    bool meter_defaulted = false;
    const char *cached = workshop_import_entry_score(import, &meter_defaulted);
    char *xml = cached ? common_strdup(cached) : NULL;
    if (cached) {
        workshop_import_cache_release(import);
        free(title);
        free(composer);
        if (!xml) {
            return workshop_fail(URSATZ_ERR_ALLOC, NULL);
        }
    } else {
        const ImportedScore *notes = workshop_import_entry_notes(import);
        AnalyzerScoreMetadata metadata = {title, composer};
        UrsatzStatus status = analyzer_score_export_musicxml(notes, &metadata, &xml, NULL);
        meter_defaulted = analyzer_imported_meter_defaulted(notes);

        if (status != URSATZ_OK) {
            workshop_import_cache_release(import);
            fprintf(stderr, "workshop_analysis_get_piece_musicxml: analyzer_score_export_musicxml failed for piece "
                             "'%s' with UrsatzStatus=%d\n",
                    id, (int)status);
            free(title);
            free(composer);
            return workshop_fail(status == URSATZ_ERR_ALLOC || status == URSATZ_ERR_REQUEST ? status : URSATZ_ERR_INTERNAL, NULL);
        }

        free(title);
        free(composer);
        char *kept = xml ? common_strdup(xml) : NULL;
        if (kept) {
            workshop_import_entry_set_score(import, kept, meter_defaulted);
        }
        workshop_import_cache_release(import);
        if (!xml) {
            return workshop_fail(URSATZ_ERR_ALLOC, NULL);
        }
    }

    *out_xml = xml;
    *out_meter_defaulted = meter_defaulted;
    return URSATZ_OK;
}

static UrsatzStatus get_piece_meter_defaulted_unlocked(const char *pieces_dir, const char *id,
                                                                       bool *out_meter_defaulted) {
    char dir_path[320];
    if (!workshop_library_piece_dir(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = workshop_library_meta_read(dir_path);
    if (!meta) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    const cJSON *format_item = cJSON_GetObjectItemCaseSensitive(meta, "format");
    ImportedFormat format = format_from_label(cJSON_IsString(format_item) ? format_item->valuestring : NULL);
    cJSON_Delete(meta);

    bool import_failed = false;
    WorkshopImportEntry *import = acquire_import(dir_path, id, format, &import_failed);
    if (!import) {
        return import_failed ? workshop_fail(URSATZ_ERR_FORMAT, NULL) : workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    *out_meter_defaulted = analyzer_imported_meter_defaulted(workshop_import_entry_notes(import));
    workshop_import_cache_release(import);
    return URSATZ_OK;
}

static UrsatzStatus get_piece_playback_unlocked(const char *pieces_dir, const char *id,
                                                                char **out_json) {
    char dir_path[320];
    if (!workshop_library_piece_dir(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = workshop_library_meta_read(dir_path);
    if (!meta) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    const cJSON *format_item = cJSON_GetObjectItemCaseSensitive(meta, "format");
    ImportedFormat format = format_from_label(cJSON_IsString(format_item) ? format_item->valuestring : NULL);
    cJSON_Delete(meta);

    bool import_failed = false;
    WorkshopImportEntry *import = acquire_import(dir_path, id, format, &import_failed);
    if (!import) {
        return import_failed ? workshop_fail(URSATZ_ERR_FORMAT, NULL) : workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    const ImportedScore *notes = workshop_import_entry_notes(import);

    AnalyzerPlayback playback;
    UrsatzStatus status = analyzer_playback_build(notes, &playback);
    char *json = NULL;
    if (status == URSATZ_OK) {
        status = analyzer_playback_to_json(&playback, notes, &json);
        analyzer_playback_destroy(&playback);
    }
    workshop_import_cache_release(import);
    if (status == URSATZ_ERR_ALLOC) {
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    if (status != URSATZ_OK) {
        /* A source that imported a moment ago and now has no readable tempo is a failed import. */
        return workshop_fail(URSATZ_ERR_FORMAT, NULL);
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
    if (!workshop_library_piece_dir(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    cJSON *meta = workshop_library_meta_read(dir_path);
    if (!meta) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    const cJSON *format_item = cJSON_GetObjectItemCaseSensitive(meta, "format");
    ImportedFormat format = format_from_label(cJSON_IsString(format_item) ? format_item->valuestring : NULL);
    char *title = workshop_library_meta_string_copy(meta, "filename");
    cJSON_Delete(meta);
    if (!title) {
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
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
        return common_path_exists(dir_path) ? workshop_fail(URSATZ_ERR_IO, NULL)
                                                : workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }

    bool import_failed = false;
    WorkshopImportEntry *import = acquire_import(dir_path, id, format, &import_failed);
    if (!import) {
        free(title);
        free(document_json);
        return import_failed ? workshop_fail(URSATZ_ERR_FORMAT, NULL) : workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }

    AnalyzerTextExportOptions options = analyzer_text_export_options_default();
    options.title = title;
    options.first_measure = first_measure;
    options.last_measure = last_measure;
    char *text = NULL;
    UrsatzStatus status =
        analyzer_text_export_piece(workshop_import_entry_notes(import), document_json, &options, &text, NULL);
    workshop_import_cache_release(import);
    free(title);
    free(document_json);
    if (status == URSATZ_ERR_ALLOC) {
        return workshop_fail(URSATZ_ERR_ALLOC, NULL);
    }
    if (status != URSATZ_OK) {
        fprintf(stderr, "workshop_analysis_get_piece_export: analyzer_text_export_piece failed for piece '%s' "
                         "with UrsatzStatus=%d\n",
                id, (int)status);
        return workshop_fail(status == URSATZ_ERR_REQUEST ? URSATZ_ERR_REQUEST : URSATZ_ERR_INTERNAL, NULL);
    }
    *out_text = text;
    return URSATZ_OK;
}

bool workshop_document_subset(const char *document_json, const char *const *names, size_t count,
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
    if (!workshop_library_piece_dir(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    char analysis_path[400];
    snprintf(analysis_path, sizeof(analysis_path), "%s/analysis.json", dir_path);
    char *document_json = NULL;
    common_file_read(analysis_path, &document_json, NULL, NULL);
    if (!document_json) {
        return common_path_exists(dir_path) ? workshop_fail(URSATZ_ERR_IO, NULL) : workshop_fail(URSATZ_ERR_NOT_FOUND, NULL);
    }
    if (!names) {
        *out_json = document_json;
        return URSATZ_OK;
    }
    bool ok = workshop_document_subset(document_json, names, count, out_json, NULL);
    free(document_json);
    return ok ? URSATZ_OK : workshop_fail(URSATZ_ERR_IO, NULL);
}

void workshop_piece_detail_destroy(WorkshopPieceDetail *detail) {
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

UrsatzStatus workshop_analysis_get_piece(const char *pieces_dir, const char *id,
                                                        WorkshopPieceDetail **out_detail) {
    UrsatzStatus result;
    (void)workshop_analysis_ensure_current(pieces_dir, id); /* a stale analysis is made again in a transaction of its own (B10 moves it to a job) */
    if (!workshop_library_piece_lock_enter(id, &result)) {
        return result;
    }
    result = get_piece_unlocked(pieces_dir, id, out_detail);
    workshop_piece_unlock(id);
    return result;
}

UrsatzStatus workshop_analysis_get_document(const char *pieces_dir, const char *id,
                                                          const char *const *names, size_t count, char **out_json) {
    UrsatzStatus result;
    if (!workshop_library_piece_lock_enter(id, &result)) {
        return result;
    }
    result = get_document_unlocked(pieces_dir, id, names, count, out_json);
    workshop_piece_unlock(id);
    return result;
}

/* --- the Library's writers: each begins its transaction before any lock and commits it after (items/txn.h) ----------------------------------- */

static WorkshopTxn *begin_library(const char *command, const char *id) {
    WorkshopTxn *txn = workshop_txn_begin(NULL, command, "michelle", id ? id : "", 0);
    workshop_library_set_txn(txn);
    return txn;
}

/* Commits when result is OK, aborts otherwise; the status to answer. */
static UrsatzStatus end_library(WorkshopTxn *txn, UrsatzStatus result) {
    workshop_library_set_txn(NULL);
    if (!txn) {
        return result == URSATZ_OK ? workshop_fail(URSATZ_ERR_IO, NULL) : result;
    }
    if (result != URSATZ_OK) {
        workshop_txn_abort(txn);
        return result;
    }
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    return workshop_txn_commit(txn, &error) == URSATZ_OK ? URSATZ_OK : workshop_fail(URSATZ_ERR_IO, NULL);
}

/* Stages the removal of the piece's folder, under the piece lock. */
static UrsatzStatus delete_piece_staged(const char *pieces_dir, const char *id) {
    UrsatzStatus result;
    if (!workshop_library_piece_lock_enter(id, &result)) {
        return result;
    }
    workshop_import_cache_invalidate(id);
    result = workshop_library_delete_piece_unlocked(pieces_dir, id);
    workshop_piece_unlock(id);
    return result;
}

UrsatzStatus workshop_analysis_delete_piece(const char *pieces_dir, const char *id) {
    WorkshopTxn *txn = begin_library("library.delete", id);
    return end_library(txn, txn ? delete_piece_staged(pieces_dir, id) : workshop_fail(URSATZ_ERR_IO, NULL));
}

UrsatzStatus workshop_analysis_reanalyze(const char *pieces_dir, const char *id) {
    WorkshopTxn *txn = begin_library("library.reanalyze", id);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_IO, NULL);
    }
    UrsatzStatus result;
    if (!workshop_library_piece_lock_enter(id, &result)) {
        return end_library(txn, result);
    }
    result = reanalyze_unlocked(pieces_dir, id);
    workshop_piece_unlock(id);
    return end_library(txn, result);
}

/* True when the stored analysis of the piece is current and its metadata needs no back-fill: a read, so nothing is begun for it. */
static bool piece_is_current(const char *pieces_dir, const char *id) {
    char dir_path[320];
    if (!workshop_library_piece_dir(pieces_dir, id, dir_path, sizeof(dir_path))) {
        return false;
    }
    cJSON *meta = workshop_library_meta_read(dir_path);
    bool has_key = meta && cJSON_IsString(cJSON_GetObjectItemCaseSensitive(meta, "ursatz_commit"));
    cJSON_Delete(meta);
    char analysis_path[400];
    snprintf(analysis_path, sizeof(analysis_path), "%s/analysis.json", dir_path);
    char *document = NULL;
    common_file_read(analysis_path, &document, NULL, NULL);
    bool current = has_key && document && analyzer_document_status(document) == ANALYZER_DOCUMENT_CURRENT;
    free(document);
    return current;
}

UrsatzStatus workshop_analysis_ensure_current(const char *pieces_dir, const char *id) {
    if (piece_is_current(pieces_dir, id)) {
        return URSATZ_OK;
    }
    WorkshopTxn *txn = begin_library("library.reanalyze", id);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_IO, NULL);
    }
    UrsatzStatus result;
    if (!workshop_library_piece_lock_enter(id, &result)) {
        return end_library(txn, result);
    }
    result = ensure_current_unlocked(pieces_dir, id);
    workshop_piece_unlock(id);
    return end_library(txn, result);
}

UrsatzStatus workshop_analysis_get_piece_musicxml(const char *pieces_dir, const char *id,
                                                                char **out_xml, bool *out_meter_defaulted) {
    UrsatzStatus result;
    if (!workshop_library_piece_lock_enter(id, &result)) {
        return result;
    }
    result = get_piece_musicxml_unlocked(pieces_dir, id, out_xml, out_meter_defaulted);
    workshop_piece_unlock(id);
    return result;
}

UrsatzStatus workshop_analysis_get_piece_meter_defaulted(const char *pieces_dir, const char *id,
                                                                       bool *out_meter_defaulted) {
    UrsatzStatus result;
    if (!workshop_library_piece_lock_enter(id, &result)) {
        return result;
    }
    result = get_piece_meter_defaulted_unlocked(pieces_dir, id, out_meter_defaulted);
    workshop_piece_unlock(id);
    return result;
}

UrsatzStatus workshop_analysis_get_piece_playback(const char *pieces_dir, const char *id,
                                                                char **out_json) {
    UrsatzStatus result;
    if (!workshop_library_piece_lock_enter(id, &result)) {
        return result;
    }
    result = get_piece_playback_unlocked(pieces_dir, id, out_json);
    workshop_piece_unlock(id);
    return result;
}

UrsatzStatus workshop_analysis_get_piece_export(const char *pieces_dir, const char *id,
                                                              int first_measure, int last_measure, char **out_text) {
    UrsatzStatus result;
    if (!workshop_library_piece_lock_enter(id, &result)) {
        return result;
    }
    result = get_piece_export_unlocked(pieces_dir, id, first_measure, last_measure, out_text);
    workshop_piece_unlock(id);
    return result;
}

static bool notation_equal(bool has_a, const WorkshopNotation *a, bool has_b, const WorkshopNotation *b) {
    if (has_a != has_b) {
        return false;
    }
    return !has_a || (strcmp(a->meter, b->meter) == 0 && strcmp(a->value_scale, b->value_scale) == 0 &&
                      strcmp(a->pickup, b->pickup) == 0);
}

static UrsatzStatus update_piece_unlocked_enter(const char *pieces_dir, const char *id, const WorkshopMetaUpdate *update, UrsatzError *error);

UrsatzStatus workshop_analysis_update_piece(const char *pieces_dir, const char *id,
                                                          const WorkshopMetaUpdate *update, UrsatzError *error) {
    WorkshopTxn *txn = begin_library("library.update", id);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_IO, NULL);
    }
    return end_library(txn, update_piece_unlocked_enter(pieces_dir, id, update, error));
}

static UrsatzStatus update_piece_unlocked_enter(const char *pieces_dir, const char *id, const WorkshopMetaUpdate *update, UrsatzError *error) {
    UrsatzStatus result;
    if (!workshop_library_piece_lock_enter(id, &result)) {
        return result;
    }
    bool had_notation = false;
    WorkshopNotation previous;
    memset(&previous, 0, sizeof(previous));
    if (update->has_notation) {
        workshop_library_get_notation(pieces_dir, id, &had_notation, &previous);
    }
    workshop_import_cache_invalidate(id);
    result = workshop_library_update_metadata_unlocked(pieces_dir, id, update, error);
    if (result == URSATZ_OK && update->has_notation &&
        !notation_equal(had_notation, &previous, update->notation != NULL,
                        update->notation ? update->notation : &previous)) {
        result = reanalyze_unlocked(pieces_dir, id);
        if (result != URSATZ_OK) {
            /* The piece keeps its old analysis; put the old reading back to match it. */
            WorkshopMetaUpdate undo = {0};
            undo.has_notation = true;
            undo.notation = had_notation ? &previous : NULL;
            workshop_import_cache_invalidate(id);
            workshop_library_update_metadata_unlocked(pieces_dir, id, &undo, NULL);
            if (result == URSATZ_ERR_FORMAT) {
                workshop_error_text(error, "invalid notation");
                result = workshop_fail(URSATZ_ERR_REQUEST, NULL);
            }
        }
    }
    workshop_piece_unlock(id);
    return result;
}

/* ---- the Library route's answers (A20261109-76) ---------------------------- */

/* The members of the document a summary keeps: what the header and the Provenance list read. */
static const char *const kSummarySections[] = {"schema_version", "metadata", "meter"};

UrsatzStatus workshop_library_piece_json(const char *pieces_dir, const char *id, bool summary, cJSON **out) {
    *out = NULL;
    WorkshopPieceDetail *detail = NULL;
    UrsatzStatus result = workshop_analysis_get_piece(pieces_dir, id, &detail);
    if (result != URSATZ_OK) {
        return result;
    }
    cJSON *object = cJSON_CreateObject();
    cJSON *notation = workshop_library_notation_json(detail->has_notation, &detail->notation);
    cJSON *ids = cJSON_CreateArray();
    cJSON *document = NULL;
    cJSON *sections = NULL;
    bool ok = object && notation && ids && cJSON_AddStringToObject(object, "id", detail->id) &&
              cJSON_AddStringToObject(object, "filename", detail->filename) &&
              cJSON_AddStringToObject(object, "format", detail->format) &&
              cJSON_AddStringToObject(object, "uploaded_at", detail->uploaded_at);
    if (ok) {
        ok = cJSON_AddItemToObject(object, "notation", notation);
        notation = ok ? NULL : notation;
    }
    ok = ok && cJSON_AddStringToObject(object, "analysis_status", detail->analysis_status ? detail->analysis_status : "unreadable");
    if (ok && detail->analysis_status && strcmp(detail->analysis_status, "current") != 0) {
        /* The stored document is served as it is: a re-analysis failed (or the piece was written by a build without this check);
           the status names why it is stale. */
        ok = cJSON_AddStringToObject(object, "stale", detail->analysis_status) != NULL;
    }
    for (size_t i = 0; ok && i < detail->note_count; i++) {
        cJSON *note = cJSON_CreateString(detail->note_xml_ids[i]);
        ok = note != NULL;
        if (ok) {
            cJSON_AddItemToArray(ids, note);
        }
    }
    if (ok) {
        ok = cJSON_AddItemToObject(object, "note_xml_ids", ids);
        ids = ok ? NULL : ids;
    }
    /* The stored document is inlined, or its summary with the names of all its sections when ?document=summary asks for it. */
    if (ok && summary) {
        char *summary_json = NULL;
        char *names_json = NULL;
        if (workshop_document_subset(detail->document_json, kSummarySections, sizeof(kSummarySections) / sizeof(kSummarySections[0]),
                                     &summary_json, &names_json)) {
            document = cJSON_Parse(summary_json);
            sections = cJSON_Parse(names_json);
        }
        free(summary_json);
        free(names_json);
        ok = document && sections;
    } else if (ok) {
        document = cJSON_Parse(detail->document_json);
        ok = document != NULL;
    }
    if (ok) {
        ok = cJSON_AddItemToObject(object, "document", document);
        document = ok ? NULL : document;
    }
    if (ok && sections) {
        ok = cJSON_AddItemToObject(object, "document_sections", sections);
        sections = ok ? NULL : sections;
    }
    /* The subjects that link this piece (R-117). */
    cJSON *subjects = ok ? workshop_subjects_for_piece(detail->id) : NULL;
    ok = ok && subjects && cJSON_AddItemToObject(object, "subjects", subjects);
    if (ok) {
        subjects = NULL;
    }
    cJSON_Delete(subjects);
    workshop_piece_detail_destroy(detail);
    if (!ok) {
        cJSON_Delete(notation);
        cJSON_Delete(ids);
        cJSON_Delete(document);
        cJSON_Delete(sections);
        cJSON_Delete(object);
        return workshop_fail(URSATZ_ERR_INTERNAL, "failed to load piece");
    }
    *out = object;
    return URSATZ_OK;
}

UrsatzStatus workshop_library_patch(const char *pieces_dir, const char *id, const cJSON *body, cJSON **piece, UrsatzError *error) {
    *piece = NULL;
    WorkshopLibraryPatch patch;
    UrsatzStatus result = workshop_library_patch_parse(body, &patch, error);
    if (result != URSATZ_OK) {
        return result;
    }
    result = workshop_analysis_update_piece(pieces_dir, id, &patch.update, error);
    workshop_library_patch_free(&patch);
    if (result != URSATZ_OK) {
        return result;
    }
    return workshop_library_entry_json(pieces_dir, id, piece);
}

UrsatzStatus workshop_library_delete(const char *pieces_dir, const char *id) {
    WorkshopTxn *txn = begin_library("library.delete", id);
    if (!txn) {
        return workshop_fail(URSATZ_ERR_IO, NULL);
    }
    UrsatzStatus result = delete_piece_staged(pieces_dir, id);
    if (result == URSATZ_OK) {
        /* A collection never keeps an id whose piece is gone, and neither does a subject: the cleanups are in the same transaction. */
        if (workshop_collection_remove_piece_everywhere(txn, id) != URSATZ_OK) {
            fprintf(stderr, "analyzer: piece %s deleted, but it could not be removed from every collection\n", id);
        }
        if (workshop_subject_forget_piece(txn, id) != URSATZ_OK) {
            fprintf(stderr, "analyzer: piece %s deleted, but it could not be removed from every subject\n", id);
        }
    }
    return end_library(txn, result);
}
