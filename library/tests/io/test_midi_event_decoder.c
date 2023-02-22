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

/* --- time signature meta-events (0xFF 0x58) --- */

static MidiEventDecoderResult collect(const unsigned char *a, size_t a_size, const unsigned char *b, size_t b_size,
                                      MidiTimeSignatureEvents *out) {
    SmfTrack tracks[2] = {{a, a_size}, {b, b_size}};
    return midi_event_decoder_collect_time_signatures(tracks, b == NULL ? 1 : 2, out);
}

static void test_collects_time_signature(void) {
    unsigned char track[] = {
        0x00, 0xFF, 0x58, 0x04, 0x06, 0x03, 0x18, 0x08, /* tick 0: 6/8 */
        0x60, 0xFF, 0x58, 0x04, 0x03, 0x02, 0x18, 0x08  /* tick 96: 3/4 */
    };
    MidiTimeSignatureEvents events = {0};
    assert(collect(track, sizeof(track), NULL, 0, &events) == MIDI_EVENT_DECODER_OK);
    assert(events.count == 2);
    assert(events.conflict_count == 0);
    assert(events.events[0].tick == 0);
    assert(time_signature_top_number(events.events[0].time_signature) == 6);
    assert(time_signature_bottom_number(events.events[0].time_signature) == 8);
    assert(events.events[1].tick == 96);
    assert(time_signature_top_number(events.events[1].time_signature) == 3);
    assert(time_signature_bottom_number(events.events[1].time_signature) == 4);
    midi_time_signature_events_destroy(&events);
}

static void test_collects_from_every_track_in_time_order(void) {
    unsigned char a[] = {0x60, 0xFF, 0x58, 0x04, 0x03, 0x02, 0x18, 0x08};
    unsigned char b[] = {0x00, 0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08};
    MidiTimeSignatureEvents events = {0};
    assert(collect(a, sizeof(a), b, sizeof(b), &events) == MIDI_EVENT_DECODER_OK);
    assert(events.count == 2);
    assert(events.events[0].tick == 0 && events.events[0].track_index == 1);
    assert(events.events[1].tick == 96 && events.events[1].track_index == 0);
    midi_time_signature_events_destroy(&events);
}

static void test_merges_exact_duplicates(void) {
    unsigned char a[] = {0x00, 0xFF, 0x58, 0x04, 0x03, 0x02, 0x18, 0x08};
    unsigned char b[] = {0x00, 0xFF, 0x58, 0x04, 0x03, 0x02, 0x18, 0x08};
    MidiTimeSignatureEvents events = {0};
    assert(collect(a, sizeof(a), b, sizeof(b), &events) == MIDI_EVENT_DECODER_OK);
    assert(events.count == 1);
    assert(events.conflict_count == 0);
    midi_time_signature_events_destroy(&events);
}

static void test_conflict_last_track_wins_and_is_reported(void) {
    unsigned char a[] = {0x00, 0xFF, 0x58, 0x04, 0x03, 0x02, 0x18, 0x08};
    unsigned char b[] = {0x00, 0xFF, 0x58, 0x04, 0x06, 0x03, 0x18, 0x08};
    MidiTimeSignatureEvents events = {0};
    assert(collect(a, sizeof(a), b, sizeof(b), &events) == MIDI_EVENT_DECODER_OK);
    assert(events.count == 1);
    assert(time_signature_top_number(events.events[0].time_signature) == 6);
    assert(events.events[0].track_index == 1);
    assert(events.conflict_count == 1);
    assert(events.conflict_ticks[0] == 0);
    midi_time_signature_events_destroy(&events);
}

static void test_skips_malformed_time_signatures(void) {
    unsigned char track[] = {
        0x00, 0xFF, 0x58, 0x03, 0x04, 0x02, 0x18,       /* length 3: skipped */
        0x00, 0xFF, 0x58, 0x04, 0x00, 0x02, 0x18, 0x08, /* nn 0: skipped */
        0x00, 0xFF, 0x58, 0x04, 0x65, 0x02, 0x18, 0x08, /* nn 101: skipped */
        0x00, 0xFF, 0x58, 0x04, 0x04, 0x05, 0x18, 0x08, /* bottom 32: skipped */
        0x00, 0xFF, 0x58, 0x04, 0x02, 0x01, 0x18, 0x08  /* 2/2: kept */
    };
    MidiTimeSignatureEvents events = {0};
    assert(collect(track, sizeof(track), NULL, 0, &events) == MIDI_EVENT_DECODER_OK);
    assert(events.count == 1);
    assert(time_signature_top_number(events.events[0].time_signature) == 2);
    assert(time_signature_bottom_number(events.events[0].time_signature) == 2);
    midi_time_signature_events_destroy(&events);
}

static void test_no_time_signatures_is_ok_and_empty(void) {
    unsigned char track[] = {0x00, 0x90, 0x3C, 0x64};
    MidiTimeSignatureEvents events = {0};
    assert(collect(track, sizeof(track), NULL, 0, &events) == MIDI_EVENT_DECODER_OK);
    assert(events.count == 0 && events.events == NULL);
    midi_time_signature_events_destroy(&events);
}

static void test_note_decode_ignores_time_signatures(void) {
    unsigned char track[] = {0x00, 0xFF, 0x58, 0x04, 0x03, 0x02, 0x18, 0x08, 0x00, 0x90, 0x3C, 0x64};
    MidiNoteEvent *events = NULL;
    size_t count = 0;
    assert(midi_event_decoder_decode(track, sizeof(track), &events, &count) == MIDI_EVENT_DECODER_OK);
    assert(count == 1);
    free(events);
}

static void test_collect_rejects_truncated_track_and_null(void) {
    unsigned char track[] = {0x00, 0xFF, 0x58, 0x04, 0x03};
    MidiTimeSignatureEvents events = {0};
    assert(collect(track, sizeof(track), NULL, 0, &events) == MIDI_EVENT_DECODER_ERR_TRUNCATED);
    assert(midi_event_decoder_collect_time_signatures(NULL, 0, NULL) == MIDI_EVENT_DECODER_ERR_NULL_TRACK);
}

int main(void) {
    test_decodes_note_on_and_off();
    test_note_on_velocity_zero_is_note_off();
    test_running_status_reuses_previous_status_byte();
    test_skips_non_note_events_without_error();
    test_rejects_truncated_track();
    test_rejects_null_track();
    test_collects_time_signature();
    test_collects_from_every_track_in_time_order();
    test_merges_exact_duplicates();
    test_conflict_last_track_wins_and_is_reported();
    test_skips_malformed_time_signatures();
    test_no_time_signatures_is_ok_and_empty();
    test_note_decode_ignores_time_signatures();
    test_collect_rejects_truncated_track_and_null();
    printf("test_midi_event_decoder: ok\n");
    return 0;
}
