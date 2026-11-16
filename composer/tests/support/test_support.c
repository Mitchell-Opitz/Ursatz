#include "support/test_support.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char *test_read_file(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "cannot open %s\n", path);
    }
    assert(file);
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    assert(length >= 0);
    char *text = malloc((size_t)length + 1);
    assert(text);
    size_t read = fread(text, 1, (size_t)length, file);
    assert(read == (size_t)length);
    text[length] = '\0';
    fclose(file);
    if (size) {
        *size = read;
    }
    return text;
}

bool test_exists(const char *path) {
    return access(path, F_OK) == 0;
}

void test_write_file(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert(file);
    fputs(text, file);
    fclose(file);
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
