#include "export/musicxml_export.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/voice_reference.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "io/midi_importer.h"
#include "persistence/persistence.h"
#include "persistence/persistence_note_events.h"

/*
 * Regression for the multi-track voice-assignment bug: Bach_Prelude_C_
 * Major.mid (tests/fixtures/arpeggiation-investigation/) is a type-1,
 * single-channel file with RH and LH on two separate note-bearing
 * tracks. Before per-track voice assignment, assign_voice_slots only
 * ran when the whole file was single-track, so every note here kept
 * optional_voice == NULL, musicxml_export_write grouped RH and LH into
 * one implicit voice, and their overlapping notes tripped
 * MUSICXML_EXPORT_ERR_OVERLAPPING_NOTES. It must now import with RH/LH
 * assigned to distinct voices and export cleanly.
 */

#define FIXTURE_PATH "../fixtures/arpeggiation-investigation/Bach_Prelude_C_Major.mid"

static unsigned char *read_fixture(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    assert(f != NULL);
    assert(fseek(f, 0, SEEK_END) == 0);
    long size = ftell(f);
    assert(size > 0);
    assert(fseek(f, 0, SEEK_SET) == 0);

    unsigned char *buf = malloc((size_t)size);
    assert(buf != NULL);
    assert(fread(buf, 1, (size_t)size, f) == (size_t)size);
    fclose(f);

    *out_size = (size_t)size;
    return buf;
}

int main(void) {
    size_t size = 0;
    unsigned char *data = read_fixture(FIXTURE_PATH, &size);

    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    MidiImportResult import_result = midi_importer_import(data, size, &events, &pitch_values, &count);
    free(data);
    assert(import_result == MIDI_IMPORT_OK);
    assert(count > 0);

    /* Every note must have been assigned a voice, and at least two
       distinct voice ids must appear -- RH and LH landed in separate
       voices rather than all sharing the single implicit one. */
    const char *first_voice_id = NULL;
    bool saw_distinct_voice = false;
    for (size_t i = 0; i < count; i++) {
        const VoiceReference *voice = note_event_optional_voice(events[i]);
        assert(voice != NULL);
        const char *voice_id = entity_id_value(voice_reference_target_id(voice));
        if (first_voice_id == NULL) {
            first_voice_id = voice_id;
        } else if (strcmp(first_voice_id, voice_id) != 0) {
            saw_distinct_voice = true;
        }
    }
    assert(saw_distinct_voice);

    const char *path = "test_musicxml_export_bach_prelude.sqlite";
    remove(path);
    PersistenceHandle *handle = NULL;
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);
    assert(persistence_store_note_events(handle, events, count) == PERSISTENCE_NOTE_EVENTS_OK);

    char *xml = NULL;
    MusicXmlExportResult export_result = musicxml_export_write(handle, &xml);
    assert(export_result == MUSICXML_EXPORT_OK);
    assert(xml != NULL);
    assert(strstr(xml, "<score-partwise version=\"3.1\">") != NULL);

    free(xml);
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitch_values[i]);
    }
    free(events);
    free(pitch_values);
    persistence_close(handle);
    remove(path);

    printf("musicxml_export_bach_prelude: ok (%zu notes)\n", count);
    return 0;
}
