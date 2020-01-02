#include "persistence/persistence_note_events.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "io/midi_importer.h"
#include "persistence/persistence.h"
#include "pitch/pitch_numeric_value.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

/*
 * Round-trip test: import NoteEvents from a real (hand-built, in the
 * same style as tests/io/test_midi_importer.c) SMF fixture, store them,
 * reload them, and assert full value equality per event -- including
 * exact rational anchor/duration fields, so no float/double conversion
 * ever touches this path.
 */

static void append_bytes(unsigned char *buf, size_t *pos, const void *bytes, size_t len) {
    memcpy(buf + *pos, bytes, len);
    *pos += len;
}

static void append_u32(unsigned char *buf, size_t *pos, uint32_t v) {
    unsigned char b[4] = {(unsigned char)(v >> 24), (unsigned char)(v >> 16), (unsigned char)(v >> 8),
                           (unsigned char)v};
    append_bytes(buf, pos, b, 4);
}

static void append_u16(unsigned char *buf, size_t *pos, uint16_t v) {
    unsigned char b[2] = {(unsigned char)(v >> 8), (unsigned char)v};
    append_bytes(buf, pos, b, 2);
}

static size_t build_smf_type0(unsigned char *buf, uint16_t division, const unsigned char *track_body,
                               size_t track_body_size) {
    size_t pos = 0;
    append_bytes(buf, &pos, "MThd", 4);
    append_u32(buf, &pos, 6);
    append_u16(buf, &pos, 0);
    append_u16(buf, &pos, 1);
    append_u16(buf, &pos, division);
    append_bytes(buf, &pos, "MTrk", 4);
    append_u32(buf, &pos, (uint32_t)track_body_size);
    append_bytes(buf, &pos, track_body, track_body_size);
    return pos;
}

static void test_round_trip(void) {
    /* PPQN 96: note60 for one beat, then note62 for one beat, back to back. */
    unsigned char track[] = {
        0x00, 0x90, 0x3C, 0x64, /* delta 0,  note-on  60 */
        0x60, 0x80, 0x3C, 0x40, /* delta 96, note-off 60 */
        0x00, 0x90, 0x3E, 0x64, /* delta 0,  note-on  62 */
        0x60, 0x80, 0x3E, 0x40, /* delta 96, note-off 62 */
        0x00, 0xFF, 0x2F, 0x00  /* end of track */
    };

    unsigned char buf[128];
    size_t size = build_smf_type0(buf, 96, track, sizeof(track));

    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    MidiImportResult import_result = midi_importer_import(buf, size, &events, &pitch_values, &count);
    assert(import_result == MIDI_IMPORT_OK);
    assert(count == 2);

    const char *path = "test_persistence_note_events_round_trip.sqlite";
    remove(path);

    PersistenceHandle *handle = NULL;
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);

    assert(persistence_store_note_events(handle, events, count) == PERSISTENCE_NOTE_EVENTS_OK);

    NoteEvent **reloaded = NULL;
    size_t reloaded_count = 0;
    assert(persistence_load_note_events(handle, &reloaded, &reloaded_count) == PERSISTENCE_NOTE_EVENTS_OK);
    assert(reloaded_count == count);

    for (size_t i = 0; i < count; i++) {
        assert(strcmp(entity_id_value(note_event_id(events[i])), entity_id_value(note_event_id(reloaded[i]))) == 0);
        assert(note_event_equals_by_value(events[i], reloaded[i]));

        const TemporalAnchor *original_anchor = note_event_temporal_anchor(events[i]);
        const TemporalAnchor *reloaded_anchor = note_event_temporal_anchor(reloaded[i]);
        const MusicalTime *original_position = temporal_anchor_position(original_anchor);
        const MusicalTime *reloaded_position = temporal_anchor_position(reloaded_anchor);
        assert(musical_time_numerator(original_position) == musical_time_numerator(reloaded_position));
        assert(musical_time_denominator(original_position) == musical_time_denominator(reloaded_position));

        const Duration *original_duration = note_event_duration(events[i]);
        const Duration *reloaded_duration = note_event_duration(reloaded[i]);
        assert(duration_numerator(original_duration) == duration_numerator(reloaded_duration));
        assert(duration_denominator(original_duration) == duration_denominator(reloaded_duration));

        const ProvenanceReference *original_provenance = note_event_provenance(events[i]);
        const ProvenanceReference *reloaded_provenance = note_event_provenance(reloaded[i]);
        assert(provenance_reference_equals(original_provenance, reloaded_provenance));

        assert(note_event_optional_voice(reloaded[i]) == NULL);
    }

    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        note_event_destroy(reloaded[i]);
        pitch_numeric_value_destroy(pitch_values[i]);
    }
    free(events);
    free(reloaded);
    free(pitch_values);

    persistence_close(handle);
    remove(path);
}

int main(void) {
    test_round_trip();
    printf("persistence note_events: all tests passed\n");
    return 0;
}
