#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/markdown_report.h>

#include "test_support.h"

/*
 * Golden snapshots of the Markdown report for the five reference pieces
 * (Ursatz framework-v1 corpus), from the MIDI files and from their MusicXML
 * versions (tests/musicxml). Each piece runs import -> pipeline ->
 * analysis document -> Markdown and is compared byte for byte with
 * tests/snapshots/<slug>.report.md. On a mismatch the first differing
 * line is printed. With UPDATE_SNAPSHOTS=1 in the environment, the
 * golden files are rewritten instead.
 *
 * The document is built from the fixture's file name alone, so the
 * report carries no absolute path.
 */

static const struct {
    const char *fixture;
    const char *slug;
    bool musicxml;
} PIECES[] = {
    {"Bach 114.mid", "bach-114", false},       {"Mozart 545.mid", "mozart-545", false},
    {"Clementi 36.mid", "clementi-36", false}, {"Field 1.mid", "field-1", false},
    {"Field 5.mid", "field-5", false},         {"bach-114.musicxml", "bach-114-musicxml", true},
    {"mozart-545.musicxml", "mozart-545-musicxml", true}, {"clementi-36.musicxml", "clementi-36-musicxml", true},
    {"field-1.musicxml", "field-1-musicxml", true},       {"field-5.musicxml", "field-5-musicxml", true},
};

static char *render_piece(const char *fixture, bool musicxml) {
    char path[512];
    if (musicxml) {
        snprintf(path, sizeof(path), "%s/%s", URSATZ_MUSICXML_DIR, fixture);
    } else {
        snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_FIXTURE_DIR, fixture);
    }

    ImportedNotes notes;
    if (input_pipeline_import_file(path, INPUT_FORMAT_AUTO, &notes) != INPUT_PIPELINE_OK) {
        return NULL;
    }
    AnalysisReport report;
    if (!run_analysis_pipeline(&notes, &report)) {
        imported_notes_destroy(&notes);
        return NULL;
    }
    AnalysisDocument *document = analysis_document_build(fixture, &notes, &report);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    if (!document) {
        return NULL;
    }
    char *markdown = markdown_report_render(document);
    analysis_document_destroy(document);
    return markdown;
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

/* Prints the 1-based number and both versions of the first line where
   expected and actual differ. */
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
    bool update = update_env && strcmp(update_env, "1") == 0;
    int failures = 0;

    for (size_t i = 0; i < sizeof(PIECES) / sizeof(PIECES[0]); i++) {
        char snapshot_path[512];
        snprintf(snapshot_path, sizeof(snapshot_path), "%s/%s.report.md", URSATZ_SNAPSHOT_DIR, PIECES[i].slug);

        char *actual = render_piece(PIECES[i].fixture, PIECES[i].musicxml);
        if (!actual) {
            fprintf(stderr, "%s: failed to render the report\n", PIECES[i].slug);
            failures++;
            continue;
        }

        if (update) {
            if (write_file(snapshot_path, actual)) {
                printf("%s: snapshot updated\n", PIECES[i].slug);
            } else {
                fprintf(stderr, "%s: cannot write %s\n", PIECES[i].slug, snapshot_path);
                failures++;
            }
            free(actual);
            continue;
        }

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
        free(actual);
    }

    return failures == 0 ? 0 : 1;
}
