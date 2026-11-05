#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/imported_notes.h>
#include <ursatz_analyzer/input_pipeline.h>

/*
 * The material export (text_export.h) and the in-memory import it reads through (input_pipeline.h).
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

int main(void) {
    test_import_memory();
    if (failures > 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    return 0;
}
