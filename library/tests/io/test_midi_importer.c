#include "io/midi_importer.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_numeric_value.h"
#include "pitch/pitch_spelling.h"
#include "pitch/tuning_system.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

/*
 * MIDIImporter end-to-end tests: hand-constructed SMF type 0 fixtures
 * exercise a sequence of notes and a simultaneous chord, checking the
 * produced NoteEvent's pitch, timing, duration, and Provenance. A
 * separate test confirms clean, defined failure on a truncated file.
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

static PitchIdentity *make_expected_pitch(uint8_t midi_note) {
    EntityID *tuning_id = NULL;
    entity_id_create("midi-note-number", &tuning_id);
    TuningSystem *tuning_system = NULL;
    tuning_system_create(tuning_id, "MIDI note number", &tuning_system);

    PitchNumericValue *numeric = NULL;
    pitch_numeric_value_create(tuning_system, midi_note, 1, &numeric);

    PitchIdentity *identity = NULL;
    pitch_numeric_value_to_identity(numeric, &identity);

    pitch_numeric_value_destroy(numeric);
    tuning_system_destroy(tuning_system);
    return identity;
}

static void assert_position(const NoteEvent *note, long numerator, unsigned long denominator) {
    const TemporalAnchor *anchor = note_event_temporal_anchor(note);
    const MusicalTime *position = temporal_anchor_position(anchor);
    MusicalTime *expected = NULL;
    musical_time_create(numerator, denominator, &expected);
    assert(musical_time_equals(position, expected));
    musical_time_destroy(expected);
}

static void assert_duration(const NoteEvent *note, long numerator, unsigned long denominator) {
    const Duration *duration = note_event_duration(note);
    Duration *expected = NULL;
    duration_create(numerator, denominator, &expected);
    assert(duration_equals(duration, expected));
    duration_destroy(expected);
}

static void assert_pitch(const NoteEvent *note, uint8_t midi_note) {
    PitchIdentity *expected = make_expected_pitch(midi_note);
    assert(pitch_identity_equals(note_event_pitch(note), expected));
    pitch_identity_destroy(expected);
}

static void assert_spelling(const NoteEvent *note, PitchLetter letter, int accidental, int octave) {
    const PitchSpelling *spelling = note_event_optional_pitch_spelling(note);
    assert(spelling != NULL);
    assert(pitch_spelling_letter(spelling) == letter);
    assert(pitch_spelling_accidental(spelling) == accidental);
    assert(pitch_spelling_octave(spelling) == octave);
}

static void test_spells_chromatic_scale_sharps_only(void) {
    /* PPQN 4: MIDI notes 60..72 (C4 through C5), one tick each, back to
       back -- covers every pitch class plus the octave boundary. */
    unsigned char track[13 * 8 + 4];
    size_t pos = 0;
    for (int note = 60; note <= 72; note++) {
        unsigned char on[4] = {0x00, 0x90, (unsigned char)note, 0x64};
        unsigned char off[4] = {0x01, 0x80, (unsigned char)note, 0x40};
        memcpy(track + pos, on, 4);
        pos += 4;
        memcpy(track + pos, off, 4);
        pos += 4;
    }
    unsigned char eot[4] = {0x00, 0xFF, 0x2F, 0x00};
    memcpy(track + pos, eot, 4);
    pos += 4;
    assert(pos == sizeof(track));

    unsigned char buf[256];
    size_t size = build_smf_type0(buf, 4, track, pos);

    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    MidiImportResult r = midi_importer_import(buf, size, &events, &pitch_values, &count);
    assert(r == MIDI_IMPORT_OK);
    assert(count == 13);

    assert_spelling(events[0], PITCH_LETTER_C, 0, 4);  /* 60: C4 */
    assert_spelling(events[1], PITCH_LETTER_C, 1, 4);  /* 61: C#4 */
    assert_spelling(events[2], PITCH_LETTER_D, 0, 4);  /* 62: D4 */
    assert_spelling(events[3], PITCH_LETTER_D, 1, 4);  /* 63: D#4 */
    assert_spelling(events[4], PITCH_LETTER_E, 0, 4);  /* 64: E4 */
    assert_spelling(events[5], PITCH_LETTER_F, 0, 4);  /* 65: F4 */
    assert_spelling(events[6], PITCH_LETTER_F, 1, 4);  /* 66: F#4 */
    assert_spelling(events[7], PITCH_LETTER_G, 0, 4);  /* 67: G4 */
    assert_spelling(events[8], PITCH_LETTER_G, 1, 4);  /* 68: G#4 */
    assert_spelling(events[9], PITCH_LETTER_A, 0, 4);  /* 69: A4 */
    assert_spelling(events[10], PITCH_LETTER_A, 1, 4); /* 70: A#4 */
    assert_spelling(events[11], PITCH_LETTER_B, 0, 4); /* 71: B4 */
    assert_spelling(events[12], PITCH_LETTER_C, 0, 5); /* 72: C5 -- octave boundary */

    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitch_values[i]);
    }
    free(events);
    free(pitch_values);
}

static void assert_provenance(const NoteEvent *note) {
    const ProvenanceReference *provenance = note_event_provenance(note);
    assert(provenance_reference_state(provenance) == PROVENANCE_REFERENCE_PRESENT);
    const EntityID *target = provenance_reference_target_id(provenance);
    assert(strcmp(entity_id_value(target), "midi-importer-v1") == 0);
}

static void test_sequence_of_notes(void) {
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
    MidiImportResult r = midi_importer_import(buf, size, &events, &pitch_values, &count);

    assert(r == MIDI_IMPORT_OK);
    assert(count == 2);

    /* denominator = 4 * PPQN = 384 (whole-note fraction, tempo-independent). */
    assert_position(events[0], 0, 384);
    assert_duration(events[0], 96, 384);
    assert_pitch(events[0], 0x3C);
    assert_provenance(events[0]);
    assert(pitch_numeric_value_position_numerator(pitch_values[0]) == 0x3C);
    assert(pitch_numeric_value_position_denominator(pitch_values[0]) == 1);

    assert_position(events[1], 96, 384);
    assert_duration(events[1], 96, 384);
    assert_pitch(events[1], 0x3E);
    assert_provenance(events[1]);
    assert(pitch_numeric_value_position_numerator(pitch_values[1]) == 0x3E);
    assert(pitch_numeric_value_position_denominator(pitch_values[1]) == 1);

    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitch_values[i]);
    }
    free(events);
    free(pitch_values);
}

static void test_simultaneous_chord(void) {
    /* PPQN 96: three notes on together at tick 0, off together at tick 96. */
    unsigned char track[] = {
        0x00, 0x90, 0x3C, 0x64, /* delta 0, note-on 60 */
        0x00, 0x90, 0x40, 0x64, /* delta 0, note-on 64 */
        0x00, 0x90, 0x43, 0x64, /* delta 0, note-on 67 */
        0x60, 0x80, 0x3C, 0x40, /* delta 96, note-off 60 */
        0x00, 0x80, 0x40, 0x40, /* delta 0, note-off 64 */
        0x00, 0x80, 0x43, 0x40, /* delta 0, note-off 67 */
        0x00, 0xFF, 0x2F, 0x00
    };

    unsigned char buf[128];
    size_t size = build_smf_type0(buf, 96, track, sizeof(track));

    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    MidiImportResult r = midi_importer_import(buf, size, &events, &pitch_values, &count);

    assert(r == MIDI_IMPORT_OK);
    assert(count == 3);

    int seen_60 = 0, seen_64 = 0, seen_67 = 0;
    for (size_t i = 0; i < count; i++) {
        assert_position(events[i], 0, 384);
        assert_duration(events[i], 96, 384);
        assert_provenance(events[i]);

        PitchIdentity *p60 = make_expected_pitch(0x3C);
        PitchIdentity *p64 = make_expected_pitch(0x40);
        PitchIdentity *p67 = make_expected_pitch(0x43);
        if (pitch_identity_equals(note_event_pitch(events[i]), p60)) {
            seen_60 = 1;
            assert(pitch_numeric_value_position_numerator(pitch_values[i]) == 0x3C);
        } else if (pitch_identity_equals(note_event_pitch(events[i]), p64)) {
            seen_64 = 1;
            assert(pitch_numeric_value_position_numerator(pitch_values[i]) == 0x40);
        } else if (pitch_identity_equals(note_event_pitch(events[i]), p67)) {
            seen_67 = 1;
            assert(pitch_numeric_value_position_numerator(pitch_values[i]) == 0x43);
        }
        pitch_identity_destroy(p60);
        pitch_identity_destroy(p64);
        pitch_identity_destroy(p67);
    }
    assert(seen_60 && seen_64 && seen_67);

    for (size_t i = 0; i < count; i++) {
        note_event_destroy(events[i]);
        pitch_numeric_value_destroy(pitch_values[i]);
    }
    free(events);
    free(pitch_values);
}

static void test_rejects_truncated_file(void) {
    unsigned char buf[8];
    memcpy(buf, "MThd", 4);
    /* claims length 6 but buffer ends here -- truncated. */
    unsigned char len[4] = {0, 0, 0, 6};
    memcpy(buf + 4, len, 4);

    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    MidiImportResult r = midi_importer_import(buf, sizeof(buf), &events, &pitch_values, &count);

    assert(r == MIDI_IMPORT_ERR_TRUNCATED);
    assert(events == NULL);
    assert(pitch_values == NULL);
}

static void test_rejects_null_buffer(void) {
    NoteEvent **events = NULL;
    PitchNumericValue **pitch_values = NULL;
    size_t count = 0;
    MidiImportResult r = midi_importer_import(NULL, 0, &events, &pitch_values, &count);
    assert(r == MIDI_IMPORT_ERR_NULL_BUFFER);
}

int main(void) {
    test_sequence_of_notes();
    test_simultaneous_chord();
    test_spells_chromatic_scale_sharps_only();
    test_rejects_truncated_file();
    test_rejects_null_buffer();
    printf("test_midi_importer: ok\n");
    return 0;
}
