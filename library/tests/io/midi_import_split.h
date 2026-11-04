#ifndef TEST_MIDI_IMPORT_SPLIT_H
#define TEST_MIDI_IMPORT_SPLIT_H

#include <stddef.h>
#include <string.h>

#include "io/midi_importer.h"

/*
 * Test helper over midi_importer_import_complete: imports once, hands the caller the notes plus the meter
 * and/or tempo it asked for (NULL to skip a part), and destroys the rest. On failure nothing is written.
 */
static inline MidiImportResult test_midi_import_parts(const unsigned char *data, size_t size, NoteEvent ***out_events,
                                                      PitchNumericValue ***out_pitch_values, size_t *out_count,
                                                      MidiMeterImport *out_meter, MidiTempoImport *out_tempo) {
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
    if (out_meter != NULL) {
        *out_meter = import.meter;
        memset(&import.meter, 0, sizeof(import.meter));
    }
    if (out_tempo != NULL) {
        *out_tempo = import.tempo;
        memset(&import.tempo, 0, sizeof(import.tempo));
    }
    midi_import_destroy(&import);
    return MIDI_IMPORT_OK;
}

#endif
