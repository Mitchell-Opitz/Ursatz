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
#include <ursatz_analyzer/style_profile.h>

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
            "       %s --corpus <dir> [--out <dir>] [--resume] [--markdown]\n"
            "       %s --style-profile <corpus output dir> --name <style name> [--out <file>]\n"
            "       %s --compare-styles <a.json> <b.json> [--format text|json]\n",
            program, program, program, program);
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

static double rational_value(StyleRational r) {
    return (double)r.numerator / (double)r.denominator;
}

static void print_json_string(const char *text) {
    putchar('"');
    for (; *text; text++) {
        if (*text == '"' || *text == '\\') {
            putchar('\\');
        }
        if ((unsigned char)*text < 0x20) {
            printf("\\u%04x", (unsigned char)*text);
        } else {
            putchar(*text);
        }
    }
    putchar('"');
}

static void print_json_rational(StyleRational r) {
    printf("{\"numerator\": %llu, \"denominator\": %llu, \"value\": %.6f}", r.numerator, r.denominator,
           rational_value(r));
}

static void print_json_labels(char *const labels[3]) {
    printf("[");
    for (int k = 0; k < 3 && labels[k]; k++) {
        printf(k ? ", " : "");
        print_json_string(labels[k]);
    }
    printf("]");
}

static void print_text_labels(const char *tag, char *const labels[3]) {
    printf(" %s:", tag);
    if (!labels[0]) {
        printf(" -");
    }
    for (int k = 0; k < 3 && labels[k]; k++) {
        printf("%s %s", k ? "," : "", labels[k]);
    }
}

static void print_comparison_text(const StyleComparison *comparison) {
    if (comparison->overall_present) {
        printf("overall %.4f (%llu/%llu)\n", rational_value(comparison->overall), comparison->overall.numerator,
               comparison->overall.denominator);
    }
    for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
        if (!comparison->dimension_present[d]) {
            continue;
        }
        StyleRational r = comparison->dimension[d];
        printf("%s %.4f (%llu/%llu)\n", style_dimension_name((StyleDimension)d), rational_value(r), r.numerator,
               r.denominator);
        for (size_t i = 0; i < comparison->category_count; i++) {
            const StyleCategoryDistance *c = &comparison->categories[i];
            if (c->dimension != (StyleDimension)d) {
                continue;
            }
            printf("  %s %.4f (%llu/%llu)", c->category, rational_value(c->distance), c->distance.numerator,
                   c->distance.denominator);
            print_text_labels("a", c->top_a);
            printf(";");
            print_text_labels("b", c->top_b);
            printf("\n");
        }
    }
}

static void print_comparison_json(const StyleComparison *comparison) {
    printf("{\n  \"overall\": ");
    if (comparison->overall_present) {
        print_json_rational(comparison->overall);
    } else {
        printf("null");
    }
    printf(",\n  \"dimensions\": [");
    bool first = true;
    for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
        if (!comparison->dimension_present[d]) {
            continue;
        }
        printf("%s\n    {\"dimension\": \"%s\", \"distance\": ", first ? "" : ",", style_dimension_name((StyleDimension)d));
        print_json_rational(comparison->dimension[d]);
        printf("}");
        first = false;
    }
    printf("\n  ],\n  \"categories\": [");
    for (size_t i = 0; i < comparison->category_count; i++) {
        const StyleCategoryDistance *c = &comparison->categories[i];
        printf("%s\n    {\"dimension\": \"%s\", \"category\": ", i ? "," : "", style_dimension_name(c->dimension));
        print_json_string(c->category);
        printf(", \"distance\": ");
        print_json_rational(c->distance);
        printf(", \"top_a\": ");
        print_json_labels(c->top_a);
        printf(", \"top_b\": ");
        print_json_labels(c->top_b);
        printf("}");
    }
    printf("\n  ]\n}\n");
}

static int run_compare_styles(const char *path_a, const char *path_b, bool json) {
    StyleModel *a = NULL, *b = NULL;
    if (style_model_file_read(path_a, &a, NULL) != STYLE_MODEL_FILE_OK) {
        fprintf(stderr, "Error: cannot read style model '%s'\n", path_a);
        return 2;
    }
    if (style_model_file_read(path_b, &b, NULL) != STYLE_MODEL_FILE_OK) {
        fprintf(stderr, "Error: cannot read style model '%s'\n", path_b);
        style_model_destroy(a);
        return 2;
    }
    StyleComparison comparison;
    StyleCompareResult compared = style_profile_compare(a, b, &comparison);
    style_model_destroy(a);
    style_model_destroy(b);
    if (compared != STYLE_COMPARE_OK) {
        fprintf(stderr, "Error: %s\n",
                compared == STYLE_COMPARE_ERR_OVERFLOW ? "the distance does not fit in 64 bits" : "cannot compare the models");
        return 2;
    }
    if (json) {
        print_comparison_json(&comparison);
    } else {
        print_comparison_text(&comparison);
    }
    style_comparison_destroy(&comparison);
    return 0;
}

static int run_style_profile(const char *corpus_output_dir, const char *name, const char *out_file) {
    StyleProfile *profile = style_profile_create();
    if (!profile) {
        fprintf(stderr, "Error: out of memory\n");
        return 2;
    }
    StyleProfileResult added = style_profile_add_corpus(profile, corpus_output_dir);
    if (added != STYLE_PROFILE_OK) {
        fprintf(stderr, "Error: cannot read the corpus in '%s'\n", corpus_output_dir);
        style_profile_destroy(profile);
        return 2;
    }
    char *default_path = NULL;
    if (!out_file) {
        default_path = malloc(strlen(corpus_output_dir) + 18);
        if (!default_path) {
            fprintf(stderr, "Error: out of memory\n");
            style_profile_destroy(profile);
            return 2;
        }
        sprintf(default_path, "%s/style-model.json", corpus_output_dir);
        out_file = default_path;
    }
    int exit_code = 0;
    if (style_model_file_write(profile, name, out_file) != STYLE_MODEL_FILE_OK) {
        fprintf(stderr, "Error: cannot write '%s'\n", out_file);
        exit_code = 2;
    } else {
        printf("Style model '%s' from %zu pieces written to %s\n", name, style_profile_piece_count(profile), out_file);
    }
    free(default_path);
    style_profile_destroy(profile);
    return exit_code;
}

int main(int argc, char *argv[]) {
    const char *path = NULL;
    const char *compare_a = NULL;
    const char *compare_b = NULL;
    bool format_json = false;
    bool format_set = false;
    const char *style_dir = NULL;
    const char *style_name = NULL;
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
        } else if (strcmp(arg, "--compare-styles") == 0) {
            if (i + 2 >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            compare_a = argv[++i];
            compare_b = argv[++i];
        } else if (strcmp(arg, "--format") == 0) {
            if (i + 1 >= argc || (strcmp(argv[i + 1], "text") != 0 && strcmp(argv[i + 1], "json") != 0)) {
                print_usage(argv[0]);
                return 1;
            }
            format_json = strcmp(argv[++i], "json") == 0;
            format_set = true;
        } else if (strcmp(arg, "--style-profile") == 0 || strcmp(arg, "--name") == 0) {
            if (i + 1 >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            if (strcmp(arg, "--name") == 0) {
                style_name = argv[++i];
            } else {
                style_dir = argv[++i];
            }
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
    if (compare_a) {
        if (style_dir || style_name || corpus_dir || corpus_out || path || want_json || want_markdown || corpus_resume ||
            species != 0 || cantus_set || input_format_set) {
            print_usage(argv[0]);
            return 1;
        }
        return run_compare_styles(compare_a, compare_b, format_json);
    }
    if (format_set) {
        print_usage(argv[0]);
        return 1;
    }
    if (style_dir || style_name) {
        if (!style_dir || !style_name || !style_name[0] || corpus_dir || path || want_json || want_markdown || corpus_resume ||
            species != 0 || cantus_set || input_format_set) {
            print_usage(argv[0]);
            return 1;
        }
        return run_style_profile(style_dir, style_name, corpus_out);
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
