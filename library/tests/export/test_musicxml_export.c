#include "export/musicxml_export.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "io/midi_importer.h"
#include "persistence/persistence.h"
#include "persistence/persistence_note_events.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_numeric_value.h"
#include "pitch/pitch_spelling.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

/*
 * MusicXmlExport tests: fixtures are built either via MIDIImporter (for
 * cases needing real PitchSpelling, the same fixture style as
 * tests/io/test_midi_importer.c and tests/persistence/
 * test_persistence_note_events.c) or hand-constructed NoteEvents (for
 * the missing-spelling and overlapping-notes failure cases, which
 * MIDIImporter output can never produce on its own).
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

static PersistenceHandle *open_fresh(const char *path) {
    remove(path);
    PersistenceHandle *handle = NULL;
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);
    return handle;
}

static void destroy_events(NoteEvent **events, size_t count) {
    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
    }
    free(events);
}

/* PPQN 96: middle C (60) for a quarter, then D4 (62) for a quarter. */
static void test_pitch_and_duration_and_cosmetic_flag(void) {
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
    assert(midi_importer_import(buf, size, &events, &pitch_values, &count) == MIDI_IMPORT_OK);
    assert(count == 2);

    const char *path = "test_musicxml_export_pitch_duration.sqlite";
    PersistenceHandle *handle = open_fresh(path);
    assert(persistence_store_note_events(handle, events, count) == PERSISTENCE_NOTE_EVENTS_OK);

    char *xml = NULL;
    assert(musicxml_export_write(handle, &xml) == MUSICXML_EXPORT_OK);
    assert(xml != NULL);

    assert(strstr(xml, "<score-partwise version=\"3.1\">") != NULL);
    assert(strstr(xml, "miscellaneous-field name=\"ursatz:synthetic-meter\">true<") != NULL);
    assert(strstr(xml, "<beats>4</beats>") != NULL);
    assert(strstr(xml, "<divisions>768</divisions>") != NULL);
    assert(strstr(xml, "<step>C</step>") != NULL);
    assert(strstr(xml, "<step>D</step>") != NULL);
    assert(strstr(xml, "<octave>4</octave>") != NULL);
    /* Each note is a quarter note: 768 ticks/quarter (3072/4). */
    assert(strstr(xml, "<duration>768</duration>") != NULL);
    /* No <tie> anywhere: neither note crosses the synthetic barline. */
    assert(strstr(xml, "<tie") == NULL);
    /* MIDIImporter assigns sequential ids "midi-note-<n>"; both pass
       through xml_id_from_entity unchanged since they're already valid
       XML ID tokens. */
    assert(strstr(xml, "<note xml:id=\"midi-note-0\">") != NULL);
    assert(strstr(xml, "<note xml:id=\"midi-note-1\">") != NULL);

    free(xml);
    for (size_t i = 0; i < count; i++) {
        pitch_numeric_value_destroy(pitch_values[i]);
    }
    free(pitch_values);
    destroy_events(events, count);
    persistence_close(handle);
    remove(path);
}

static void test_synthetic_measure_tie_split(void) {
    /* PPQN 96 (quarter=96 ticks, whole=384 ticks): a rest of 3 quarters
       (ticks 0-288) followed by a half note (192 ticks, 288-480) that
       straddles the whole-note barline at tick 384 -- must split into
       two tied fragments across two <measure> elements. Delta times
       <128 fit in one byte; 288 needs a multi-byte VLQ: 288 = 0x120 ->
       VLQ bytes 0x82 0x20. */
    unsigned char track[] = {
        0x82, 0x20, 0x90, 0x3C, 0x64, /* delta 288, note-on  60 */
        0x81, 0x40, 0x80, 0x3C, 0x40, /* delta 192, note-off 60 */
        0x00, 0xFF, 0x2F, 0x00        /* end of track */
    };
    unsigned char buf[128];
    size_t size = build_smf_type0(buf, 96, track, sizeof(track));

    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    MidiImportResult r = midi_importer_import(buf, size, &events, &pitch_values, &count);
    assert(r == MIDI_IMPORT_OK);
    assert(count == 1);

    const char *path = "test_musicxml_export_tie_split.sqlite";
    PersistenceHandle *handle = open_fresh(path);
    assert(persistence_store_note_events(handle, events, count) == PERSISTENCE_NOTE_EVENTS_OK);

    char *xml = NULL;
    assert(musicxml_export_write(handle, &xml) == MUSICXML_EXPORT_OK);
    assert(xml != NULL);

    assert(strstr(xml, "<measure number=\"1\">") != NULL);
    assert(strstr(xml, "<measure number=\"2\">") != NULL);
    assert(strstr(xml, "<rest/>") != NULL);
    assert(strstr(xml, "<tie type=\"start\"/>") != NULL);
    assert(strstr(xml, "<tie type=\"stop\"/>") != NULL);
    /* The single NoteEvent (id "midi-note-0") is split into two tied
       fragments -- each must carry a distinct id tracing back to the
       source note, not a duplicated xml:id. */
    assert(strstr(xml, "<note xml:id=\"midi-note-0_a\">") != NULL);
    assert(strstr(xml, "<note xml:id=\"midi-note-0_b\">") != NULL);
    assert(strstr(xml, "<note xml:id=\"midi-note-0\">") == NULL);
    /* The leading rest (a synthetic gap-filler, not backed by a
       NoteEvent) must carry no xml:id at all. */
    assert(strstr(xml, "<note>\n        <rest/>") != NULL);

    free(xml);
    for (size_t i = 0; i < count; i++) {
        pitch_numeric_value_destroy(pitch_values[i]);
    }
    free(pitch_values);
    destroy_events(events, count);
    persistence_close(handle);
    remove(path);
}

static NoteEvent *make_note(const char *id_value, long start_num, unsigned long start_den, long dur_num,
                             unsigned long dur_den, bool with_spelling) {
    EntityID *id = NULL;
    entity_id_create(id_value, &id);
    MusicalTime *position = NULL;
    musical_time_create(start_num, start_den, &position);
    TemporalAnchor *anchor = NULL;
    temporal_anchor_create(position, &anchor);
    Duration *duration = NULL;
    duration_create(dur_num, dur_den, &duration);
    PitchIdentity *pitch = NULL;
    unsigned char token[1] = {60};
    pitch_identity_create(token, sizeof(token), &pitch);
    ProvenanceReference *provenance = NULL;
    provenance_reference_create_absent(&provenance);

    PitchSpelling *spelling = NULL;
    if (with_spelling) {
        pitch_spelling_create(PITCH_LETTER_C, 0, 4, &spelling);
    }

    NoteEvent *event = NULL;
    assert(note_event_create(id, anchor, duration, pitch, spelling, NULL, provenance, &event) == NOTE_EVENT_OK);
    return event;
}

static void test_missing_spelling_fails_fast(void) {
    NoteEvent *event = make_note("no-spelling", 0, 1, 1, 4, false);

    const char *path = "test_musicxml_export_missing_spelling.sqlite";
    PersistenceHandle *handle = open_fresh(path);
    NoteEvent *events[1] = {event};
    assert(persistence_store_note_events(handle, events, 1) == PERSISTENCE_NOTE_EVENTS_OK);

    char *xml = NULL;
    assert(musicxml_export_write(handle, &xml) == MUSICXML_EXPORT_ERR_MISSING_SPELLING);
    assert(xml == NULL);

    note_event_destroy(event);
    persistence_close(handle);
    remove(path);
}

static void test_overlapping_notes_fail(void) {
    /* A: [0, 1/2). B: starts at 1/4, inside A -- overlap. */
    NoteEvent *a = make_note("note-a", 0, 1, 1, 2, true);
    NoteEvent *b = make_note("note-b", 1, 4, 1, 4, true);

    const char *path = "test_musicxml_export_overlap.sqlite";
    PersistenceHandle *handle = open_fresh(path);
    NoteEvent *events[2] = {a, b};
    assert(persistence_store_note_events(handle, events, 2) == PERSISTENCE_NOTE_EVENTS_OK);

    char *xml = NULL;
    assert(musicxml_export_write(handle, &xml) == MUSICXML_EXPORT_ERR_OVERLAPPING_NOTES);
    assert(xml == NULL);

    note_event_destroy(a);
    note_event_destroy(b);
    persistence_close(handle);
    remove(path);
}

static void test_id_sanitization_for_invalid_entity_ids(void) {
    /* "3rd-note" starts with a digit (invalid XML ID start) and contains
       a space (invalid XML ID character) -- both must be sanitized:
       prefix 'n', replace the space with '_'. */
    NoteEvent *event = make_note("3rd note", 0, 1, 1, 4, true);

    const char *path = "test_musicxml_export_sanitized_id.sqlite";
    PersistenceHandle *handle = open_fresh(path);
    NoteEvent *events[1] = {event};
    assert(persistence_store_note_events(handle, events, 1) == PERSISTENCE_NOTE_EVENTS_OK);

    char *xml = NULL;
    assert(musicxml_export_write(handle, &xml) == MUSICXML_EXPORT_OK);
    assert(xml != NULL);

    assert(strstr(xml, "<note xml:id=\"n3rd_note\">") != NULL);

    free(xml);
    note_event_destroy(event);
    persistence_close(handle);
    remove(path);
}

static void test_empty_piece_is_valid(void) {
    const char *path = "test_musicxml_export_empty.sqlite";
    PersistenceHandle *handle = open_fresh(path);

    char *xml = NULL;
    assert(musicxml_export_write(handle, &xml) == MUSICXML_EXPORT_OK);
    assert(xml != NULL);
    assert(strstr(xml, "<measure number=\"1\">") != NULL);
    assert(strstr(xml, "<note>") == NULL);
    assert(strstr(xml, "miscellaneous-field name=\"ursatz:synthetic-meter\">true<") != NULL);

    free(xml);
    persistence_close(handle);
    remove(path);
}

static void test_null_args_rejected(void) {
    char *xml = NULL;
    assert(musicxml_export_write(NULL, &xml) == MUSICXML_EXPORT_ERR_NULL_ARG);

    const char *path = "test_musicxml_export_null_out.sqlite";
    PersistenceHandle *handle = open_fresh(path);
    assert(musicxml_export_write(handle, NULL) == MUSICXML_EXPORT_ERR_NULL_ARG);
    persistence_close(handle);
    remove(path);
}

int main(void) {
    test_pitch_and_duration_and_cosmetic_flag();
    test_synthetic_measure_tie_split();
    test_missing_spelling_fails_fast();
    test_id_sanitization_for_invalid_entity_ids();
    test_overlapping_notes_fail();
    test_empty_piece_is_valid();
    test_null_args_rejected();
    printf("musicxml_export: all tests passed\n");
    return 0;
}
