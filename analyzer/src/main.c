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
#include <ursatz_analyzer/similar_material.h>
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
            "       %s --compare-styles <a.json> <b.json> [--format text|json]\n"
            "       %s --similar <corpus output dir> (--motive <file> | --progression \"<numerals>\") [--min <n/d>] "
            "[--max <n>] [--format text|json]\n",
            program, program, program, program, program);
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
    char fraction[100];
    style_rational_format(r, fraction, sizeof fraction);
    printf("{\"fraction\": \"%s\", \"value\": %.6f}", fraction, style_rational_value(r));
}

/* "0.1234 (n/d)" */
static void print_text_rational(StyleRational r) {
    char fraction[100];
    style_rational_format(r, fraction, sizeof fraction);
    printf("%.4f (%s)", style_rational_value(r), fraction);
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
        printf("overall ");
        print_text_rational(comparison->overall);
        printf("\n");
    }
    for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
        if (!comparison->dimension_present[d]) {
            continue;
        }
        printf("%s ", style_dimension_name((StyleDimension)d));
        print_text_rational(comparison->dimension[d]);
        printf("\n");
        for (size_t i = 0; i < comparison->category_count; i++) {
            const StyleCategoryDistance *c = &comparison->categories[i];
            if (c->dimension != (StyleDimension)d) {
                continue;
            }
            printf("  %s ", c->category);
            print_text_rational(c->distance);
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

static void print_json_fraction(unsigned long numerator, unsigned long denominator) {
    printf("\"%lu/%lu\"", numerator, denominator);
}

static void print_json_score(SimilarityScore score) {
    printf("{\"fraction\": ");
    print_json_fraction(score.numerator, score.denominator);
    printf(", \"value\": %.6f}", (double)score.numerator / (double)score.denominator);
}

static void print_similar_json(const SimilarQuery *query, unsigned long min_numerator, unsigned long min_denominator,
                               const SimilarResults *results) {
    printf("{\n  \"schema\": \"ursatz-similar-material\",\n  \"schema_version\": 1,\n  \"query\": {\"kind\": ");
    if (query->kind == SIMILAR_QUERY_MOTIVE) {
        printf("\"motive\", \"notes\": [");
        for (size_t i = 0; i < query->note_count; i++) {
            const SimilarNote *note = &query->notes[i];
            printf("%s{\"pitch\": %d, \"duration\": ", i ? ", " : "", note->pitch);
            print_json_fraction(note->duration_numerator, note->duration_denominator);
            printf("}");
        }
    } else {
        printf("\"progression\", \"numerals\": [");
        for (size_t i = 0; i < query->numeral_count; i++) {
            printf(i ? ", " : "");
            print_json_string(query->numerals[i]);
        }
    }
    printf("]},\n  \"minimum\": ");
    print_json_fraction(min_numerator, min_denominator);
    printf(",\n  \"pieces_searched\": %zu,\n  \"matches\": [", results->pieces_searched);
    for (size_t i = 0; i < results->count; i++) {
        const SimilarMatch *m = &results->matches[i];
        printf("%s\n    {\"piece\": ", i ? "," : "");
        print_json_string(m->piece);
        printf(", \"document\": ");
        print_json_string(m->document);
        printf(",\n     \"start\": {\"measure\": %d, \"beat\": %g}, \"end\": {\"measure\": %d, \"beat\": %g},\n     \"score\": ",
               m->start_measure, m->start_beat, m->end_measure, m->end_beat);
        print_json_score(m->score.similarity);
        printf(",\n     \"dimensions\": [");
        for (size_t d = 0; d < m->score.dimension_score_count; d++) {
            printf("%s{\"dimension\": \"%s\", \"score\": ", d ? ", " : "",
                   similarity_dimension_name(m->score.dimension_scores[d].dimension));
            print_json_score(m->score.dimension_scores[d].score);
            printf("}");
        }
        printf("],\n     \"numeral_matches\": %zu,\n     \"notes\": [", m->numeral_matches);
        for (size_t k = 0; k < m->note_count; k++) {
            const SimilarNote *note = &m->notes[k];
            printf("%s{\"pitch\": %d, \"onset\": ", k ? ", " : "", note->pitch);
            print_json_fraction(note->onset_numerator, note->onset_denominator);
            printf(", \"duration\": ");
            print_json_fraction(note->duration_numerator, note->duration_denominator);
            printf("}");
        }
        printf("]}");
    }
    printf("\n  ]\n}\n");
}

static void print_similar_text(const SimilarQuery *query, const SimilarResults *results) {
    if (results->count == 0) {
        printf("no match in %zu pieces\n", results->pieces_searched);
        return;
    }
    for (size_t i = 0; i < results->count; i++) {
        const SimilarMatch *m = &results->matches[i];
        printf("%.4f (%lu/%lu) %s m.%d b.%g - m.%d b.%g", (double)m->score.similarity.numerator / (double)m->score.similarity.denominator,
               m->score.similarity.numerator, m->score.similarity.denominator, m->piece, m->start_measure, m->start_beat,
               m->end_measure, m->end_beat);
        for (size_t d = 0; d < m->score.dimension_score_count; d++) {
            printf(" %s %lu/%lu", similarity_dimension_name(m->score.dimension_scores[d].dimension),
                   m->score.dimension_scores[d].score.numerator, m->score.dimension_scores[d].score.denominator);
        }
        if (query->kind == SIMILAR_QUERY_PROGRESSION) {
            printf(" numerals %zu/%zu", m->numeral_matches, query->numeral_count);
        }
        printf("\n");
    }
}

static int run_similar(const char *corpus_output_dir, const char *motive_file, const char *progression, const SimilarOptions *options,
                       bool json) {
    SimilarQuery query = {0};
    SimilarResult read = motive_file ? similar_query_from_file(motive_file, &query)
                                     : similar_query_from_numerals(progression, &query);
    if (read == SIMILAR_ERR_QUERY || read == SIMILAR_ERR_ARGUMENT) {
        fprintf(stderr, "Error: %s\n",
                motive_file ? "the motive must have at least two notes" : "the progression must be two or more Roman numerals");
        return 1;
    }
    if (read != SIMILAR_OK) {
        if (read != SIMILAR_ERR_IO) {
            fprintf(stderr, "Error: cannot read the motive '%s'\n", motive_file);
        }
        return 2;
    }
    SimilarResults results = {0};
    SimilarResult found = similar_material_find(corpus_output_dir, &query, options, &results);
    int exit_code = 0;
    if (found == SIMILAR_ERR_QUERY) {
        fprintf(stderr, "Error: the query is not a motive or a progression this search can score\n");
        exit_code = 1;
    } else if (found != SIMILAR_OK) {
        fprintf(stderr, "Error: cannot read the corpus in '%s'\n", corpus_output_dir);
        exit_code = 2;
    } else if (json) {
        print_similar_json(&query, options->minimum_numerator, options->minimum_denominator, &results);
    } else {
        print_similar_text(&query, &results);
    }
    similar_results_destroy(&results);
    similar_query_destroy(&query);
    return exit_code;
}

/* "n/d" with 0 <= n <= d and d > 0. */
static bool parse_minimum(const char *text, unsigned long *numerator, unsigned long *denominator) {
    int used = 0;
    unsigned long n, d;
    if (text[0] == '-' || sscanf(text, "%lu/%lu%n", &n, &d, &used) != 2 || text[used] != '\0' || d == 0 || n > d) {
        return false;
    }
    *numerator = n;
    *denominator = d;
    return true;
}

static bool parse_count(const char *text, size_t *out) {
    char *end;
    unsigned long value = strtoul(text, &end, 10);
    if (text[0] < '0' || text[0] > '9' || *end != '\0' || value == 0) {
        return false;
    }
    *out = (size_t)value;
    return true;
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
    const char *similar_dir = NULL;
    const char *motive_file = NULL;
    const char *progression = NULL;
    bool similar_flags = false; /* --min or --max given */
    SimilarOptions similar_options = similar_options_default();
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
        } else if (strcmp(arg, "--similar") == 0 || strcmp(arg, "--motive") == 0 || strcmp(arg, "--progression") == 0) {
            if (i + 1 >= argc) {
                print_usage(argv[0]);
                return 1;
            }
            const char **target = strcmp(arg, "--similar") == 0 ? &similar_dir
                                  : strcmp(arg, "--motive") == 0 ? &motive_file
                                                                 : &progression;
            *target = argv[++i];
        } else if (strcmp(arg, "--min") == 0 || strcmp(arg, "--max") == 0) {
            bool minimum = strcmp(arg, "--min") == 0;
            if (i + 1 >= argc ||
                !(minimum ? parse_minimum(argv[i + 1], &similar_options.minimum_numerator, &similar_options.minimum_denominator)
                          : parse_count(argv[i + 1], &similar_options.max_results))) {
                print_usage(argv[0]);
                return 1;
            }
            i++;
            similar_flags = true;
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
    if (similar_dir || motive_file || progression || similar_flags) {
        if (!similar_dir || (motive_file != NULL) == (progression != NULL) || compare_a || style_dir || style_name ||
            corpus_dir || corpus_out || path || want_json || want_markdown || corpus_resume || species != 0 || cantus_set ||
            input_format_set) {
            print_usage(argv[0]);
            return 1;
        }
        return run_similar(similar_dir, motive_file, progression, &similar_options, format_json);
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
