#include <ursatz_analyzer/imported_notes.h>

#include <stdlib.h>

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
    free(notes->streams);
    midi_meter_import_destroy(&notes->meter);
    midi_key_signature_events_destroy(&notes->key_signature_events);
    for (size_t i = 0; i < notes->source.part_count; i++) {
        free(notes->source.parts[i].id);
        free(notes->source.parts[i].name);
    }
    free(notes->source.parts);
    free(notes->source.part_index);
    free(notes->source.staff);
    notes->source = (ImportedSource){INPUT_FORMAT_MIDI, false, false, false, NULL, 0, NULL, NULL, false, 0.0, 0, 0, 0};
    notes->meter = (MidiMeterImport){NULL, NULL, NULL, 0};
    notes->events = NULL;
    notes->pitch_values = NULL;
    notes->streams = NULL;
    notes->count = 0;
    notes->ticks_per_quarter = 0;
}
