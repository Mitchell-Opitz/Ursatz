#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/score_export.h>

#include "test_support.h"

/*
 * The MusicXML export of an import (score_export.h): the score the GUI wrote from the library's import.
 *   - the five reference pieces (MIDI) and tests/musicxml/pickup.musicxml, field-5.musicxml give the text of
 *     tests/snapshots/gui/<stem>.musicxml byte for byte; with UPDATE_SNAPSHOTS=1 (or URSATZ_UPDATE_SNAPSHOTS=1) the
 *     files are rewritten,
 *   - the export imports again with the same number of notes,
 *   - the argument errors.
 *
 *   test_score_export          the six files and the cases
 *   test_score_export smoke    Bach 114, pickup and the cases (the valgrind run)
 */

static const struct {
    const char *dir; /* "midi" or "musicxml" */
    const char *file;
    const char *stem;
    bool smoke;
} FILES[] = {
    {"midi", "Bach 114.mid", "bach-114", true},          {"midi", "Mozart 545.mid", "mozart-545", false},
    {"midi", "Clementi 36.mid", "clementi-36", false},   {"midi", "Field 1.mid", "field-1", false},
    {"midi", "Field 5.mid", "field-5", false},           {"musicxml", "pickup.musicxml", "pickup", true},
    {"musicxml", "field-5.musicxml", "field-5-musicxml", false},
};

static int failures = 0;

#define CHECK(condition, ...)                                    \
    do {                                                         \
        if (!(condition)) {                                      \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                        \
            fprintf(stderr, "\n");                               \
            failures++;                                          \
        }                                                        \
    } while (0)

static bool import_file(size_t i, ImportedNotes *notes) {
    char path[512];
    if (strcmp(FILES[i].dir, "midi") == 0) {
        snprintf(path, sizeof(path), "%s/framework-v1-corpus/%s", URSATZ_ANALYZER_FIXTURE_DIR, FILES[i].file);
    } else {
        snprintf(path, sizeof(path), "%s/%s", URSATZ_MUSICXML_DIR, FILES[i].file);
    }
    return input_pipeline_import_file(path, INPUT_FORMAT_AUTO, notes) == INPUT_PIPELINE_OK;
}

static void check_snapshot(size_t i, const char *xml, bool update) {
    char path[512];
    snprintf(path, sizeof(path), "%s/gui/%s.musicxml", URSATZ_SNAPSHOT_DIR, FILES[i].stem);
    if (update) {
        CHECK(test_write_file(path, xml), "%s: cannot write %s", FILES[i].stem, path);
        return;
    }
    char *expected = test_read_file(path);
    CHECK(expected != NULL, "%s: missing %s (run with UPDATE_SNAPSHOTS=1 to create it)", FILES[i].stem, path);
    if (expected) {
        CHECK(strcmp(expected, xml) == 0, "%s: MusicXML differs from %s", FILES[i].stem, path);
    }
    free(expected);
}

/* The export reads back with as many notes as the import had. */
static void check_reimport(size_t i, const ImportedNotes *notes, const char *xml) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.export.musicxml", URSATZ_ANALYZER_TMP_DIR, FILES[i].stem);
    if (!test_write_file(path, xml)) {
        CHECK(false, "%s: cannot write %s", FILES[i].stem, path);
        return;
    }
    ImportedNotes again;
    if (input_pipeline_import_file(path, INPUT_FORMAT_MUSICXML, &again) != INPUT_PIPELINE_OK) {
        CHECK(false, "%s: the export does not import", FILES[i].stem);
    } else {
        CHECK(again.count == notes->count, "%s: %zu notes after the export, %zu before", FILES[i].stem, again.count,
              notes->count);
        imported_notes_destroy(&again);
    }
    remove(path);
}

static void check_arguments(void) {
    ImportedNotes notes;
    char *xml = (char *)1;
    CHECK(analyzer_score_export_musicxml(NULL, &xml) == ANALYZER_ERR_ARGUMENT && xml == NULL, "NULL notes");
    if (!import_file(0, &notes)) {
        CHECK(false, "arguments: cannot import %s", FILES[0].file);
        return;
    }
    CHECK(analyzer_score_export_musicxml(&notes, NULL) == ANALYZER_ERR_ARGUMENT, "NULL out");
    NoteEvent **events = notes.events;
    notes.events = NULL;
    xml = (char *)1;
    CHECK(analyzer_score_export_musicxml(&notes, &xml) == ANALYZER_ERR_ARGUMENT && xml == NULL, "notes with no events");
    notes.events = events;
    imported_notes_destroy(&notes);
}

int main(int argc, char **argv) {
    bool smoke = argc > 1 && strcmp(argv[1], "smoke") == 0;
    const char *update_env = getenv("UPDATE_SNAPSHOTS");
    if (!update_env) {
        update_env = getenv("URSATZ_UPDATE_SNAPSHOTS");
    }
    bool update = update_env && strcmp(update_env, "1") == 0;

    for (size_t i = 0; i < sizeof(FILES) / sizeof(FILES[0]); i++) {
        if (smoke && !FILES[i].smoke) {
            continue;
        }
        ImportedNotes notes;
        if (!import_file(i, &notes)) {
            CHECK(false, "%s: cannot import", FILES[i].file);
            continue;
        }
        char *xml = NULL;
        AnalyzerStatus status = analyzer_score_export_musicxml(&notes, &xml);
        CHECK(status == ANALYZER_OK && xml != NULL, "%s: export status %d", FILES[i].stem, (int)status);
        if (xml) {
            CHECK(strncmp(xml, "<?xml", 5) == 0, "%s: not an XML document", FILES[i].stem);
            check_snapshot(i, xml, update);
            check_reimport(i, &notes, xml);
            printf("%s: %zu bytes\n", FILES[i].stem, strlen(xml));
        }
        free(xml);
        imported_notes_destroy(&notes);
    }
    check_arguments();
    if (failures > 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all score export checks passed\n");
    return 0;
}
