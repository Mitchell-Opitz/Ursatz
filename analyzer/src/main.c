#include <stdio.h>

#include "pipeline/midi_pipeline.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <midi-file-path>\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    ImportedNotes notes;
    if (midi_pipeline_import_file(path, &notes) != MIDI_PIPELINE_OK) {
        return 1;
    }

    printf("Received file: %s\n", path);
    printf("Imported %zu note events\n", notes.count);

    imported_notes_destroy(&notes);
    return 0;
}
