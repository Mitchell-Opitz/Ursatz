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
    for (size_t i = 0; i < notes->source.tempo_marking_count; i++) {
        free(notes->source.tempo_markings[i].text);
    }
    free(notes->source.tempo_markings);
    for (size_t i = 0; i < notes->source.dynamics_count; i++) {
        free(notes->source.dynamics[i].marking);
    }
    free(notes->source.dynamics);
    notes->source = (ImportedSource){.format = INPUT_FORMAT_MIDI};
    notes->meter = (MidiMeterImport){NULL, NULL, NULL, 0};
    notes->events = NULL;
    notes->pitch_values = NULL;
    notes->streams = NULL;
    notes->count = 0;
    notes->ticks_per_quarter = 0;
}
