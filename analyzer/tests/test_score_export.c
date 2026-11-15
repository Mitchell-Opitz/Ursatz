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
 *   - each note's base xml:id (analyzer_score_note_xml_ids) appears in the export, as the id or, for a tied note, the id
 *     and a fragment suffix; the count is the note count and the ids differ,
 *   - the meter of a file that declares one is not defaulted; a MIDI file with no time signature is,
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

static bool id_in_export(const char *xml, const char *id) {
    char exact[128];
    char suffixed[128];
    snprintf(exact, sizeof(exact), "xml:id=\"%s\"", id);
    snprintf(suffixed, sizeof(suffixed), "xml:id=\"%s_", id);
    return strstr(xml, exact) != NULL || strstr(xml, suffixed) != NULL;
}

static void check_note_ids(size_t i, const ImportedNotes *notes, const char *xml) {
    char **ids = NULL;
    size_t count = 99;
    AnalyzerStatus status = analyzer_score_note_xml_ids(notes, &ids, &count);
    CHECK(status == ANALYZER_OK, "%s: note ids status %d", FILES[i].stem, (int)status);
    if (status != ANALYZER_OK) {
        return;
    }
    CHECK(count == notes->count, "%s: %zu ids for %zu notes", FILES[i].stem, count, notes->count);
    size_t missing = 0;
    size_t repeated = 0;
    for (size_t n = 0; n < count; n++) {
        if (!id_in_export(xml, ids[n])) {
            missing++;
        }
        if (n > 0 && strcmp(ids[n], ids[n - 1]) == 0) {
            repeated++;
        }
    }
    CHECK(missing == 0, "%s: %zu note ids are not in the export", FILES[i].stem, missing);
    CHECK(repeated == 0, "%s: %zu note ids repeat the one before", FILES[i].stem, repeated);
    analyzer_score_note_xml_ids_destroy(ids, count);
}

static void check_meter_defaulted(void) {
    for (size_t i = 0; i < sizeof(FILES) / sizeof(FILES[0]); i++) {
        ImportedNotes notes;
        if (!import_file(i, &notes)) {
            CHECK(false, "%s: cannot import", FILES[i].file);
            continue;
        }
        CHECK(!analyzer_imported_meter_defaulted(&notes), "%s: the declared meter reads as defaulted", FILES[i].stem);
        imported_notes_destroy(&notes);
    }

    /* Five quarter notes at division 96 and no time signature event. */
    unsigned char track[] = {0x00, 0x90, 0x3C, 0x64, 0x60, 0x80, 0x3C, 0x40, 0x00, 0x90, 0x3E, 0x64, 0x60, 0x80, 0x3E, 0x40,
                             0x00, 0x90, 0x40, 0x64, 0x60, 0x80, 0x40, 0x40, 0x00, 0x90, 0x41, 0x64, 0x60, 0x80, 0x41, 0x40,
                             0x00, 0x90, 0x43, 0x64, 0x60, 0x80, 0x43, 0x40, 0x00, 0xFF, 0x2F, 0x00};
    unsigned char buf[256];
    size_t size = test_smf_type0(buf, 96, track, sizeof(track));
    char path[512];
    snprintf(path, sizeof(path), "%s/no-meter.mid", URSATZ_ANALYZER_TMP_DIR);
    FILE *file = fopen(path, "wb");
    if (!file || fwrite(buf, 1, size, file) != size) {
        CHECK(false, "cannot write %s", path);
        if (file) {
            fclose(file);
        }
        return;
    }
    fclose(file);
    ImportedNotes notes;
    if (input_pipeline_import_file(path, INPUT_FORMAT_AUTO, &notes) != INPUT_PIPELINE_OK) {
        CHECK(false, "cannot import %s", path);
    } else {
        CHECK(analyzer_imported_meter_defaulted(&notes), "a MIDI file with no time signature does not read as defaulted");
        imported_notes_destroy(&notes);
    }
    remove(path);
    CHECK(!analyzer_imported_meter_defaulted(NULL), "NULL notes read as defaulted");
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
    char **ids = (char **)1;
    size_t id_count = 7;
    CHECK(analyzer_score_note_xml_ids(NULL, &ids, &id_count) == ANALYZER_ERR_ARGUMENT && ids == NULL && id_count == 0,
          "ids: NULL notes");
    CHECK(analyzer_score_note_xml_ids(&notes, NULL, &id_count) == ANALYZER_ERR_ARGUMENT, "ids: NULL out");
    CHECK(analyzer_score_note_xml_ids(&notes, &ids, NULL) == ANALYZER_ERR_ARGUMENT, "ids: NULL count");
    analyzer_score_note_xml_ids_destroy(NULL, 0);
    NoteEvent **events = notes.events;
    notes.events = NULL;
    xml = (char *)1;
    CHECK(analyzer_score_export_musicxml(&notes, &xml) == ANALYZER_ERR_ARGUMENT && xml == NULL, "notes with no events");
    ids = (char **)1;
    CHECK(analyzer_score_note_xml_ids(&notes, &ids, &id_count) == ANALYZER_ERR_ARGUMENT && ids == NULL,
          "ids: notes with no events");
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
            check_note_ids(i, &notes, xml);
            printf("%s: %zu bytes\n", FILES[i].stem, strlen(xml));
        }
        free(xml);
        imported_notes_destroy(&notes);
    }
    check_meter_defaulted();
    check_arguments();
    if (failures > 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all score export checks passed\n");
    return 0;
}
