#include <ursatz_analyzer/corpus.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/markdown_report.h>
#include <ursatz_analyzer/report_writer.h>
#include <ursatz_analyzer/version.h>

#include "cJSON.h"
#include "pipeline/analysis_document_internal.h"
#include "pipeline/corpus_internal.h"
#include "pipeline/run_file.h"
#include "util/file_util.h"
#include "util/misc_util.h"

#define ANALYSIS_DOCUMENT_SUFFIX ".analysis.json"
#define REPORT_SUFFIX ".report.md"
#define INDEX_NAME "corpus-index.json"
#define HOME_KEY_CAPACITY 64

/* What the corpus index records about one piece. */
typedef struct {
    CorpusPieceStatus status;
    const char *error;               /* static text; NULL unless failed */
    char *document;                  /* output path relative to the output directory; NULL unless analyzed */
    unsigned long long content_hash; /* FNV-1a 64 of the input file's bytes */
    long measures;                   /* -1 when unknown */
    char home_key[HOME_KEY_CAPACITY]; /* empty when unknown */
} PieceRecord;

typedef struct {
    char **paths;
    size_t count;
    size_t capacity;
} PieceList;

char *corpus_join(const char *dir, const char *name) {
    size_t dir_len = strlen(dir);
    size_t name_len = strlen(name);
    bool need_slash = dir_len > 0 && dir[dir_len - 1] != '/';
    char *joined = malloc(dir_len + (need_slash ? 1 : 0) + name_len + 1);
    if (!joined) {
        return NULL;
    }
    memcpy(joined, dir, dir_len);
    if (need_slash) {
        joined[dir_len++] = '/';
    }
    memcpy(joined + dir_len, name, name_len + 1);
    return joined;
}

bool corpus_make_dirs(const char *path) {
    char *copy = strdup(path);
    if (!copy) {
        return false;
    }
    bool ok = true;
    for (char *p = copy + 1; ok; p++) {
        if (*p == '/' || *p == '\0') {
            char saved = *p;
            *p = '\0';
            if (mkdir(copy, 0777) != 0 && errno != EEXIST) {
                ok = false;
            }
            *p = saved;
            if (saved == '\0') {
                break;
            }
        }
    }
    if (ok) {
        struct stat info;
        ok = stat(path, &info) == 0 && S_ISDIR(info.st_mode);
    }
    free(copy);
    return ok;
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
        char *full = corpus_join(dir, entry->d_name);
        char *entry_relative = corpus_join(relative, entry->d_name);
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

static const char *import_error_text(InputPipelineResult result) {
    switch (result) {
    case INPUT_PIPELINE_ERR_CANNOT_READ_FILE:
        return "cannot read file";
    case INPUT_PIPELINE_ERR_UNKNOWN_FORMAT:
        return "unknown format";
    default:
        return "import failed";
    }
}

/* Writes text to <output_dir>/<relative><suffix>, creating directories. */
static bool write_output_file(const char *output_dir, const char *relative, const char *suffix, const char *text) {
    char *name = malloc(strlen(relative) + strlen(suffix) + 1);
    if (!name) {
        return false;
    }
    strcpy(name, relative);
    strcat(name, suffix);
    char *path = corpus_join(output_dir, name);
    free(name);
    if (!path) {
        return false;
    }
    bool ok = corpus_make_parent_dirs(path) && write_report_file(path, text);
    free(path);
    return ok;
}

static unsigned long long fnv1a_file(const char *path) {
    uint64_t hash = FNV1A_OFFSET_BASIS;
    FILE *file = fopen(path, "rb");
    if (!file) {
        return hash;
    }
    unsigned char chunk[8192];
    size_t got;
    while ((got = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        hash = fnv1a_bytes(hash, chunk, got);
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
static CorpusRunResult analyze_piece(const CorpusRunOptions *options, const char *output_dir, const char *relative,
                                     const char *input_path, PieceRecord *record) {
    record->status = CORPUS_PIECE_FAILED;
    record->error = NULL;
    record->document = NULL;
    record->measures = -1;
    record->home_key[0] = '\0';
    record->content_hash = fnv1a_file(input_path);

    ImportedNotes notes;
    AnalyzerResult *result = NULL;
    InputPipelineResult imported = INPUT_PIPELINE_OK;
    RunFileResult ran = analyzer_run_file(input_path, INPUT_FORMAT_AUTO, NULL, &notes, &result, &imported, NULL);
    if (ran == RUN_FILE_ERR_IMPORT) {
        record->error = import_error_text(imported);
        return CORPUS_RUN_OK;
    }
    if (ran != RUN_FILE_OK) {
        record->error = "out of memory";
        return CORPUS_RUN_OK;
    }
    AnalysisDocument *document = analyzer_run_file_document(input_path, &notes, result);
    if (!document) {
        record->error = "out of memory";
        return CORPUS_RUN_OK;
    }

    read_document_summary(document, record);
    char *json = analysis_document_to_json(document);
    char *markdown = options->markdown ? markdown_report_render(document) : NULL;
    analysis_document_destroy(document);
    char *document_name = malloc(strlen(relative) + strlen(ANALYSIS_DOCUMENT_SUFFIX) + 1);
    if (!json || (options->markdown && !markdown) || !document_name) {
        free(json);
        free(markdown);
        free(document_name);
        record->measures = -1;
        record->home_key[0] = '\0';
        record->error = "out of memory";
        return CORPUS_RUN_OK;
    }
    strcpy(document_name, relative);
    strcat(document_name, ANALYSIS_DOCUMENT_SUFFIX);

    bool wrote = write_output_file(output_dir, relative, ANALYSIS_DOCUMENT_SUFFIX, json) &&
                 (!markdown || write_output_file(output_dir, relative, REPORT_SUFFIX, markdown));
    free(json);
    free(markdown);
    if (!wrote) {
        free(document_name);
        return CORPUS_RUN_ERR_WRITE;
    }
    record->status = CORPUS_PIECE_ANALYZED;
    record->document = document_name;
    return CORPUS_RUN_OK;
}

/* ---- Corpus index ---- */

static bool append_text(char **buffer, size_t *length, size_t *capacity, const char *text) {
    size_t add = strlen(text);
    if (*length + add + 1 > *capacity) {
        size_t grown = *capacity ? *capacity : 1024;
        while (*length + add + 1 > grown) {
            grown *= 2;
        }
        char *data = realloc(*buffer, grown);
        if (!data) {
            return false;
        }
        *buffer = data;
        *capacity = grown;
    }
    memcpy(*buffer + *length, text, add + 1);
    *length += add;
    return true;
}

/* A JSON string literal for text (or the literal null for NULL), escaped by cJSON. */
static bool append_json_string(char **buffer, size_t *length, size_t *capacity, const char *text) {
    if (!text) {
        return append_text(buffer, length, capacity, "null");
    }
    cJSON *item = cJSON_CreateString(text);
    char *printed = item ? cJSON_PrintUnformatted(item) : NULL;
    cJSON_Delete(item);
    if (!printed) {
        return false;
    }
    bool ok = append_text(buffer, length, capacity, printed);
    cJSON_free(printed);
    return ok;
}

/* The index for the first count pieces: 2-space indent, fixed key order, one piece member per line. */
static char *render_index(const char *input_dir, const char *const *paths, const PieceRecord *records, size_t count) {
    char *out = NULL;
    size_t length = 0;
    size_t capacity = 0;
    char number[64];
    bool ok = append_text(&out, &length, &capacity,
                          "{\n  \"schema\": \"ursatz-corpus-index\",\n  \"schema_version\": 1,\n"
                          "  \"analyzer_version\": \"" URSATZ_ANALYZER_VERSION "\",\n") &&
              (snprintf(number, sizeof(number), "  \"document_schema_version\": %d,\n",
                        URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION),
               append_text(&out, &length, &capacity, number)) &&
              append_text(&out, &length, &capacity, "  \"input_dir\": ") &&
              append_json_string(&out, &length, &capacity, input_dir) &&
              append_text(&out, &length, &capacity, count ? ",\n  \"pieces\": [\n" : ",\n  \"pieces\": []\n");
    for (size_t i = 0; ok && i < count; i++) {
        const PieceRecord *record = &records[i];
        ok = append_text(&out, &length, &capacity, "    {\n      \"path\": ") &&
             append_json_string(&out, &length, &capacity, paths[i]) &&
             append_text(&out, &length, &capacity, ",\n      \"document\": ") &&
             append_json_string(&out, &length, &capacity, record->document) &&
             append_text(&out, &length, &capacity, ",\n      \"status\": ") &&
             append_text(&out, &length, &capacity,
                         record->status == CORPUS_PIECE_FAILED ? "\"failed\"" : "\"analyzed\"") &&
             append_text(&out, &length, &capacity, ",\n      \"error\": ") &&
             append_json_string(&out, &length, &capacity, record->error) &&
             (snprintf(number, sizeof(number), ",\n      \"content_hash\": \"%016llx\",\n      \"measures\": ",
                       record->content_hash),
              append_text(&out, &length, &capacity, number));
        if (ok && record->measures >= 0) {
            snprintf(number, sizeof(number), "%ld", record->measures);
            ok = append_text(&out, &length, &capacity, number);
        } else if (ok) {
            ok = append_text(&out, &length, &capacity, "null");
        }
        ok = ok && append_text(&out, &length, &capacity, ",\n      \"home_key\": ") &&
             append_json_string(&out, &length, &capacity, record->home_key[0] ? record->home_key : NULL) &&
             append_text(&out, &length, &capacity, i + 1 < count ? "\n    },\n" : "\n    }\n");
    }
    if (ok && count) {
        ok = append_text(&out, &length, &capacity, "  ]\n");
    }
    ok = ok && append_text(&out, &length, &capacity, "}\n");
    if (!ok) {
        free(out);
        return NULL;
    }
    return out;
}

/* ---- Resume ---- */

/* Reads the index left by an earlier run. *out is NULL when there is no index, or when it was written by another
   analyzer or document schema version (nothing in it can be reused). An index that exists but cannot be read is
   CORPUS_RUN_ERR_INDEX. */
static CorpusRunResult load_previous_index(const char *output_dir, cJSON **out) {
    *out = NULL;
    char *path = corpus_join(output_dir, INDEX_NAME);
    if (!path) {
        fprintf(stderr, "Error: out of memory\n");
        return CORPUS_RUN_ERR_INDEX;
    }
    char *text = NULL;
    if (!file_read_all(path, &text, NULL)) {
        free(path);
        return CORPUS_RUN_OK;
    }
    cJSON *root = cJSON_Parse(text);
    free(text);
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "schema");
    const cJSON *pieces = cJSON_GetObjectItemCaseSensitive(root, "pieces");
    if (!cJSON_IsString(name) || strcmp(name->valuestring, "ursatz-corpus-index") != 0 || !cJSON_IsArray(pieces)) {
        fprintf(stderr, "Error: cannot read the corpus index '%s'\n", path);
        cJSON_Delete(root);
        free(path);
        return CORPUS_RUN_ERR_INDEX;
    }
    free(path);

    const cJSON *analyzer_version = cJSON_GetObjectItemCaseSensitive(root, "analyzer_version");
    const cJSON *document_schema = cJSON_GetObjectItemCaseSensitive(root, "document_schema_version");
    if (!cJSON_IsString(analyzer_version) || strcmp(analyzer_version->valuestring, URSATZ_ANALYZER_VERSION) != 0 ||
        !cJSON_IsNumber(document_schema) ||
        (int)document_schema->valuedouble != URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION) {
        cJSON_Delete(root);
        return CORPUS_RUN_OK;
    }
    *out = root;
    return CORPUS_RUN_OK;
}

static bool file_exists(const char *dir, const char *relative, const char *suffix) {
    char *name = malloc(strlen(relative) + strlen(suffix) + 1);
    if (!name) {
        return false;
    }
    strcpy(name, relative);
    strcat(name, suffix);
    char *path = corpus_join(dir, name);
    free(name);
    struct stat info;
    bool exists = path && stat(path, &info) == 0 && S_ISREG(info.st_mode);
    free(path);
    return exists;
}

/* Fills record from the earlier run's entry for relative when that entry can be reused: it is an analyzed entry
   with the same content hash, its document file exists, and so does the report when one is wanted. */
static bool reuse_previous(const cJSON *previous, const CorpusRunOptions *options, const char *output_dir,
                           const char *relative, unsigned long long content_hash, PieceRecord *record) {
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
        !file_exists(output_dir, document->valuestring, "") ||
        (options->markdown && !file_exists(output_dir, relative, REPORT_SUFFIX))) {
        return false;
    }
    char *document_copy = strdup(document->valuestring);
    if (!document_copy) {
        return false;
    }
    const cJSON *measures = cJSON_GetObjectItemCaseSensitive(entry, "measures");
    const cJSON *home_key = cJSON_GetObjectItemCaseSensitive(entry, "home_key");
    record->status = CORPUS_PIECE_SKIPPED;
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
    char *final_path = corpus_join(output_dir, INDEX_NAME);
    bool ok = text && final_path && file_write_atomic(final_path, text);
    free(text);
    free(final_path);
    return ok;
}

CorpusRunResult corpus_run(const CorpusRunOptions *options, CorpusRunSummary *out) {
    CorpusRunSummary summary = {0, 0, 0};
    if (out) {
        *out = summary;
    }
    if (!options || !options->input_dir || !options->input_dir[0]) {
        fprintf(stderr, "Error: corpus run needs an input directory\n");
        return CORPUS_RUN_ERR_ARGUMENT;
    }

    struct stat input_info;
    if (stat(options->input_dir, &input_info) != 0 || !S_ISDIR(input_info.st_mode)) {
        fprintf(stderr, "Error: cannot read input directory '%s'\n", options->input_dir);
        return CORPUS_RUN_ERR_INPUT_DIR;
    }

    char *default_output = NULL;
    const char *output_dir = options->output_dir;
    if (!output_dir) {
        default_output = corpus_join(options->input_dir, CORPUS_DEFAULT_OUTPUT_NAME);
        if (!default_output) {
            fprintf(stderr, "Error: out of memory\n");
            return CORPUS_RUN_ERR_OUTPUT_DIR;
        }
        output_dir = default_output;
    }
    if (!corpus_make_dirs(output_dir)) {
        fprintf(stderr, "Error: cannot create output directory '%s'\n", output_dir);
        free(default_output);
        return CORPUS_RUN_ERR_OUTPUT_DIR;
    }

    struct stat output_info;
    bool have_output_info = stat(output_dir, &output_info) == 0;

    PieceList pieces = {0};
    PieceRecord *records = NULL;
    CorpusRunResult result = CORPUS_RUN_OK;
    if (!walk_directory(options->input_dir, "", have_output_info ? &output_info : NULL, &pieces)) {
        fprintf(stderr, "Error: out of memory while listing '%s'\n", options->input_dir);
        result = CORPUS_RUN_ERR_INPUT_DIR;
    } else {
        qsort(pieces.paths, pieces.count, sizeof(char *), compare_paths);
        records = calloc(pieces.count ? pieces.count : 1, sizeof(PieceRecord));
        if (!records) {
            fprintf(stderr, "Error: out of memory\n");
            result = CORPUS_RUN_ERR_WRITE;
        }
    }

    cJSON *previous = NULL;
    if (result == CORPUS_RUN_OK && options->resume) {
        result = load_previous_index(output_dir, &previous);
    }

    size_t done = 0;
    for (; result == CORPUS_RUN_OK && done < pieces.count; done++) {
        const char *relative = pieces.paths[done];
        char *input_path = corpus_join(options->input_dir, relative);
        if (!input_path) {
            fprintf(stderr, "Error: out of memory\n");
            result = CORPUS_RUN_ERR_WRITE;
            break;
        }
        PieceRecord *record = &records[done];
        if (!(previous && reuse_previous(previous, options, output_dir, relative, fnv1a_file(input_path), record))) {
            result = analyze_piece(options, output_dir, relative, input_path, record);
        }
        free(input_path);
        if (result != CORPUS_RUN_OK) {
            break;
        }
        if (!write_index(output_dir, options->input_dir, (const char *const *)pieces.paths, records, done + 1)) {
            fprintf(stderr, "Error: cannot write the corpus index in '%s'\n", output_dir);
            result = CORPUS_RUN_ERR_WRITE;
            done++;
            break;
        }
        if (record->status == CORPUS_PIECE_ANALYZED) {
            summary.analyzed++;
        } else if (record->status == CORPUS_PIECE_SKIPPED) {
            summary.skipped++;
        } else {
            summary.failed++;
        }
        if (options->on_piece) {
            options->on_piece(relative, record->status, record->error, options->user);
        }
    }
    if (result == CORPUS_RUN_OK && pieces.count == 0 &&
        !write_index(output_dir, options->input_dir, (const char *const *)pieces.paths, records, 0)) {
        fprintf(stderr, "Error: cannot write the corpus index in '%s'\n", output_dir);
        result = CORPUS_RUN_ERR_WRITE;
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
