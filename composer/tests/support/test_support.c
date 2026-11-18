#include "support/test_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "plan/plan_json_internal.h"

#include "ursatz_test.h"

void test_write_text(const char *path, const char *text) {
    if (!test_write_file(path, text, strlen(text))) {
        fprintf(stderr, "cannot write %s\n", path);
        abort();
    }
}

/* Removes the identity members of every note under node. */
static void strip_note_identity(cJSON *node) {
    cJSON *child;
    if (cJSON_IsObject(node)) {
        cJSON *notes = cJSON_GetObjectItemCaseSensitive(node, "notes");
        cJSON *note;
        cJSON_ArrayForEach(note, notes) {
            cJSON_DeleteItemFromObjectCaseSensitive(note, "id");
            cJSON_DeleteItemFromObjectCaseSensitive(note, "source");
            cJSON_DeleteItemFromObjectCaseSensitive(note, "operation");
        }
    }
    cJSON_ArrayForEach(child, node) {
        strip_note_identity(child);
    }
}

char *test_plan_text_as_format_three(const char *text) {
    cJSON *document = cJSON_Parse(text);
    if (!document) {
        return NULL;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(document, "next_note_id");
    cJSON_ReplaceItemInObjectCaseSensitive(document, "format_version", cJSON_CreateNumber(3));
    strip_note_identity(document);
    char *printed = cJSON_Print(document);
    cJSON_Delete(document);
    char *out = printed ? json_indent_with_spaces(printed) : NULL; /* the project's layout: 2 spaces and "key": value */
    cJSON_free(printed);
    return out;
}

MidiImportResult test_midi_import_notes(const unsigned char *data, size_t size, NoteEvent ***out_events,
                                        PitchNumericValue ***out_pitch_values, size_t *out_count) {
    MidiImport import;
    MidiImportResult result = midi_importer_import_complete(data, size, &import);
    if (result != MIDI_IMPORT_OK) {
        return result;
    }
    *out_events = import.events;
    *out_pitch_values = import.pitch_values;
    *out_count = import.count;
    import.events = NULL;
    import.pitch_values = NULL;
    import.count = 0;
    midi_import_destroy(&import);
    return MIDI_IMPORT_OK;
}
