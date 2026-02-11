#include "midi_render_test_util.h"

/* Voice tracks and note events: byte-exact. Whole-note fractions at division 480 (1920 ticks per whole note). */

static void assert_track_bytes(const SmfTrack *track, const unsigned char *expected, size_t expected_size) {
    assert(track->size == expected_size);
    assert(memcmp(track->data, expected, expected_size) == 0);
}

static void test_two_voices(void) {
    TestNotes notes;
    test_notes_init(&notes);
    /* voice A: C4 (60) at 0 for a quarter, E4 (64) at a quarter for a quarter; voice B: G3 (55) at a quarter for a half */
    test_notes_add(&notes, 0, 1, 1, 4, 60, "voice-a");
    test_notes_add(&notes, 1, 4, 1, 4, 64, "voice-a");
    test_notes_add(&notes, 1, 4, 1, 2, 55, "voice-b");

    unsigned char *data = NULL;
    size_t size = 0;
    assert(midi_renderer_render(notes.events, notes.pitches, notes.count, NULL, NULL, NULL, 0, 480, &data, &size) ==
           MIDI_RENDER_OK);
    TestSmf smf;
    test_smf_parse(data, size, &smf);
    assert(smf.track_count == 3);

    static const unsigned char voice_a[] = {0x00, 0x90, 0x3C, 0x40, 0x83, 0x60, 0x80, 0x3C, 0x00, 0x00, 0x90, 0x40, 0x40,
                                            0x83, 0x60, 0x80, 0x40, 0x00, 0x00, 0xFF, 0x2F, 0x00};
    static const unsigned char voice_b[] = {0x83, 0x60, 0x91, 0x37, 0x40, 0x87, 0x40, 0x81, 0x37, 0x00, 0x00, 0xFF, 0x2F, 0x00};
    assert_track_bytes(&smf.tracks[1], voice_a, sizeof(voice_a));
    assert_track_bytes(&smf.tracks[2], voice_b, sizeof(voice_b));

    free(smf.tracks);
    free(data);
    test_notes_free(&notes);
}

static void test_no_voice_shares_one_track(void) {
    TestNotes notes;
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 4, 60, NULL);
    test_notes_add(&notes, 0, 1, 1, 4, 64, NULL);
    unsigned char *data = NULL;
    size_t size = 0;
    assert(midi_renderer_render(notes.events, notes.pitches, notes.count, NULL, NULL, NULL, 0, 480, &data, &size) ==
           MIDI_RENDER_OK);
    TestSmf smf;
    test_smf_parse(data, size, &smf);
    assert(smf.track_count == 2);
    int channel;
    assert(test_track_note_ons(&smf.tracks[1], &channel) == 2 && channel == 0);
    free(smf.tracks);
    free(data);
    test_notes_free(&notes);
}

static void test_back_to_back_same_pitch_releases_first(void) {
    TestNotes notes;
    test_notes_init(&notes);
    test_notes_add(&notes, 0, 1, 1, 4, 60, NULL);
    test_notes_add(&notes, 1, 4, 1, 4, 60, NULL);
    unsigned char *data = NULL;
    size_t size = 0;
    assert(midi_renderer_render(notes.events, notes.pitches, notes.count, NULL, NULL, NULL, 0, 480, &data, &size) ==
           MIDI_RENDER_OK);
    TestSmf smf;
    test_smf_parse(data, size, &smf);
    static const unsigned char expected[] = {0x00, 0x90, 0x3C, 0x40, 0x83, 0x60, 0x80, 0x3C, 0x00, 0x00, 0x90, 0x3C, 0x40,
                                             0x83, 0x60, 0x80, 0x3C, 0x00, 0x00, 0xFF, 0x2F, 0x00};
    assert_track_bytes(&smf.tracks[1], expected, sizeof(expected));
    free(smf.tracks);
    free(data);
    test_notes_free(&notes);
}

static void test_track_order_follows_first_sounding(void) {
    TestNotes notes;
    test_notes_init(&notes);
    /* voice-late is listed first but sounds later: it gets track 2 / channel 1 */
    test_notes_add(&notes, 1, 2, 1, 4, 72, "voice-late");
    test_notes_add(&notes, 0, 1, 1, 4, 60, "voice-early");
    unsigned char *data = NULL;
    size_t size = 0;
    assert(midi_renderer_render(notes.events, notes.pitches, notes.count, NULL, NULL, NULL, 0, 480, &data, &size) ==
           MIDI_RENDER_OK);
    TestSmf smf;
    test_smf_parse(data, size, &smf);
    int channel;
    assert(test_track_note_ons(&smf.tracks[1], &channel) == 1 && channel == 0 && smf.tracks[1].data[2 + 1] == 0x3C);
    assert(test_track_note_ons(&smf.tracks[2], &channel) == 1 && channel == 1 && smf.tracks[2].data[3 + 1] == 0x48);
    free(smf.tracks);
    free(data);
    test_notes_free(&notes);
}

int main(void) {
    test_two_voices();
    test_no_voice_shares_one_track();
    test_back_to_back_same_pitch_releases_first();
    test_track_order_follows_first_sounding();
    printf("test_midi_renderer_notes: all tests passed\n");
    return 0;
}
