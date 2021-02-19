#include <ursatz_analyzer/midi_pipeline.h>

#include <stdio.h>
#include <stdlib.h>

#include "io/midi_importer.h"
#include "io/midi_key_signature.h"
#include "io/smf_reader.h"

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
    MidiMeterImport meter = {NULL, NULL, NULL, 0};
    MidiImportResult import_result =
        midi_importer_import_with_meter(buffer, (size_t)size, &events, &pitch_values, &count, &meter);

    MidiKeySignature key_signature = {false, 0, false};
    MidiKeySignatureEvents key_signature_events = {NULL, 0};
    unsigned ticks_per_quarter = 0;
    if (import_result == MIDI_IMPORT_OK) {
        SmfHeader header;
        SmfTrack *tracks = NULL;
        size_t track_count = 0;
        if (smf_reader_parse(buffer, (size_t)size, &header, &tracks, &track_count) == SMF_READER_OK) {
            ticks_per_quarter = header.division;
            free(tracks);
        }

        /* A malformed key signature meta-event doesn't fail an
           otherwise-valid import -- it just means no key signature is
           reported, same as a file that never had one. */
        if (midi_key_signature_scan(buffer, (size_t)size, &key_signature) != MIDI_KEY_SIGNATURE_OK) {
            key_signature = (MidiKeySignature){false, 0, false};
        }
        if (midi_key_signature_scan_all(buffer, (size_t)size, &key_signature_events) !=
            MIDI_KEY_SIGNATURE_SCAN_ALL_OK) {
            key_signature_events = (MidiKeySignatureEvents){NULL, 0};
        }
    }
    free(buffer);

    if (import_result != MIDI_IMPORT_OK) {
        midi_key_signature_events_destroy(&key_signature_events);
        fprintf(stderr, "Error: failed to import '%s': %s\n", path, midi_import_error_message(import_result));
        return MIDI_PIPELINE_ERR_IMPORT_FAILED;
    }

    out->events = events;
    out->pitch_values = pitch_values;
    out->count = count;
    out->key_signature = key_signature;
    out->key_signature_events = key_signature_events;
    out->ticks_per_quarter = ticks_per_quarter;
    out->meter = meter;
    for (size_t i = 0; i < meter.conflict_count; i++) {
        fprintf(stderr, "Warning: '%s' declares different time signatures at tick %llu; the last one is used\n",
                path, (unsigned long long)meter.conflict_ticks[i]);
    }
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
    midi_meter_import_destroy(&notes->meter);
    midi_key_signature_events_destroy(&notes->key_signature_events);
    notes->meter = (MidiMeterImport){NULL, NULL, NULL, 0};
    notes->events = NULL;
    notes->pitch_values = NULL;
    notes->count = 0;
    notes->ticks_per_quarter = 0;
}
