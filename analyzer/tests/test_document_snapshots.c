#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/input_pipeline.h>

/*
 * Golden snapshots of the analysis document of the five reference pieces
 * (Ursatz framework-v1 corpus, MIDI files). Each piece runs import ->
 * pipeline -> analysis document and is compared with
 * tests/snapshots/<slug>.analysis.json after the run-dependent metadata
 * members (analyzer_version, ursatz_commit, input) are removed. With
 * UPDATE_SNAPSHOTS=1 (or URSATZ_UPDATE_SNAPSHOTS=1) in the environment the
 * golden files are rewritten instead.
 */

static const struct {
    const char *fixture;
    const char *slug;
} PIECES[] = {
    {"Bach 114.mid", "bach-114"}, {"Mozart 545.mid", "mozart-545"},   {"Clementi 36.mid", "clementi-36"},
    {"Field 1.mid", "field-1"},   {"Field 5.mid", "field-5"},
};

static char *render_piece(const char *fixture) {
    char path[512];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_FIXTURE_DIR, fixture);

    ImportedNotes notes;
    if (input_pipeline_import_file(path, INPUT_FORMAT_AUTO, &notes) != INPUT_PIPELINE_OK) {
        return NULL;
    }
    AnalysisReport report;
    if (!run_analysis_pipeline(&notes, &report)) {
        imported_notes_destroy(&notes);
        return NULL;
    }
    char *json = analysis_document_render(fixture, &notes, &report);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    if (!json) {
        return NULL;
    }

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root) {
        return NULL;
    }
    cJSON *metadata = cJSON_GetObjectItemCaseSensitive(root, "metadata");
    if (cJSON_IsObject(metadata)) {
        cJSON_DeleteItemFromObjectCaseSensitive(metadata, "analyzer_version");
        cJSON_DeleteItemFromObjectCaseSensitive(metadata, "ursatz_commit");
        cJSON_DeleteItemFromObjectCaseSensitive(metadata, "input");
    }
    char *text = cJSON_Print(root);
    cJSON_Delete(root);
    return text;
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *data = malloc((size_t)size + 1);
    if (data && fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        data = NULL;
    }
    fclose(file);
    if (data) {
        data[size] = '\0';
    }
    return data;
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
    if (!update_env) {
        update_env = getenv("URSATZ_UPDATE_SNAPSHOTS");
    }
    bool update = update_env && strcmp(update_env, "1") == 0;
    int failures = 0;

    for (size_t i = 0; i < sizeof(PIECES) / sizeof(PIECES[0]); i++) {
        char snapshot_path[512];
        snprintf(snapshot_path, sizeof(snapshot_path), "%s/%s.analysis.json", URSATZ_SNAPSHOT_DIR, PIECES[i].slug);

        char *actual = render_piece(PIECES[i].fixture);
        if (!actual) {
            fprintf(stderr, "%s: failed to render the document\n", PIECES[i].slug);
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

        char *expected = read_file(snapshot_path);
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
