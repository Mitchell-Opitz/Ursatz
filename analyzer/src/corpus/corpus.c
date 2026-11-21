#include <ursatz_analyzer/analyzer_api.h>
#include <ursatz_analyzer_internal/analysis_document.h>
#include <ursatz_analyzer_internal/input_pipeline.h>
#include <ursatz_analyzer/analyzer_api.h>
#include <ursatz_analyzer_internal/version.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


#include "cJSON.h"
#include "export/analysis_document_internal.h"
#include "corpus/corpus_internal.h"
#include <common/files.h>
#include <common/hash.h>
#include <common/strings.h>
#include "util/misc_util.h"

#define ANALYSIS_DOCUMENT_SUFFIX ".analysis.json"
#define INDEX_NAME "corpus-index.json"
#define HOME_KEY_CAPACITY 64

/* What the corpus index records about one piece. */
typedef struct {
    AnalyzerCorpusPieceStatus status;
    const char *error;               /* static text; NULL unless failed */
    char *document;                  /* output path relative to the output directory; NULL unless analyzed */
    unsigned long long content_hash; /* FNV-1a 64 of the input file's bytes */
    long measures;                   /* -1 when unknown */
    char home_key[HOME_KEY_CAPACITY]; /* empty when unknown */
    char *message;                   /* the importer's text of a failed import, for the caller's report; NULL when none */
    char **warnings;                 /* the warnings an import raised, for the caller's report */
    size_t warning_count;
} PieceRecord;

static void record_report_free(PieceRecord *record) {
    free(record->message);
    for (size_t i = 0; i < record->warning_count; i++) {
        free(record->warnings[i]);
    }
    free(record->warnings);
    record->message = NULL;
    record->warnings = NULL;
    record->warning_count = 0;
}

typedef struct {
    char **paths;
    size_t count;
    size_t capacity;
} PieceList;

bool corpus_make_dirs(const char *path) {
    return common_dir_make(path, NULL) == URSATZ_OK;
}

bool corpus_make_parent_dirs(const char *file_path) {
    const char *slash = strrchr(file_path, '/');
    if (!slash || slash == file_path) {
        return true;
    }
    char *parent = strndup(file_path, (size_t)(slash - file_path));
    if (!parent) {
        return false;
    }
    bool ok = corpus_make_dirs(parent);
    free(parent);
    return ok;
}

static bool piece_list_add(PieceList *list, char *path) {
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 16;
        char **grown = realloc(list->paths, capacity * sizeof(char *));
        if (!grown) {
            return false;
        }
        list->paths = grown;
        list->capacity = capacity;
    }
    list->paths[list->count++] = path;
    return true;
}

static void piece_list_destroy(PieceList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->paths[i]);
    }
    free(list->paths);
    list->paths = NULL;
    list->count = list->capacity = 0;
}

static int compare_paths(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static bool has_piece_extension(const char *name) {
    static const char *const extensions[] = {".mid", ".midi", ".musicxml", ".xml"};
    const char *dot = strrchr(name, '.');
    if (!dot) {
        return false;
    }
    for (size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
        const char *want = extensions[i];
        const char *have = dot;
        while (*want && *have && tolower((unsigned char)*have) == *want) {
            want++;
            have++;
        }
        if (!*want && !*have) {
            return true;
        }
    }
    return false;
}

static bool same_file(const struct stat *a, const struct stat *b) {
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino;
}

/* Adds the pieces under dir (whose path relative to the input directory is relative, "" for the top) to list.
   Returns false on allocation failure. Unreadable subdirectories are skipped. */
static bool walk_directory(const char *dir, const char *relative, const struct stat *output_info, PieceList *list) {
    DIR *handle = opendir(dir);
    if (!handle) {
        return true;
    }
    bool ok = true;
    struct dirent *entry;
    while (ok && (entry = readdir(handle)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char *full = common_path_join_alloc(dir, entry->d_name);
        char *entry_relative = common_path_join_alloc(relative, entry->d_name);
        struct stat info;
        if (!full || !entry_relative) {
            ok = false;
        } else if (lstat(full, &info) == 0 && S_ISDIR(info.st_mode)) {
            if (!(output_info && same_file(&info, output_info))) {
                ok = walk_directory(full, entry_relative, output_info, list);
            }
        } else if (stat(full, &info) == 0 && S_ISREG(info.st_mode) && has_piece_extension(entry->d_name)) {
            ok = piece_list_add(list, entry_relative);
            entry_relative = NULL;
        }
        free(full);
        free(entry_relative);
    }
    closedir(handle);
    return ok;
}

static const char *import_error_text(ImportedScoreResult result) {
    switch (result) {
    case IMPORTED_SCORE_ERR_IO:
        return "cannot read file";
    case IMPORTED_SCORE_ERR_UNKNOWN_FORMAT:
        return "unknown format";
    default:
        return "import failed";
    }
}

/* Writes text to <output_dir>/<relative><suffix>, creating directories. */
static bool write_output_file(const char *output_dir, const char *relative, const char *suffix, const char *text, UrsatzError *error) {
    char *name = malloc(strlen(relative) + strlen(suffix) + 1);
    if (!name) {
        return false;
    }
    strcpy(name, relative);
    strcat(name, suffix);
    char *path = common_path_join_alloc(output_dir, name);
    free(name);
    if (!path) {
        return false;
    }
    bool ok = corpus_make_parent_dirs(path) && (common_file_write_atomic(path, text, strlen(text), NULL) == URSATZ_OK);
    if (!ok) {
        ursatz_error_set(error, URSATZ_ERR_IO, path, "cannot write '%s'", path);
    }
    free(path);
    return ok;
}

static unsigned long long fnv1a_file(const char *path) {
    uint64_t hash = COMMON_FNV1A64_OFFSET;
    FILE *file = fopen(path, "rb");
    if (!file) {
        return hash;
    }
    unsigned char chunk[8192];
    size_t got;
    while ((got = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        hash = common_fnv1a64(chunk, got, hash);
    }
    fclose(file);
    return hash;
}

/* The measure count and the home key's label, read from the document's meter and keys sections. */
static void read_document_summary(const AnalysisDocument *document, PieceRecord *record) {
    const cJSON *meter = cJSON_GetObjectItemCaseSensitive(document->root, "meter");
    const cJSON *measure_count = cJSON_GetObjectItemCaseSensitive(meter, "measure_count");
    if (cJSON_IsNumber(measure_count)) {
        record->measures = (long)measure_count->valuedouble;
    }
    const cJSON *keys = cJSON_GetObjectItemCaseSensitive(document->root, "keys");
    const cJSON *home = cJSON_GetObjectItemCaseSensitive(keys, "home_key");
    const cJSON *label = cJSON_GetObjectItemCaseSensitive(home, "label");
    if (cJSON_IsString(label) && label->valuestring) {
        snprintf(record->home_key, sizeof(record->home_key), "%s", label->valuestring);
    }
}

/* Imports, analyzes and writes one piece into record. A piece that cannot be analyzed is a failed record and still
   returns OK; only a failure to write an output file is a run error. */
static UrsatzStatus analyze_piece(const char *output_dir, const char *relative, const char *input_path,
                                     const KnowledgeSnapshot *knowledge, PieceRecord *record, UrsatzError *error) {
    record->status = ANALYZER_CORPUS_PIECE_FAILED;
    record->error = NULL;
    record->document = NULL;
    record->measures = -1;
    record->home_key[0] = '\0';
    record->content_hash = fnv1a_file(input_path);

    ImportedScore notes;
    AnalyzerResult *result = NULL;
    ImportedScoreResult imported = IMPORTED_SCORE_OK;
    UrsatzStatus analyzed = URSATZ_OK;
    UrsatzError import_error = {{0}};
    AnalyzerRunFileResult ran =
        analyzer_run_file(input_path, IMPORTED_FORMAT_AUTO, NULL, knowledge, &notes, &result, &imported, NULL, &analyzed, &import_error);
    if (ran == ANALYZER_RUN_FILE_ERR_IMPORT) {
        record->error = import_error_text(imported);
        record->message = strdup(import_error.message);
        return URSATZ_OK;
    }
    if (ran != ANALYZER_RUN_FILE_OK) {
        record->error = analyzed == URSATZ_ERR_ALLOC ? "out of memory" : "analysis failed";
        return URSATZ_OK;
    }
    if (notes.source.warning_line_count > 0) {
        record->warnings = calloc(notes.source.warning_line_count, sizeof(char *));
        for (size_t i = 0; record->warnings && i < notes.source.warning_line_count; i++) {
            record->warnings[i] = strdup(notes.source.warnings[i]);
            record->warning_count += record->warnings[i] != NULL;
        }
    }
    AnalysisDocument *document = analyzer_run_file_document(input_path, &notes, result);
    if (!document) {
        record->error = "out of memory";
        return URSATZ_OK;
    }

    read_document_summary(document, record);
    char *json = analyzer_document_to_json(document);
    analyzer_document_destroy(document);
    char *document_name = malloc(strlen(relative) + strlen(ANALYSIS_DOCUMENT_SUFFIX) + 1);
    if (!json || !document_name) {
        free(json);
        free(document_name);
        record->measures = -1;
        record->home_key[0] = '\0';
        record->error = "out of memory";
        return URSATZ_OK;
    }
    strcpy(document_name, relative);
    strcat(document_name, ANALYSIS_DOCUMENT_SUFFIX);

    bool wrote = write_output_file(output_dir, relative, ANALYSIS_DOCUMENT_SUFFIX, json, error);
    free(json);
    if (!wrote) {
        free(document_name);
        return URSATZ_ERR_IO;
    }
    record->status = ANALYZER_CORPUS_PIECE_ANALYZED;
    record->document = document_name;
    return URSATZ_OK;
}

/* ---- Corpus index ---- */

/* A JSON string literal for text, or the literal null for NULL; the escaping is cJSON's. */
static void append_json_or_null(CommonText *out, const char *text) {
    if (text) {
        common_text_append_json_string(out, text);
    } else {
        common_text_append(out, "null");
    }
}

/* The index for the first count pieces: 2-space indent, fixed key order, one piece member per line. */
static char *render_index(const char *input_dir, const char *const *paths, const PieceRecord *records, size_t count) {
    CommonText out;
    common_text_init(&out);
    common_text_appendf(&out, "{\n  \"schema\": \"ursatz-corpus-index\",\n  \"schema_version\": %d,\n", ANALYZER_CORPUS_INDEX_VERSION);
    common_text_append(&out,
                       "  \"analyzer_version\": \"" URSATZ_ANALYZER_VERSION "\",\n"
                       "  \"producer_key\": \"");
    common_text_append(&out, analyzer_producer_key());
    common_text_append(&out, "\",\n");
    common_text_appendf(&out, "  \"document_schema_version\": %d,\n", ANALYZER_DOCUMENT_SCHEMA_VERSION);
    common_text_append(&out, "  \"input_dir\": ");
    append_json_or_null(&out, input_dir);
    common_text_append(&out, count ? ",\n  \"pieces\": [\n" : ",\n  \"pieces\": []\n");
    for (size_t i = 0; i < count; i++) {
        const PieceRecord *record = &records[i];
        common_text_append(&out, "    {\n      \"path\": ");
        append_json_or_null(&out, paths[i]);
        common_text_append(&out, ",\n      \"document\": ");
        append_json_or_null(&out, record->document);
        common_text_append(&out, ",\n      \"status\": ");
        common_text_append(&out, record->status == ANALYZER_CORPUS_PIECE_FAILED ? "\"failed\"" : "\"analyzed\"");
        common_text_append(&out, ",\n      \"error\": ");
        append_json_or_null(&out, record->error);
        common_text_appendf(&out, ",\n      \"content_hash\": \"%016llx\",\n      \"measures\": ", record->content_hash);
        if (record->measures >= 0) {
            common_text_appendf(&out, "%ld", record->measures);
        } else {
            common_text_append(&out, "null");
        }
        common_text_append(&out, ",\n      \"home_key\": ");
        append_json_or_null(&out, record->home_key[0] ? record->home_key : NULL);
        common_text_append(&out, i + 1 < count ? "\n    },\n" : "\n    }\n");
    }
    if (count) {
        common_text_append(&out, "  ]\n");
    }
    common_text_append(&out, "}\n");
    return common_text_take(&out);
}

/* ---- Resume ---- */

/* Reads the index left by an earlier run. *out is NULL when there is no index, or when it was written by another
   analyzer, document schema version or producer key (nothing in it can be reused; an index of schema version 1 holds
   ursatz_commit instead of producer_key, so it reuses nothing and is written again). An index that exists but cannot be
   read is URSATZ_ERR_FORMAT. */
static UrsatzStatus load_previous_index(const char *output_dir, cJSON **out, UrsatzError *error) {
    *out = NULL;
    char *path = common_path_join_alloc(output_dir, INDEX_NAME);
    if (!path) {
        return ursatz_error_set(error, URSATZ_ERR_FORMAT, output_dir, "out of memory");
    }
    char *text = NULL;
    if (common_file_read(path, &text, NULL, NULL) != URSATZ_OK) {
        free(path);
        return URSATZ_OK;
    }
    cJSON *root = cJSON_Parse(text);
    free(text);
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "schema");
    const cJSON *pieces = cJSON_GetObjectItemCaseSensitive(root, "pieces");
    if (!cJSON_IsString(name) || strcmp(name->valuestring, "ursatz-corpus-index") != 0 || !cJSON_IsArray(pieces)) {
        UrsatzStatus status = ursatz_error_set(error, URSATZ_ERR_FORMAT, path, "cannot read the corpus index '%s'", path);
        cJSON_Delete(root);
        free(path);
        return status;
    }
    free(path);
    /* an index of another schema version is not reused: the run writes it again */
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "schema_version");
    if (!cJSON_IsNumber(version) || version->valuedouble != ANALYZER_CORPUS_INDEX_VERSION) {
        cJSON_Delete(root);
        return URSATZ_OK;
    }

    const cJSON *analyzer_version = cJSON_GetObjectItemCaseSensitive(root, "analyzer_version");
    const cJSON *producer_key = cJSON_GetObjectItemCaseSensitive(root, "producer_key");
    const cJSON *document_schema = cJSON_GetObjectItemCaseSensitive(root, "document_schema_version");
    if (!cJSON_IsString(analyzer_version) || !cJSON_IsString(producer_key) || !cJSON_IsNumber(document_schema) ||
        analyzer_document_status_of((int)document_schema->valuedouble, analyzer_version->valuestring,
                                    producer_key->valuestring) != ANALYZER_DOCUMENT_CURRENT) {
        cJSON_Delete(root);
        return URSATZ_OK;
    }
    *out = root;
    return URSATZ_OK;
}

static bool file_exists(const char *dir, const char *relative, const char *suffix) {
    char *name = malloc(strlen(relative) + strlen(suffix) + 1);
    if (!name) {
        return false;
    }
    strcpy(name, relative);
    strcat(name, suffix);
    char *path = common_path_join_alloc(dir, name);
    free(name);
    struct stat info;
    bool exists = path && stat(path, &info) == 0 && S_ISREG(info.st_mode);
    free(path);
    return exists;
}

/* Fills record from the earlier run's entry for relative when that entry can be reused: it is an analyzed entry
   with the same content hash, and its document file exists. */
static bool reuse_previous(const cJSON *previous, const char *output_dir, const char *relative,
                           unsigned long long content_hash, PieceRecord *record) {
    const cJSON *entry;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(previous, "pieces")) {
        const cJSON *path = cJSON_GetObjectItemCaseSensitive(entry, "path");
        if (cJSON_IsString(path) && strcmp(path->valuestring, relative) == 0) {
            break;
        }
    }
    if (!entry) {
        return false;
    }
    const cJSON *status = cJSON_GetObjectItemCaseSensitive(entry, "status");
    const cJSON *document = cJSON_GetObjectItemCaseSensitive(entry, "document");
    const cJSON *hash = cJSON_GetObjectItemCaseSensitive(entry, "content_hash");
    char expected_hash[17];
    snprintf(expected_hash, sizeof(expected_hash), "%016llx", content_hash);
    if (!cJSON_IsString(status) || strcmp(status->valuestring, "analyzed") != 0 || !cJSON_IsString(document) ||
        !cJSON_IsString(hash) || strcmp(hash->valuestring, expected_hash) != 0 ||
        !file_exists(output_dir, document->valuestring, "")) {
        return false;
    }
    char *document_copy = strdup(document->valuestring);
    if (!document_copy) {
        return false;
    }
    const cJSON *measures = cJSON_GetObjectItemCaseSensitive(entry, "measures");
    const cJSON *home_key = cJSON_GetObjectItemCaseSensitive(entry, "home_key");
    record->status = ANALYZER_CORPUS_PIECE_SKIPPED;
    record->error = NULL;
    record->document = document_copy;
    record->content_hash = content_hash;
    record->measures = cJSON_IsNumber(measures) ? (long)measures->valuedouble : -1;
    record->home_key[0] = '\0';
    if (cJSON_IsString(home_key)) {
        snprintf(record->home_key, sizeof(record->home_key), "%s", home_key->valuestring);
    }
    return true;
}

/* Writes the index for the first count pieces atomically over corpus-index.json. */
static bool write_index(const char *output_dir, const char *input_dir, const char *const *paths,
                        const PieceRecord *records, size_t count) {
    char *text = render_index(input_dir, paths, records, count);
    char *final_path = common_path_join_alloc(output_dir, INDEX_NAME);
    bool ok = text && final_path && (common_file_write_atomic(final_path, text, strlen(text), NULL) == URSATZ_OK);
    free(text);
    free(final_path);
    return ok;
}

UrsatzStatus analyzer_corpus_run(const AnalyzerCorpusOptions *options, AnalyzerCorpusSummary *out, UrsatzError *error) {
    AnalyzerCorpusSummary summary = {0, 0, 0};
    if (out) {
        *out = summary;
    }
    if (!options || !options->knowledge || !options->input_dir || !options->input_dir[0]) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "", "corpus run needs an input directory and a knowledge snapshot");
    }

    struct stat input_info;
    if (stat(options->input_dir, &input_info) != 0 || !S_ISDIR(input_info.st_mode)) {
        return ursatz_error_set(error, URSATZ_ERR_NOT_FOUND, options->input_dir, "cannot read input directory '%s'", options->input_dir);
    }

    char *default_output = NULL;
    const char *output_dir = options->output_dir;
    if (!output_dir) {
        default_output = common_path_join_alloc(options->input_dir, CORPUS_DEFAULT_OUTPUT_NAME);
        if (!default_output) {
            return ursatz_error_set(error, URSATZ_ERR_IO, "", "out of memory");
        }
        output_dir = default_output;
    }
    if (!corpus_make_dirs(output_dir)) {
        UrsatzStatus status = ursatz_error_set(error, URSATZ_ERR_IO, output_dir, "cannot create output directory '%s'", output_dir);
        free(default_output);
        return status;
    }

    struct stat output_info;
    bool have_output_info = stat(output_dir, &output_info) == 0;

    PieceList pieces = {0};
    PieceRecord *records = NULL;
    UrsatzStatus result = URSATZ_OK;
    if (!walk_directory(options->input_dir, "", have_output_info ? &output_info : NULL, &pieces)) {
        result = ursatz_error_set(error, URSATZ_ERR_NOT_FOUND, options->input_dir, "out of memory while listing '%s'", options->input_dir);
    } else {
        qsort(pieces.paths, pieces.count, sizeof(char *), compare_paths);
        records = calloc(pieces.count ? pieces.count : 1, sizeof(PieceRecord));
        if (!records) {
            result = ursatz_error_set(error, URSATZ_ERR_IO, "", "out of memory");
        }
    }

    cJSON *previous = NULL;
    if (result == URSATZ_OK && options->resume) {
        result = load_previous_index(output_dir, &previous, error);
    }

    size_t done = 0;
    for (; result == URSATZ_OK && done < pieces.count; done++) {
        const char *relative = pieces.paths[done];
        char *input_path = common_path_join_alloc(options->input_dir, relative);
        if (!input_path) {
            result = ursatz_error_set(error, URSATZ_ERR_IO, "", "out of memory");
            break;
        }
        PieceRecord *record = &records[done];
        if (!(previous && reuse_previous(previous, output_dir, relative, fnv1a_file(input_path), record))) {
            result = analyze_piece(output_dir, relative, input_path, options->knowledge, record, error);
        }
        free(input_path);
        if (result != URSATZ_OK) {
            record_report_free(record);
            break;
        }
        if (!write_index(output_dir, options->input_dir, (const char *const *)pieces.paths, records, done + 1)) {
            result = ursatz_error_set(error, URSATZ_ERR_IO, output_dir, "cannot write the corpus index in '%s'", output_dir);
            record_report_free(record);
            done++;
            break;
        }
        if (record->status == ANALYZER_CORPUS_PIECE_ANALYZED) {
            summary.analyzed++;
        } else if (record->status == ANALYZER_CORPUS_PIECE_SKIPPED) {
            summary.skipped++;
        } else {
            summary.failed++;
        }
        if (options->on_piece) {
            AnalyzerCorpusPieceReport report = {record->message, (const char *const *)record->warnings, record->warning_count};
            options->on_piece(relative, record->status, record->error, &report, options->user);
        }
        record_report_free(record);
    }
    if (result == URSATZ_OK && pieces.count == 0 &&
        !write_index(output_dir, options->input_dir, (const char *const *)pieces.paths, records, 0)) {
        result = ursatz_error_set(error, URSATZ_ERR_IO, output_dir, "cannot write the corpus index in '%s'", output_dir);
    }
    for (size_t i = 0; records && i < done && i < pieces.count; i++) {
        free(records[i].document);
    }
    free(records);
    cJSON_Delete(previous);

    piece_list_destroy(&pieces);
    free(default_output);
    if (out) {
        *out = summary;
    }
    return result;
}
