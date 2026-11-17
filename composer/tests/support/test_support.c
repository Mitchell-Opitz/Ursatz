#include "support/test_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ursatz_test.h"

void test_write_text(const char *path, const char *text) {
    if (!test_write_file(path, text, strlen(text))) {
        fprintf(stderr, "cannot write %s\n", path);
        abort();
    }
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
