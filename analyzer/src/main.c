#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/corpus.h>
#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/markdown_report.h>
#include <ursatz_analyzer/report_writer.h>

#include "pipeline/counterpoint_pipeline.h"

static bool write_markdown_report(const char *path, const AnalysisDocument *document) {
    char *markdown = markdown_report_render(document);
    if (!markdown) {
        fprintf(stderr, "Error: out of memory while rendering report for '%s'\n", path);
        return false;
    }

    char *output_path = derive_report_path(path);
    if (!output_path) {
        fprintf(stderr, "Error: out of memory while deriving report path for '%s'\n", path);
        free(markdown);
        return false;
    }

    bool wrote = write_report_file(output_path, markdown);
    if (wrote) {
        printf("Report written to %s\n", output_path);
    }

    free(output_path);
    free(markdown);
    return wrote;
}

static bool write_analysis_document(const char *path, const AnalysisDocument *analysis_document) {
    char *document = analysis_document_to_json(analysis_document);
    if (!document) {
        fprintf(stderr, "Error: out of memory while rendering analysis document for '%s'\n", path);
        return false;
    }

    char *output_path = derive_analysis_document_path(path);
    if (!output_path) {
        fprintf(stderr, "Error: out of memory while deriving analysis document path for '%s'\n", path);
        free(document);
        return false;
    }

    bool wrote = write_report_file(output_path, document);
    if (wrote) {
        printf("Analysis document written to %s\n", output_path);
    }

    free(output_path);
    free(document);
    return wrote;
}

static void print_usage(const char *program) {
    fprintf(stderr,
            "Usage: %s [--json] [--markdown] [--input-format midi|musicxml] [--check-counterpoint <1-5> "
            "[--cantus upper|lower]] <input-file-path>\n"
            "       %s --corpus <dir> [--out <dir>] [--resume] [--markdown]\n",
            program, program);
}

static void print_corpus_piece(const char *path, CorpusPieceStatus status, const char *error, void *user) {
    (void)user;
    const char *word = status == CORPUS_PIECE_ANALYZED ? "analyzed" : (status == CORPUS_PIECE_SKIPPED ? "skipped" : "failed");
    if (error) {
        printf("%s %s: %s\n", word, path, error);
    } else {
        printf("%s %s\n", word, path);
    }
}

static int run_corpus(const char *input_dir, const char *output_dir, bool resume, bool markdown) {
    CorpusRunOptions options = {input_dir, output_dir, resume, markdown, print_corpus_piece, NULL};
    CorpusRunSummary summary;
    if (corpus_run(&options, &summary) != CORPUS_RUN_OK) {
        return 2;
    }
    printf("%zu analyzed, %zu skipped, %zu failed\n", summary.analyzed, summary.skipped, summary.failed);
    return 0;
}

int main(int argc, char *argv[]) {
    const char *path = NULL;
    const char *corpus_dir = NULL;
    const char *corpus_out = NULL;
    bool corpus_resume = false;
    bool input_format_set = false;
    bool want_json = false;
    bool want_markdown = false;
    InputFormat input_format = INPUT_FORMAT_AUTO;
    int species = 0;
    bool cantus_set = false;
    bool cantus_upper = false;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--json") == 0) {
            want_json = true;
        } else if (strcmp(arg, "--markdown") == 0) {
            want_markdown = true;
        } else if (strcmp(arg, "--input-format") == 0) {
            if (i + 1 >= argc || !input_format_parse(argv[++i], &input_format)) {
                print_usage(argv[0]);
                return 1;
            }
            input_format_set = true;
        } else if (strcmp(arg, "--corpus") == 0 || strcmp(arg, "--out") == 0) {
            if (i + 1 >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            if (strcmp(arg, "--corpus") == 0) {
                corpus_dir = argv[++i];
            } else {
                corpus_out = argv[++i];
            }
        } else if (strcmp(arg, "--resume") == 0) {
            corpus_resume = true;
        } else if (strcmp(arg, "--check-counterpoint") == 0) {
            if (i + 1 >= argc || strlen(argv[i + 1]) != 1 || argv[i + 1][0] < '1' || argv[i + 1][0] > '5') {
                print_usage(argv[0]);
                return 1;
            }
            species = argv[++i][0] - '0';
        } else if (strcmp(arg, "--cantus") == 0) {
            if (i + 1 >= argc || (strcmp(argv[i + 1], "upper") != 0 && strcmp(argv[i + 1], "lower") != 0)) {
                print_usage(argv[0]);
                return 1;
            }
            cantus_upper = strcmp(argv[++i], "upper") == 0;
            cantus_set = true;
        } else if (arg[0] == '-' || path) {
            print_usage(argv[0]);
            return 1;
        } else {
            path = arg;
        }
    }
    if (corpus_dir) {
        if (path || want_json || species != 0 || cantus_set || input_format_set) {
            print_usage(argv[0]);
            return 1;
        }
        return run_corpus(corpus_dir, corpus_out, corpus_resume, want_markdown);
    }
    if (corpus_out || corpus_resume) {
        print_usage(argv[0]);
        return 1;
    }
    if (!path || (cantus_set && species == 0)) {
        print_usage(argv[0]);
        return 1;
    }
    if (!want_json && !want_markdown) {
        want_json = true;
        want_markdown = true;
    }

    ImportedNotes notes;
    if (input_pipeline_import_file(path, input_format, &notes) != INPUT_PIPELINE_OK) {
        return 1;
    }

    AnalysisReport report;
    if (!run_analysis_pipeline(&notes, &report)) {
        fprintf(stderr, "Error: out of memory while analyzing '%s'\n", path);
        imported_notes_destroy(&notes);
        return 1;
    }

    int exit_code = 0;
    if (species != 0) {
        CounterpointCheckResult checked = counterpoint_check_collect(&notes, species, cantus_upper, &report);
        if (checked == COUNTERPOINT_CHECK_ERR_ALLOC) {
            fprintf(stderr, "Error: out of memory while checking counterpoint in '%s'\n", path);
            analysis_report_destroy(&report);
            imported_notes_destroy(&notes);
            return 1;
        }
        if (checked == COUNTERPOINT_CHECK_NOT_TWO_VOICES) {
            fprintf(stderr, "Error: counterpoint check needs two voices\n");
            exit_code = 2;
        }
    }

    AnalysisDocument *document = analysis_document_build(path, &notes, &report);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    if (!document) {
        fprintf(stderr, "Error: out of memory while building analysis document for '%s'\n", path);
        return 1;
    }

    bool ok = (!want_markdown || write_markdown_report(path, document)) &&
              (!want_json || write_analysis_document(path, document));

    analysis_document_destroy(document);
    return ok ? exit_code : 1;
}
