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

#include "pipeline/corpus_internal.h"

#define ANALYSIS_DOCUMENT_SUFFIX ".analysis.json"
#define REPORT_SUFFIX ".report.md"

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

/* Imports, analyzes and writes one piece. On failure of the piece itself sets *error and returns OK; only a failure
   to write an output file is a run error. */
static CorpusRunResult analyze_piece(const CorpusRunOptions *options, const char *output_dir, const char *relative,
                                     const char *input_path, CorpusPieceStatus *status, const char **error) {
    *status = CORPUS_PIECE_FAILED;
    *error = NULL;

    ImportedNotes notes;
    InputPipelineResult imported = input_pipeline_import_file(input_path, INPUT_FORMAT_AUTO, &notes);
    if (imported != INPUT_PIPELINE_OK) {
        *error = import_error_text(imported);
        return CORPUS_RUN_OK;
    }

    AnalysisReport report;
    if (!run_analysis_pipeline(&notes, &report)) {
        imported_notes_destroy(&notes);
        *error = "out of memory";
        return CORPUS_RUN_OK;
    }
    AnalysisDocument *document = analysis_document_build(input_path, &notes, &report);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    if (!document) {
        *error = "out of memory";
        return CORPUS_RUN_OK;
    }

    char *json = analysis_document_to_json(document);
    char *markdown = options->markdown ? markdown_report_render(document) : NULL;
    analysis_document_destroy(document);
    if (!json || (options->markdown && !markdown)) {
        free(json);
        free(markdown);
        *error = "out of memory";
        return CORPUS_RUN_OK;
    }

    bool wrote = write_output_file(output_dir, relative, ANALYSIS_DOCUMENT_SUFFIX, json) &&
                 (!markdown || write_output_file(output_dir, relative, REPORT_SUFFIX, markdown));
    free(json);
    free(markdown);
    if (!wrote) {
        return CORPUS_RUN_ERR_WRITE;
    }
    *status = CORPUS_PIECE_ANALYZED;
    return CORPUS_RUN_OK;
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
    CorpusRunResult result = CORPUS_RUN_OK;
    if (!walk_directory(options->input_dir, "", have_output_info ? &output_info : NULL, &pieces)) {
        fprintf(stderr, "Error: out of memory while listing '%s'\n", options->input_dir);
        result = CORPUS_RUN_ERR_INPUT_DIR;
    } else {
        qsort(pieces.paths, pieces.count, sizeof(char *), compare_paths);
    }

    for (size_t i = 0; result == CORPUS_RUN_OK && i < pieces.count; i++) {
        const char *relative = pieces.paths[i];
        char *input_path = corpus_join(options->input_dir, relative);
        if (!input_path) {
            fprintf(stderr, "Error: out of memory\n");
            result = CORPUS_RUN_ERR_WRITE;
            break;
        }
        CorpusPieceStatus status;
        const char *error;
        result = analyze_piece(options, output_dir, relative, input_path, &status, &error);
        free(input_path);
        if (result != CORPUS_RUN_OK) {
            break;
        }
        if (status == CORPUS_PIECE_ANALYZED) {
            summary.analyzed++;
        } else {
            summary.failed++;
        }
        if (options->on_piece) {
            options->on_piece(relative, status, error, options->user);
        }
    }

    piece_list_destroy(&pieces);
    free(default_output);
    if (out) {
        *out = summary;
    }
    return result;
}
