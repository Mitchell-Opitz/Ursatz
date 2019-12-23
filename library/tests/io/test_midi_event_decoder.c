#include "io/midi_event_decoder.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * midi_event_decoder tests: note-on/note-off extraction (including
 * velocity-0-as-note-off and running status), non-note events skipped
 * without error, and clean failure on truncated/malformed bytes.
 */

static void test_decodes_note_on_and_off(void) {
    unsigned char track[] = {
        0x00, 0x90, 0x3C, 0x64, /* delta 0, note-on ch0 note60 vel100 */
        0x60, 0x80, 0x3C, 0x40  /* delta 96, note-off ch0 note60 vel64 */
    };

    MidiNoteEvent *events = NULL;
    size_t count = 0;
    MidiEventDecoderResult r = midi_event_decoder_decode(track, sizeof(track), &events, &count);

    assert(r == MIDI_EVENT_DECODER_OK);
    assert(count == 2);
    assert(events[0].kind == MIDI_NOTE_ON);
    assert(events[0].tick == 0);
    assert(events[0].note == 0x3C);
    assert(events[0].velocity == 100);
    assert(events[1].kind == MIDI_NOTE_OFF);
    assert(events[1].tick == 96);
    assert(events[1].note == 0x3C);

    free(events);
}

static void test_note_on_velocity_zero_is_note_off(void) {
    unsigned char track[] = {
        0x00, 0x90, 0x40, 0x50, /* note-on note64 vel80 */
        0x10, 0x90, 0x40, 0x00  /* note-on note64 vel0 -> treated as note-off */
    };

    MidiNoteEvent *events = NULL;
    size_t count = 0;
    MidiEventDecoderResult r = midi_event_decoder_decode(track, sizeof(track), &events, &count);

    assert(r == MIDI_EVENT_DECODER_OK);
    assert(count == 2);
    assert(events[0].kind == MIDI_NOTE_ON);
    assert(events[1].kind == MIDI_NOTE_OFF);

    free(events);
}

static void test_running_status_reuses_previous_status_byte(void) {
    unsigned char track[] = {
        0x00, 0x90, 0x3C, 0x64, /* note-on note60 vel100, status byte 0x90 set */
        0x00, 0x40, 0x64,       /* running status: another note-on note64 vel100 */
        0x00, 0x3C, 0x00        /* running status: note-off (vel 0) note60 */
    };

    MidiNoteEvent *events = NULL;
    size_t count = 0;
    MidiEventDecoderResult r = midi_event_decoder_decode(track, sizeof(track), &events, &count);

    assert(r == MIDI_EVENT_DECODER_OK);
    assert(count == 3);
    assert(events[0].kind == MIDI_NOTE_ON && events[0].note == 0x3C);
    assert(events[1].kind == MIDI_NOTE_ON && events[1].note == 0x40);
    assert(events[2].kind == MIDI_NOTE_OFF && events[2].note == 0x3C);

    free(events);
}

static void test_skips_non_note_events_without_error(void) {
    unsigned char track[] = {
        0x00, 0xB0, 0x07, 0x7F,       /* CC: channel volume, 2 data bytes */
        0x00, 0xC0, 0x05,             /* program change, 1 data byte */
        0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20, /* tempo meta event, 3-byte payload */
        0x00, 0x90, 0x3C, 0x64,       /* the one note-on we care about */
        0x00, 0xFF, 0x2F, 0x00        /* end-of-track meta */
    };

    MidiNoteEvent *events = NULL;
    size_t count = 0;
    MidiEventDecoderResult r = midi_event_decoder_decode(track, sizeof(track), &events, &count);

    assert(r == MIDI_EVENT_DECODER_OK);
    assert(count == 1);
    assert(events[0].kind == MIDI_NOTE_ON);
    assert(events[0].note == 0x3C);

    free(events);
}

static void test_rejects_truncated_track(void) {
    unsigned char track[] = {0x00, 0x90, 0x3C}; /* missing velocity byte */

    MidiNoteEvent *events = NULL;
    size_t count = 0;
    MidiEventDecoderResult r = midi_event_decoder_decode(track, sizeof(track), &events, &count);

    assert(r == MIDI_EVENT_DECODER_ERR_TRUNCATED);
}

static void test_rejects_null_track(void) {
    MidiNoteEvent *events = NULL;
    size_t count = 0;
    MidiEventDecoderResult r = midi_event_decoder_decode(NULL, 0, &events, &count);
    assert(r == MIDI_EVENT_DECODER_ERR_NULL_TRACK);
}

int main(void) {
    test_decodes_note_on_and_off();
    test_note_on_velocity_zero_is_note_off();
    test_running_status_reuses_previous_status_byte();
    test_skips_non_note_events_without_error();
    test_rejects_truncated_track();
    test_rejects_null_track();
    printf("test_midi_event_decoder: ok\n");
    return 0;
}
