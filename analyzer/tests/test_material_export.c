#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/imported_notes.h>
#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/text_export.h>

#include "test_support.h"

/*
 * The material export (text_export.h) and the in-memory import it reads through (input_pipeline.h).
 *   - analyzer_text_export_material: a motif with segments a and a' and two variant sections, a theme with two slot
 *     sections, a piece with loop and ending, a section with no MusicXML, the refusals, the size rule (a 2-bar motif
 *     with 6 variants is under 8 KB) and golden snapshots tests/snapshots/material-<kind>.export.txt (UPDATE_SNAPSHOTS=1
 *     rewrites them; the "analyzer:" line is removed before comparing).
 *   - input_pipeline_import_memory: MusicXML and MIDI bytes read the same notes as the file; INPUT_FORMAT_AUTO tells
 *     them apart by their first bytes; refusals (no bytes, no format, malformed, compressed).
 */

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

#define MATERIAL_BYTES_MOTIF_WITH_VARIANTS 8192

static const char *update_snapshots_env(void) {
    const char *value = getenv("UPDATE_SNAPSHOTS");
    return value ? value : getenv("URSATZ_UPDATE_SNAPSHOTS");
}

static char *read_file(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *bytes = malloc((size_t)length + 1);
    if (bytes && fread(bytes, 1, (size_t)length, file) != (size_t)length) {
        free(bytes);
        bytes = NULL;
    }
    fclose(file);
    if (bytes) {
        bytes[length] = '\0';
        *size = (size_t)length;
    }
    return bytes;
}

/* Imports path from its bytes in memory and from the file; both must read the same number of notes. */
static void check_memory_matches_file(const char *path, InputFormat format, const char *what) {
    size_t size = 0;
    char *bytes = read_file(path, &size);
    CHECK(bytes != NULL, "%s: cannot read %s", what, path);
    if (!bytes) {
        return;
    }
    ImportedNotes from_file;
    ImportedNotes from_memory;
    InputPipelineResult file_result = input_pipeline_import_file(path, INPUT_FORMAT_AUTO, &from_file);
    InputPipelineResult memory_result = input_pipeline_import_memory(bytes, size, format, &from_memory);
    CHECK(file_result == INPUT_PIPELINE_OK, "%s: file import %d", what, (int)file_result);
    CHECK(memory_result == INPUT_PIPELINE_OK, "%s: memory import %d", what, (int)memory_result);
    if (file_result == INPUT_PIPELINE_OK && memory_result == INPUT_PIPELINE_OK) {
        CHECK(from_memory.count > 0, "%s: no notes", what);
        CHECK(from_memory.count == from_file.count, "%s: %zu notes from memory, %zu from the file", what,
              from_memory.count, from_file.count);
    }
    if (file_result == INPUT_PIPELINE_OK) {
        imported_notes_destroy(&from_file);
    }
    if (memory_result == INPUT_PIPELINE_OK) {
        imported_notes_destroy(&from_memory);
    }
    free(bytes);
}

static void test_import_memory(void) {
    const char *musicxml = URSATZ_MUSICXML_DIR "/pickup.musicxml";
    const char *compressed = URSATZ_MUSICXML_DIR "/pickup.mxl";
    const char *midi = URSATZ_ANALYZER_FIXTURE_DIR "/framework-v1-corpus/Field 1.mid";

    check_memory_matches_file(musicxml, INPUT_FORMAT_AUTO, "musicxml, auto");
    check_memory_matches_file(musicxml, INPUT_FORMAT_MUSICXML, "musicxml, named");
    check_memory_matches_file(midi, INPUT_FORMAT_AUTO, "midi, auto");
    check_memory_matches_file(midi, INPUT_FORMAT_MIDI, "midi, named");

    ImportedNotes notes;
    size_t size = 0;
    /* The importer reads no compressed MusicXML, from a file or from memory; the bytes are still taken for MusicXML. */
    char *bytes = read_file(compressed, &size);
    CHECK(bytes != NULL, "cannot read %s", compressed);
    if (bytes) {
        CHECK(input_pipeline_import_memory(bytes, size, INPUT_FORMAT_AUTO, &notes) == INPUT_PIPELINE_ERR_IMPORT_FAILED,
              "compressed MusicXML");
        free(bytes);
    }
    bytes = read_file(musicxml, &size);
    CHECK(bytes != NULL, "cannot read %s", musicxml);
    if (!bytes) {
        return;
    }
    CHECK(input_pipeline_import_memory(NULL, size, INPUT_FORMAT_AUTO, &notes) == INPUT_PIPELINE_ERR_IMPORT_FAILED,
          "NULL bytes");
    CHECK(input_pipeline_import_memory(bytes, 0, INPUT_FORMAT_AUTO, &notes) == INPUT_PIPELINE_ERR_IMPORT_FAILED,
          "empty bytes");
    CHECK(input_pipeline_import_memory(bytes, size, INPUT_FORMAT_AUTO, NULL) == INPUT_PIPELINE_ERR_IMPORT_FAILED,
          "NULL output");
    free(bytes);

    const char *text = "this is not music";
    CHECK(input_pipeline_import_memory(text, strlen(text), INPUT_FORMAT_AUTO, &notes) ==
              INPUT_PIPELINE_ERR_UNKNOWN_FORMAT,
          "text of no format");
    CHECK(input_pipeline_import_memory(text, strlen(text), INPUT_FORMAT_MUSICXML, &notes) ==
              INPUT_PIPELINE_ERR_IMPORT_FAILED,
          "text named as MusicXML");
    CHECK(input_pipeline_import_memory(text, strlen(text), INPUT_FORMAT_MIDI, &notes) ==
              INPUT_PIPELINE_ERR_IMPORT_FAILED,
          "text named as MIDI");

    bytes = read_file(URSATZ_MUSICXML_DIR "/malformed.musicxml", &size);
    CHECK(bytes != NULL, "cannot read malformed.musicxml");
    if (bytes) {
        CHECK(input_pipeline_import_memory(bytes, size, INPUT_FORMAT_AUTO, &notes) ==
                  INPUT_PIPELINE_ERR_IMPORT_FAILED,
              "malformed MusicXML");
        free(bytes);
    }
}

/* ---- material export ---- */

static char *fixture(const char *name) {
    char path[512];
    size_t size = 0;
    snprintf(path, sizeof(path), "%s/export/%s", URSATZ_ANALYZER_FIXTURE_DIR, name);
    char *bytes = read_file(path, &size);
    CHECK(bytes != NULL, "cannot read %s", path);
    return bytes;
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

static void check_snapshot(const char *kind, const char *text) {
    char path[512];
    snprintf(path, sizeof(path), "%s/material-%s.export.txt", URSATZ_SNAPSHOT_DIR, kind);
    char *actual = without_analyzer_line(text);
    CHECK(actual != NULL, "%s: out of memory", kind);
    if (!actual) {
        return;
    }
    const char *update = update_snapshots_env();
    if (update && strcmp(update, "1") == 0) {
        CHECK(test_write_file(path, actual), "%s: cannot write %s", kind, path);
        printf("%s: snapshot updated (%zu bytes)\n", kind, strlen(text));
    } else {
        char *expected = test_read_file(path);
        CHECK(expected != NULL, "%s: missing %s (run with UPDATE_SNAPSHOTS=1 to create it)", kind, path);
        if (expected) {
            CHECK(strcmp(expected, actual) == 0, "%s: differs from %s", kind, path);
            free(expected);
        }
    }
    free(actual);
}

static int count_of(const char *text, const char *needle) {
    int count = 0;
    for (const char *at = text; (at = strstr(at, needle)) != NULL; at += strlen(needle)) {
        count++;
    }
    return count;
}

static void test_motif(void) {
    char *motif = fixture("motif.musicxml");
    char *variant_1 = fixture("motif-variant-1.musicxml");
    char *variant_2 = fixture("motif-variant-2.musicxml");
    if (!motif || !variant_1 || !variant_2) {
        free(motif);
        free(variant_1);
        free(variant_2);
        return;
    }
    AnalyzerTextExportFact material_facts[] = {{"role", "head"}, {"meter", "4/4"}};
    AnalyzerTextExportFact motif_facts[] = {{"notes", "C E G E | F A G"}, {"base harmony", "I - IV\nV"}};
    AnalyzerTextExportFact variant_facts[] = {{"kind", "rhythm"}};
    AnalyzerTextExportSegment segments[] = {{"a", 0, 3}, {"a'", 4, 6}};
    AnalyzerTextExportSection sections[] = {
        {"motif", motif, motif_facts, 2, segments, 2},
        {"variant 1: invert", variant_1, variant_facts, 1, NULL, 0},
        {"variant 2: rhythm", variant_2, variant_facts, 1, NULL, 0},
    };
    AnalyzerTextExportMaterial material = {"motif", "motif-7", "Rising arch", 3, material_facts, 2, sections, 3};

    char *text = NULL;
    AnalyzerStatus status = analyzer_text_export_material(&material, &text);
    CHECK(status == ANALYZER_OK && text, "motif export: status %d", (int)status);
    if (text) {
        CHECK(strncmp(text, "# Ursatz export v2: motif\nname: Rising arch\nid: motif-7\nversion: 3\nrole: head\nmeter: 4/4\n",
                      strlen("# Ursatz export v2: motif\nname: Rising arch\nid: motif-7\nversion: 3\nrole: head\nmeter: 4/4\n")) == 0,
              "motif header:\n%s", text);
        CHECK(strstr(text, "\n## motif\nnotes: C E G E | F A G\nbase harmony: I - IV V\n") != NULL, "motif section head");
        CHECK(strstr(text, "\n## variant 1: invert\nkind: rhythm\n") != NULL, "variant 1 head");
        CHECK(strstr(text, "\n## variant 2: rhythm\n") != NULL, "variant 2 head");
        CHECK(strstr(text, "C4 @1 q[a], E4 @2 q, G4 @3 q, E4 @4 q[/a]") != NULL, "segment a marks:\n%s", text);
        CHECK(strstr(text, "F4 @1 q[a'], A4 @2 q, G4 @3 h[/a']") != NULL, "segment a' marks:\n%s", text);
        CHECK(count_of(text, "[a]") == 1 && count_of(text, "[/a]") == 1, "segment a is marked once");
        CHECK(strstr(text, "key: C major") != NULL, "section key line");
        check_snapshot("motif", text);
        free(text);
    }

    /* A segment outside the notes is refused. */
    AnalyzerTextExportSegment outside[] = {{"x", 5, 7}};
    sections[0].segments = outside;
    sections[0].segment_count = 1;
    text = NULL;
    CHECK(analyzer_text_export_material(&material, &text) == ANALYZER_ERR_ARGUMENT && !text, "segment past the notes");
    AnalyzerTextExportSegment backwards[] = {{"x", 3, 1}};
    sections[0].segments = backwards;
    CHECK(analyzer_text_export_material(&material, &text) == ANALYZER_ERR_ARGUMENT && !text, "segment first > last");
    sections[0].segments = segments;
    sections[0].segment_count = 2;

    /* A motif with 6 variants stays under 8 KB. */
    AnalyzerTextExportSection many[7] = {sections[0]};
    for (int i = 1; i < 7; i++) {
        many[i] = sections[1 + (i % 2)];
    }
    material.sections = many;
    material.section_count = 7;
    status = analyzer_text_export_material(&material, &text);
    CHECK(status == ANALYZER_OK && text, "motif with 6 variants: status %d", (int)status);
    if (text) {
        CHECK(strlen(text) < MATERIAL_BYTES_MOTIF_WITH_VARIANTS, "motif with 6 variants is %zu bytes", strlen(text));
        free(text);
    }
    free(motif);
    free(variant_1);
    free(variant_2);
}

static void test_theme(void) {
    char *theme = fixture("theme.musicxml");
    char *slot_a = fixture("motif.musicxml");
    char *slot_b = fixture("motif-variant-1.musicxml");
    if (!theme || !slot_a || !slot_b) {
        free(theme);
        free(slot_a);
        free(slot_b);
        return;
    }
    AnalyzerTextExportFact material_facts[] = {{"form", "A A'"}, {"chords", "bar 1: i"}, {"slots", "2"}};
    AnalyzerTextExportSection sections[] = {
        {"theme", theme, NULL, 0, NULL, 0},
        {"slot 1: Rising arch v1", slot_a, NULL, 0, NULL, 0},
        {"slot 2: Falling arch v2", slot_b, NULL, 0, NULL, 0},
    };
    AnalyzerTextExportMaterial material = {"theme", "theme-3", "Opening theme", 2, material_facts, 3, sections, 3};
    char *text = NULL;
    AnalyzerStatus status = analyzer_text_export_material(&material, &text);
    CHECK(status == ANALYZER_OK && text, "theme export: status %d", (int)status);
    if (text) {
        CHECK(strncmp(text, "# Ursatz export v2: theme\n", 26) == 0, "theme header");
        CHECK(strstr(text, "\n## theme\n") && strstr(text, "\n## slot 1: Rising arch v1\n") &&
                  strstr(text, "\n## slot 2: Falling arch v2\n"),
              "theme sections");
        CHECK(strstr(text, "form: A A'\nchords: bar 1: i\nslots: 2\n") != NULL, "theme facts");
        check_snapshot("theme", text);
        free(text);
    }

    /* A section with no MusicXML prints its label, facts and "not composed", and the others still export. */
    AnalyzerTextExportFact uncomposed_facts[] = {{"why", "no plan"}};
    sections[0].musicxml = NULL;
    sections[0].facts = uncomposed_facts;
    sections[0].fact_count = 1;
    material.section_count = 1;
    text = NULL;
    status = analyzer_text_export_material(&material, &text);
    CHECK(status == ANALYZER_OK && text, "uncomposed theme: status %d", (int)status);
    if (text) {
        CHECK(strstr(text, "\n## theme\nwhy: no plan\nnot composed\n") != NULL, "not composed:\n%s", text);
        CHECK(strstr(text, "\nm1") == NULL, "no measures for a section with no MusicXML");
        free(text);
    }
    free(theme);
    free(slot_a);
    free(slot_b);
}

static void test_piece(void) {
    char *loop = fixture("piece-loop.musicxml");
    char *ending = fixture("piece-ending.musicxml");
    if (!loop || !ending) {
        free(loop);
        free(ending);
        return;
    }
    AnalyzerTextExportFact material_facts[] = {{"key", "Eb minor"}, {"A loop", "8 beats, fill none, cadence HC"}};
    AnalyzerTextExportSection sections[] = {
        {"loop", loop, NULL, 0, NULL, 0},
        {"ending", ending, NULL, 0, NULL, 0},
    };
    AnalyzerTextExportMaterial material = {"piece", "piece-2", "Night piece", 4, material_facts, 2, sections, 2};
    char *text = NULL;
    AnalyzerStatus status = analyzer_text_export_material(&material, &text);
    CHECK(status == ANALYZER_OK && text, "piece export: status %d", (int)status);
    if (text) {
        CHECK(strncmp(text, "# Ursatz export v2: piece\n", 26) == 0, "piece header");
        CHECK(strstr(text, "\n## loop\n") && strstr(text, "\n## ending\n"), "piece sections");
        CHECK(strstr(text, "A loop: 8 beats, fill none, cadence HC\n") != NULL, "piece facts");
        check_snapshot("piece", text);
        free(text);
    }
    free(loop);
    free(ending);
}

static void test_material_refusals(void) {
    char *text = NULL;
    AnalyzerTextExportMaterial no_kind = {NULL, "id", "name", 1, NULL, 0, NULL, 0};
    CHECK(analyzer_text_export_material(NULL, &text) == ANALYZER_ERR_ARGUMENT, "NULL material");
    CHECK(analyzer_text_export_material(&no_kind, &text) == ANALYZER_ERR_ARGUMENT && !text, "NULL kind");
    AnalyzerTextExportMaterial empty = {"motif", "id", "name", 1, NULL, 0, NULL, 0};
    CHECK(analyzer_text_export_material(&empty, NULL) == ANALYZER_ERR_ARGUMENT, "NULL text");
    CHECK(analyzer_text_export_material(&empty, &text) == ANALYZER_OK && text &&
              strncmp(text, "# Ursatz export v2: motif\nname: name\nid: id\nversion: 1\n", 53) == 0,
          "material with no sections");
    free(text);
    text = NULL;

    AnalyzerTextExportSection no_label = {NULL, NULL, NULL, 0, NULL, 0};
    AnalyzerTextExportMaterial unlabelled = {"motif", "id", "name", 1, NULL, 0, &no_label, 1};
    CHECK(analyzer_text_export_material(&unlabelled, &text) == ANALYZER_ERR_ARGUMENT && !text, "NULL section label");

    AnalyzerTextExportSection garbage = {"bad", "this is not music", NULL, 0, NULL, 0};
    AnalyzerTextExportMaterial bad = {"motif", "id", "name", 1, NULL, 0, &garbage, 1};
    CHECK(analyzer_text_export_material(&bad, &text) == ANALYZER_ERR_LIBRARY && !text, "section that is not music");
}

int main(void) {
    test_import_memory();
    test_motif();
    test_theme();
    test_piece();
    test_material_refusals();
    if (failures > 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    return 0;
}
