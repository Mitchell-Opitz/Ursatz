#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/text_export.h>

#include "test_support.h"

/*
 * Golden snapshots of the text export of the five reference pieces (Ursatz framework-v1 corpus, MIDI files). Each piece
 * runs import -> analysis -> document -> text export and is compared with tests/snapshots/<slug>.export.txt after the
 * "analyzer:" line (the analyzer version and the Ursatz commit, which change with a release) is removed. With
 * UPDATE_SNAPSHOTS=1 (or URSATZ_UPDATE_SNAPSHOTS=1) in the environment the golden files are rewritten instead.
 *
 * Size rule: an export is at most EXPORT_BYTES_PER_MEASURE bytes per measure plus EXPORT_BYTES_BASE bytes, checked on
 * the text as written, whatever the snapshot says. The plan was 500 bytes per measure; Field 5 (two staves, 12/8, four
 * voices) is 650 per measure with 496 of them the notes, so the rule is 700.
 */

#define EXPORT_BYTES_PER_MEASURE 700
#define EXPORT_BYTES_BASE 2048

static const struct {
    const char *fixture;
    const char *slug;
} PIECES[] = {
    {"Bach 114.mid", "bach-114"}, {"Mozart 545.mid", "mozart-545"},   {"Clementi 36.mid", "clementi-36"},
    {"Field 1.mid", "field-1"},   {"Field 5.mid", "field-5"},
};

/* The export of a piece, or NULL. */
static char *export_piece(const char *fixture) {
    char path[512];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_ANALYZER_FIXTURE_DIR, fixture);

    ImportedNotes notes;
    if (input_pipeline_import_file(path, INPUT_FORMAT_AUTO, &notes) != INPUT_PIPELINE_OK) {
        return NULL;
    }
    AnalyzerResult *result = NULL;
    if (analyzer_analyze(&notes, NULL, &result) != ANALYZER_OK) {
        imported_notes_destroy(&notes);
        return NULL;
    }
    char *document = analysis_document_render_result(fixture, result);
    analyzer_result_destroy(result);
    char *text = NULL;
    if (document) {
        AnalyzerTextExportOptions options = analyzer_text_export_options_default();
        if (analyzer_text_export_piece(&notes, document, &options, &text) != ANALYZER_OK) {
            text = NULL;
        }
    }
    free(document);
    imported_notes_destroy(&notes);
    return text;
}

/* The text without its "analyzer: ..." line; a new string, or NULL. */
static char *without_analyzer_line(const char *text) {
    const char *line = strstr(text, "\nanalyzer: ");
    char *copy = malloc(strlen(text) + 1);
    if (!copy) {
        return NULL;
    }
    if (!line) {
        strcpy(copy, text);
        return copy;
    }
    size_t head = (size_t)(line - text) + 1;
    const char *rest = strchr(line + 1, '\n');
    memcpy(copy, text, head);
    strcpy(copy + head, rest ? rest + 1 : "");
    return copy;
}

static size_t measure_count(const char *text) {
    size_t count = 0;
    for (const char *line = text; line; line = strchr(line, '\n')) {
        if (*line == '\n') {
            line++;
        }
        if (line[0] == 'm' && line[1] >= '0' && line[1] <= '9') {
            count++;
        }
        if (!*line) {
            break;
        }
    }
    return count;
}

static bool write_file(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }
    size_t length = strlen(text);
    bool ok = fwrite(text, 1, length, file) == length;
    return fclose(file) == 0 && ok;
}

/* Prints the 1-based number and both versions of the first line where expected and actual differ. */
static void print_first_difference(const char *slug, const char *expected, const char *actual) {
    size_t line = 1;
    const char *expected_line = expected;
    const char *actual_line = actual;
    while (*expected && *expected == *actual) {
        if (*expected == '\n') {
            line++;
            expected_line = expected + 1;
            actual_line = actual + 1;
        }
        expected++;
        actual++;
    }
    fprintf(stderr, "%s: first difference at line %zu\n", slug, line);
    fprintf(stderr, "  expected: %.*s\n", (int)strcspn(expected_line, "\n"), expected_line);
    fprintf(stderr, "  actual:   %.*s\n", (int)strcspn(actual_line, "\n"), actual_line);
}

int main(void) {
    const char *update_env = getenv("UPDATE_SNAPSHOTS");
    if (!update_env) {
        update_env = getenv("URSATZ_UPDATE_SNAPSHOTS");
    }
    bool update = update_env && strcmp(update_env, "1") == 0;
    int failures = 0;

    for (size_t i = 0; i < sizeof(PIECES) / sizeof(PIECES[0]); i++) {
        char snapshot_path[512];
        snprintf(snapshot_path, sizeof(snapshot_path), "%s/%s.export.txt", URSATZ_SNAPSHOT_DIR, PIECES[i].slug);

        char *full = export_piece(PIECES[i].fixture);
        char *actual = full ? without_analyzer_line(full) : NULL;
        if (!full || !actual) {
            fprintf(stderr, "%s: failed to export the piece\n", PIECES[i].slug);
            failures++;
            free(full);
            free(actual);
            continue;
        }

        size_t measures = measure_count(full);
        size_t limit = EXPORT_BYTES_BASE + EXPORT_BYTES_PER_MEASURE * measures;
        if (measures == 0 || strlen(full) > limit) {
            fprintf(stderr, "%s: %zu bytes for %zu measures, over the limit of %zu\n", PIECES[i].slug, strlen(full),
                    measures, limit);
            failures++;
        }

        if (update) {
            if (write_file(snapshot_path, actual)) {
                printf("%s: snapshot updated (%zu bytes, %zu measures)\n", PIECES[i].slug, strlen(full), measures);
            } else {
                fprintf(stderr, "%s: cannot write %s\n", PIECES[i].slug, snapshot_path);
                failures++;
            }
        } else {
            char *expected = test_read_file(snapshot_path);
            if (!expected) {
                fprintf(stderr, "%s: missing %s (run with UPDATE_SNAPSHOTS=1 to create it)\n", PIECES[i].slug,
                        snapshot_path);
                failures++;
            } else if (strcmp(expected, actual) != 0) {
                print_first_difference(PIECES[i].slug, expected, actual);
                failures++;
            } else {
                printf("%s: matches snapshot\n", PIECES[i].slug);
            }
            free(expected);
        }
        free(full);
        free(actual);
    }

    return failures == 0 ? 0 : 1;
}
