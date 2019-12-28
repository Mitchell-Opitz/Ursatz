#include "pipeline/midi_pipeline.h"

#include <stdio.h>
#include <stdlib.h>

#include "io/midi_importer.h"
#include "io/midi_key_signature.h"

static const char *midi_import_error_message(MidiImportResult result) {
    switch (result) {
        case MIDI_IMPORT_ERR_NULL_BUFFER:
            return "internal error: null buffer passed to importer";
        case MIDI_IMPORT_ERR_TRUNCATED:
            return "MIDI file is truncated";
        case MIDI_IMPORT_ERR_BAD_HEADER:
            return "MIDI file has a malformed header";
        case MIDI_IMPORT_ERR_UNSUPPORTED_FORMAT:
            return "MIDI file is not SMF type 0 or type 1";
        case MIDI_IMPORT_ERR_UNSUPPORTED_DIVISION:
            return "MIDI file uses an unsupported (SMPTE) time division";
        case MIDI_IMPORT_ERR_BAD_TRACK:
            return "MIDI file contains a malformed track (bad event bytes, unmatched note-on, or unrecognized "
                   "status byte)";
        case MIDI_IMPORT_ERR_ALLOC:
            return "out of memory while importing MIDI file";
        default:
            return "unknown MIDI import error";
    }
}

MidiPipelineResult midi_pipeline_import_file(const char *path, ImportedNotes *out) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Error: cannot read file '%s'\n", path);
        return MIDI_PIPELINE_ERR_CANNOT_READ_FILE;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        fprintf(stderr, "Error: cannot read file '%s'\n", path);
        return MIDI_PIPELINE_ERR_CANNOT_READ_FILE;
    }
    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        fprintf(stderr, "Error: cannot read file '%s'\n", path);
        return MIDI_PIPELINE_ERR_CANNOT_READ_FILE;
    }
    rewind(file);

    unsigned char *buffer = malloc((size_t)size > 0 ? (size_t)size : 1);
    if (!buffer) {
        fclose(file);
        fprintf(stderr, "Error: out of memory reading '%s'\n", path);
        return MIDI_PIPELINE_ERR_CANNOT_READ_FILE;
    }

    size_t read_count = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    if (read_count != (size_t)size) {
        free(buffer);
        fprintf(stderr, "Error: cannot read file '%s'\n", path);
        return MIDI_PIPELINE_ERR_CANNOT_READ_FILE;
    }

    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    MidiImportResult import_result = midi_importer_import(buffer, (size_t)size, &events, &pitch_values, &count);

    MidiKeySignature key_signature = {false, 0, false};
    if (import_result == MIDI_IMPORT_OK) {
        /* A malformed key signature meta-event doesn't fail an
           otherwise-valid import -- it just means no key signature is
           reported, same as a file that never had one. */
        if (midi_key_signature_scan(buffer, (size_t)size, &key_signature) != MIDI_KEY_SIGNATURE_OK) {
            key_signature = (MidiKeySignature){false, 0, false};
        }
    }
    free(buffer);

    if (import_result != MIDI_IMPORT_OK) {
        fprintf(stderr, "Error: failed to import '%s': %s\n", path, midi_import_error_message(import_result));
        return MIDI_PIPELINE_ERR_IMPORT_FAILED;
    }

    out->events = events;
    out->pitch_values = pitch_values;
    out->count = count;
    out->key_signature = key_signature;
    return MIDI_PIPELINE_OK;
}

void imported_notes_destroy(ImportedNotes *notes) {
    if (!notes) {
        return;
    }
    for (size_t i = 0; i < notes->count; i++) {
        note_event_destroy(notes->events[i]);
        pitch_numeric_value_destroy(notes->pitch_values[i]);
    }
    free(notes->events);
    free(notes->pitch_values);
    notes->events = NULL;
    notes->pitch_values = NULL;
    notes->count = 0;
}
