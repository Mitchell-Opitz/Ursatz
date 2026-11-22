#include "io/midi_key_signature.h"
#include "io/tests/io/midi_import_split.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ursatz_test.h"
#include "model/pitch/key_signature.h"
#include "model/time/musical_time.h"

/*
 * midi_key_signature tests: reading Key Signature meta-events (0xFF 0x59) through the complete MIDI import
 * (test_midi_import_key_signatures) in a sharp key, a flat key, a minor key, a file with no such event and a file with
 * a truncated meta-event payload, then every key change with its position, and the KeySignature each sf byte builds.
 */

static void test_finds_sharp_major_key_signature(void) {
    /* G major: 1 sharp, major. Preceded by an unrelated note-on/note-off
       pair to confirm non-meta events are skipped correctly. */
    unsigned char track[] = {
        0x00, 0x90, 0x3C, 0x64, /* note-on */
        0x60, 0x80, 0x3C, 0x40, /* note-off */
        0x00, 0xFF, 0x59, 0x02, 0x01, 0x00, /* key signature: sf=1, mi=0 */
        0x00, 0xFF, 0x2F, 0x00 /* end of track */
    };
    unsigned char file[128];
    size_t file_size = test_smf_type0(file, 96, track, sizeof(track));

    MidiKeySignatureEvents keys = {NULL, 0};
    assert(test_midi_import_key_signatures(file, file_size, &keys) == MIDI_IMPORT_OK);
    assert(keys.count == 1);
    assert(keys.events[0].sharps_flats == 1);
    assert(keys.events[0].mode_hint == MIDI_MODE_HINT_MAJOR);
    midi_key_signature_events_destroy(&keys);
}

static void test_finds_flat_key_signature(void) {
    /* Eb major: 3 flats, major. sf byte -3 as two's-complement 0xFD. */
    unsigned char track[] = {
        0x00, 0xFF, 0x59, 0x02, 0xFD, 0x00,
        0x00, 0xFF, 0x2F, 0x00
    };
    unsigned char file[64];
    size_t file_size = test_smf_type0(file, 96, track, sizeof(track));

    MidiKeySignatureEvents keys = {NULL, 0};
    assert(test_midi_import_key_signatures(file, file_size, &keys) == MIDI_IMPORT_OK);
    assert(keys.count == 1);
    assert(keys.events[0].sharps_flats == -3);
    assert(keys.events[0].mode_hint == MIDI_MODE_HINT_MAJOR);
    midi_key_signature_events_destroy(&keys);
}

static void test_finds_minor_key_signature(void) {
    /* E minor: 1 sharp, minor. */
    unsigned char track[] = {
        0x00, 0xFF, 0x59, 0x02, 0x01, 0x01,
        0x00, 0xFF, 0x2F, 0x00
    };
    unsigned char file[64];
    size_t file_size = test_smf_type0(file, 96, track, sizeof(track));

    MidiKeySignatureEvents keys = {NULL, 0};
    assert(test_midi_import_key_signatures(file, file_size, &keys) == MIDI_IMPORT_OK);
    assert(keys.count == 1);
    assert(keys.events[0].sharps_flats == 1);
    assert(keys.events[0].mode_hint == MIDI_MODE_HINT_MINOR);
    midi_key_signature_events_destroy(&keys);
}

static void test_no_key_signature_present(void) {
    unsigned char track[] = {
        0x00, 0x90, 0x3C, 0x64,
        0x60, 0x80, 0x3C, 0x40,
        0x00, 0xFF, 0x2F, 0x00
    };
    unsigned char file[64];
    size_t file_size = test_smf_type0(file, 96, track, sizeof(track));

    MidiKeySignatureEvents keys = {NULL, 0};
    assert(test_midi_import_key_signatures(file, file_size, &keys) == MIDI_IMPORT_OK);
    assert(keys.count == 0);
}

static void test_truncated_key_signature_meta_event_fails_cleanly(void) {
    /* Declares a 2-byte payload but only supplies 1 byte before the
       buffer ends. */
    unsigned char track[] = {0x00, 0xFF, 0x59, 0x02, 0x01};
    unsigned char file[64];
    size_t file_size = test_smf_type0(file, 96, track, sizeof(track));

    MidiKeySignatureEvents untouched = {(MidiKeySignatureEvent *)0x1, 99};
    assert(test_midi_import_key_signatures(file, file_size, &untouched) == MIDI_IMPORT_ERR_BAD_TRACK);
    assert(untouched.count == 99);
}

/* ---- midi_key_signature_build and the key signatures of a complete import ---- */

static char letter_char(PitchLetter letter) {
    switch (letter) {
    case PITCH_LETTER_A: return 'A';
    case PITCH_LETTER_B: return 'B';
    case PITCH_LETTER_C: return 'C';
    case PITCH_LETTER_D: return 'D';
    case PITCH_LETTER_E: return 'E';
    case PITCH_LETTER_F: return 'F';
    case PITCH_LETTER_G: return 'G';
    }
    return '?';
}

static void assert_signature(const KeySignature *key_signature, const char *letters, int accidental) {
    size_t count = strlen(letters);
    assert(key_signature_entry_count(key_signature) == count);
    for (size_t i = 0; i < count; i++) {
        assert(letter_char(key_signature_letter_at(key_signature, i)) == letters[i]);
        assert(key_signature_accidental_at(key_signature, i) == accidental);
    }
}

static void assert_position(const MidiKeySignatureEvent *event, long numerator, unsigned long denominator) {
    MusicalTime expected;
    assert(musical_time_make(numerator, (long long)denominator, &expected) == MUSICAL_TIME_OK);
    assert(musical_time_same(event->position, expected));
}

static void test_build_all_fifteen_values(void) {
    static const char *const SHARPS[8] = {"", "F", "FC", "FCG", "FCGD", "FCGDA", "FCGDAE", "FCGDAEB"};
    static const char *const FLATS[8] = {"", "B", "BE", "BEA", "BEAD", "BEADG", "BEADGC", "BEADGCF"};
    for (int sf = -7; sf <= 7; sf++) {
        KeySignature *key_signature = NULL;
        assert(midi_key_signature_build(sf, &key_signature) == MIDI_KEY_SIGNATURE_BUILD_OK);
        assert(key_signature != NULL);
        if (sf < 0) {
            assert_signature(key_signature, FLATS[-sf], -1);
        } else {
            assert_signature(key_signature, SHARPS[sf], 1);
        }
        key_signature_destroy(key_signature);
    }
}

static void test_build_rejects_out_of_range(void) {
    KeySignature *untouched = (KeySignature *)0x1;
    assert(midi_key_signature_build(8, &untouched) == MIDI_KEY_SIGNATURE_BUILD_ERR_SF_OUT_OF_RANGE);
    assert(untouched == (KeySignature *)0x1);
    assert(midi_key_signature_build(-8, &untouched) == MIDI_KEY_SIGNATURE_BUILD_ERR_SF_OUT_OF_RANGE);
    assert(untouched == (KeySignature *)0x1);
    assert(midi_key_signature_build(127, &untouched) == MIDI_KEY_SIGNATURE_BUILD_ERR_SF_OUT_OF_RANGE);
    assert(midi_key_signature_build(0, NULL) == MIDI_KEY_SIGNATURE_BUILD_ERR_NULL_OUT);
}

static void test_import_single_key_signature(void) {
    unsigned char track[] = {
        0x00, 0xFF, 0x59, 0x02, 0x01, 0x00, /* G major */
        0x00, 0xFF, 0x2F, 0x00,
    };
    const unsigned char *tracks[1] = {track};
    size_t sizes[1] = {sizeof(track)};
    unsigned char file[128];
    size_t file_size = test_smf(file, 0, 96, tracks, sizes, 1);

    MidiKeySignatureEvents events = {NULL, 0};
    assert(test_midi_import_key_signatures(file, file_size, &events) == MIDI_IMPORT_OK);
    assert(events.count == 1);
    assert(events.events[0].tick == 0);
    assert(events.events[0].track_index == 0);
    assert(events.events[0].sharps_flats == 1);
    assert(events.events[0].mode_byte == 0);
    assert(events.events[0].mode_hint == MIDI_MODE_HINT_MAJOR);
    assert_position(&events.events[0], 0, 1);
    assert_signature(events.events[0].key_signature, "F", 1);
    midi_key_signature_events_destroy(&events);
    assert(events.events == NULL && events.count == 0);
}

static void test_import_key_changes_with_positions(void) {
    /* Division 96, 4/4: a measure is 384 ticks. C major, then G major at measure 9 (tick 3072),
       then Eb major at measure 17 (tick 6144). 3072 is the variable-length quantity 0x98 0x00. */
    unsigned char track[] = {
        0x00, 0xFF, 0x59, 0x02, 0x00, 0x00, /* C major at tick 0 */
        0x98, 0x00, 0xFF, 0x59, 0x02, 0x01, 0x00, /* G major at tick 3072 */
        0x98, 0x00, 0xFF, 0x59, 0x02, 0xFD, 0x00, /* Eb major at tick 6144 */
        0x00, 0xFF, 0x2F, 0x00,
    };
    const unsigned char *tracks[1] = {track};
    size_t sizes[1] = {sizeof(track)};
    unsigned char file[128];
    size_t file_size = test_smf(file, 0, 96, tracks, sizes, 1);

    MidiKeySignatureEvents events = {NULL, 0};
    assert(test_midi_import_key_signatures(file, file_size, &events) == MIDI_IMPORT_OK);
    assert(events.count == 3);

    assert(events.events[0].tick == 0);
    assert_position(&events.events[0], 0, 1);
    assert_signature(events.events[0].key_signature, "", 1);

    assert(events.events[1].tick == 3072);
    assert_position(&events.events[1], 8, 1); /* eight whole notes: 3072 / (4 * 96) */
    assert_signature(events.events[1].key_signature, "F", 1);

    assert(events.events[2].tick == 6144);
    assert_position(&events.events[2], 16, 1);
    assert_signature(events.events[2].key_signature, "BEA", -1);
    midi_key_signature_events_destroy(&events);
}

static void test_import_fractional_position(void) {
    /* A key change one quarter note in (96 ticks at division 96) is 1/4 of a whole note. */
    unsigned char track[] = {
        0x60, 0xFF, 0x59, 0x02, 0x02, 0x00,
        0x00, 0xFF, 0x2F, 0x00,
    };
    const unsigned char *tracks[1] = {track};
    size_t sizes[1] = {sizeof(track)};
    unsigned char file[128];
    size_t file_size = test_smf(file, 0, 96, tracks, sizes, 1);

    MidiKeySignatureEvents events = {NULL, 0};
    assert(test_midi_import_key_signatures(file, file_size, &events) == MIDI_IMPORT_OK);
    assert(events.count == 1);
    assert(events.events[0].tick == 96);
    assert_position(&events.events[0], 1, 4);
    midi_key_signature_events_destroy(&events);
}

static void test_import_merges_tracks_in_time_order(void) {
    /* Track 0: G major at tick 0, D major at tick 192. Track 1: G major at tick 0 (an exact duplicate
       of track 0's), F major at tick 96. Expected: G@0 (track 0), F@96 (track 1), D@192 (track 0). */
    unsigned char track0[] = {
        0x00, 0xFF, 0x59, 0x02, 0x01, 0x00,
        0x81, 0x40, 0xFF, 0x59, 0x02, 0x02, 0x00, /* delta 192 */
        0x00, 0xFF, 0x2F, 0x00,
    };
    unsigned char track1[] = {
        0x00, 0xFF, 0x59, 0x02, 0x01, 0x00,
        0x60, 0xFF, 0x59, 0x02, 0xFF, 0x00, /* delta 96, sf = -1 */
        0x00, 0xFF, 0x2F, 0x00,
    };
    const unsigned char *tracks[2] = {track0, track1};
    size_t sizes[2] = {sizeof(track0), sizeof(track1)};
    unsigned char file[256];
    size_t file_size = test_smf(file, 1, 96, tracks, sizes, 2);

    MidiKeySignatureEvents events = {NULL, 0};
    assert(test_midi_import_key_signatures(file, file_size, &events) == MIDI_IMPORT_OK);
    assert(events.count == 3);
    assert(events.events[0].tick == 0 && events.events[0].track_index == 0 && events.events[0].sharps_flats == 1);
    assert(events.events[1].tick == 96 && events.events[1].track_index == 1 && events.events[1].sharps_flats == -1);
    assert(events.events[2].tick == 192 && events.events[2].track_index == 0 && events.events[2].sharps_flats == 2);
    midi_key_signature_events_destroy(&events);
}

static void test_import_same_tick_orders_by_track(void) {
    /* Different events at one tick are all kept, in track order, even when the later track is listed
       with the smaller key signature. */
    unsigned char track0[] = {0x00, 0xFF, 0x59, 0x02, 0x03, 0x00, 0x00, 0xFF, 0x2F, 0x00};
    unsigned char track1[] = {0x00, 0xFF, 0x59, 0x02, 0x01, 0x00, 0x00, 0xFF, 0x2F, 0x00};
    const unsigned char *tracks[2] = {track0, track1};
    size_t sizes[2] = {sizeof(track0), sizeof(track1)};
    unsigned char file[128];
    size_t file_size = test_smf(file, 1, 96, tracks, sizes, 2);

    MidiKeySignatureEvents events = {NULL, 0};
    assert(test_midi_import_key_signatures(file, file_size, &events) == MIDI_IMPORT_OK);
    assert(events.count == 2);
    assert(events.events[0].track_index == 0 && events.events[0].sharps_flats == 3);
    assert(events.events[1].track_index == 1 && events.events[1].sharps_flats == 1);
    midi_key_signature_events_destroy(&events);
}

static void test_import_duplicates_and_conflicts_on_one_track(void) {
    /* At tick 0: sf=1/mi=0 twice (one entry), sf=1/mi=1 (a different mode, kept), sf=2/mi=0 (kept). */
    unsigned char track[] = {
        0x00, 0xFF, 0x59, 0x02, 0x01, 0x00,
        0x00, 0xFF, 0x59, 0x02, 0x01, 0x00,
        0x00, 0xFF, 0x59, 0x02, 0x01, 0x01,
        0x00, 0xFF, 0x59, 0x02, 0x02, 0x00,
        0x00, 0xFF, 0x2F, 0x00,
    };
    const unsigned char *tracks[1] = {track};
    size_t sizes[1] = {sizeof(track)};
    unsigned char file[128];
    size_t file_size = test_smf(file, 0, 96, tracks, sizes, 1);

    MidiKeySignatureEvents events = {NULL, 0};
    assert(test_midi_import_key_signatures(file, file_size, &events) == MIDI_IMPORT_OK);
    assert(events.count == 3);
    assert(events.events[0].sharps_flats == 1 && events.events[0].mode_hint == MIDI_MODE_HINT_MAJOR);
    assert(events.events[1].sharps_flats == 1 && events.events[1].mode_hint == MIDI_MODE_HINT_MINOR);
    assert(events.events[2].sharps_flats == 2 && events.events[2].mode_hint == MIDI_MODE_HINT_MAJOR);
    midi_key_signature_events_destroy(&events);
}

static void test_import_no_key_signature(void) {
    unsigned char track[] = {
        0x00, 0x90, 0x3C, 0x64,
        0x60, 0x80, 0x3C, 0x40,
        0x00, 0xFF, 0x2F, 0x00,
    };
    const unsigned char *tracks[1] = {track};
    size_t sizes[1] = {sizeof(track)};
    unsigned char file[128];
    size_t file_size = test_smf(file, 0, 96, tracks, sizes, 1);

    MidiKeySignatureEvents events = {(MidiKeySignatureEvent *)0x1, 99};
    assert(test_midi_import_key_signatures(file, file_size, &events) == MIDI_IMPORT_OK);
    assert(events.count == 0);
    assert(events.events == NULL);
    midi_key_signature_events_destroy(&events);
}

static void test_import_skips_malformed_events(void) {
    /* A length-3 key signature, then one with sf = 9 (out of range), then a valid sf = 2 at tick 16. */
    unsigned char track[] = {
        0x00, 0xFF, 0x59, 0x03, 0x01, 0x00, 0x00,
        0x00, 0xFF, 0x59, 0x02, 0x09, 0x00,
        0x10, 0xFF, 0x59, 0x02, 0x02, 0x00,
        0x00, 0xFF, 0x2F, 0x00,
    };
    const unsigned char *tracks[1] = {track};
    size_t sizes[1] = {sizeof(track)};
    unsigned char file[128];
    size_t file_size = test_smf(file, 0, 96, tracks, sizes, 1);

    MidiKeySignatureEvents events = {NULL, 0};
    assert(test_midi_import_key_signatures(file, file_size, &events) == MIDI_IMPORT_OK);
    assert(events.count == 1);
    assert(events.events[0].sharps_flats == 2);
    assert(events.events[0].tick == 16);
    midi_key_signature_events_destroy(&events);
}

static void test_import_mode_hints(void) {
    unsigned char track[] = {
        0x00, 0xFF, 0x59, 0x02, 0x01, 0x01, /* E minor */
        0x10, 0xFF, 0x59, 0x02, 0x02, 0x02, /* mi = 2: kept, unknown hint */
        0x00, 0xFF, 0x2F, 0x00,
    };
    const unsigned char *tracks[1] = {track};
    size_t sizes[1] = {sizeof(track)};
    unsigned char file[128];
    size_t file_size = test_smf(file, 0, 96, tracks, sizes, 1);

    MidiKeySignatureEvents events = {NULL, 0};
    assert(test_midi_import_key_signatures(file, file_size, &events) == MIDI_IMPORT_OK);
    assert(events.count == 2);
    assert(events.events[0].mode_hint == MIDI_MODE_HINT_MINOR && events.events[0].mode_byte == 1);
    assert(events.events[1].mode_hint == MIDI_MODE_HINT_UNKNOWN && events.events[1].mode_byte == 2);
    /* The hint is carried next to the KeySignature; the signature itself is the same for both modes. */
    assert_signature(events.events[0].key_signature, "F", 1);
    midi_key_signature_events_destroy(&events);
}

static void test_import_structural_errors(void) {
    MidiKeySignatureEvents untouched = {(MidiKeySignatureEvent *)0x1, 99};
    unsigned char dummy[1] = {0};
    assert(test_midi_import_key_signatures(dummy, 1, &untouched) == MIDI_IMPORT_ERR_TRUNCATED);
    assert(untouched.count == 99);

    unsigned char track[] = {0x00, 0xFF, 0x59, 0x02, 0x01, 0x00, 0x00, 0xFF, 0x2F, 0x00};
    const unsigned char *tracks[1] = {track};
    size_t sizes[1] = {sizeof(track)};
    unsigned char file[128];

    /* SMPTE division, and a division of zero, are rejected. */
    size_t file_size = test_smf(file, 0, 0xE728, tracks, sizes, 1);
    assert(test_midi_import_key_signatures(file, file_size, &untouched) == MIDI_IMPORT_ERR_UNSUPPORTED_DIVISION);
    file_size = test_smf(file, 0, 0, tracks, sizes, 1);
    assert(test_midi_import_key_signatures(file, file_size, &untouched) == MIDI_IMPORT_ERR_UNSUPPORTED_DIVISION);

    /* Format 2 is unsupported. */
    file_size = test_smf(file, 2, 96, tracks, sizes, 1);
    assert(test_midi_import_key_signatures(file, file_size, &untouched) == MIDI_IMPORT_ERR_UNSUPPORTED_FORMAT);

    /* A truncated meta-event is a bad track. */
    unsigned char truncated[] = {0x00, 0xFF, 0x59, 0x02, 0x01};
    const unsigned char *bad_tracks[1] = {truncated};
    size_t bad_sizes[1] = {sizeof(truncated)};
    file_size = test_smf(file, 0, 96, bad_tracks, bad_sizes, 1);
    assert(test_midi_import_key_signatures(file, file_size, &untouched) == MIDI_IMPORT_ERR_BAD_TRACK);
    assert(untouched.count == 99);
}

/* The corpus files give their key signatures in time order, each with an sf in range, a signature and a position. */
static void check_corpus_file(const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_CORPUS_DIR, name);
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    assert(length > 0);
    unsigned char *data = (unsigned char *)malloc((size_t)length);
    assert(data != NULL);
    size_t read_count = fread(data, 1, (size_t)length, file);
    fclose(file);
    assert(read_count == (size_t)length);

    MidiKeySignatureEvents events = {NULL, 0};
    assert(test_midi_import_key_signatures(data, (size_t)length, &events) == MIDI_IMPORT_OK);
    for (size_t i = 0; i < events.count; i++) {
        assert(events.events[i].sharps_flats >= -7 && events.events[i].sharps_flats <= 7);
        assert(events.events[i].key_signature != NULL);
        assert(i == 0 || events.events[i - 1].tick <= events.events[i].tick);
    }
    midi_key_signature_events_destroy(&events);
    free(data);
}

static void test_corpus_key_signatures_in_time_order(void) {
    check_corpus_file("Mozart 545.mid");
    check_corpus_file("Clementi 36.mid");
    check_corpus_file("Field 5.mid");
}

int main(void) {
    test_finds_sharp_major_key_signature();
    test_finds_flat_key_signature();
    test_finds_minor_key_signature();
    test_no_key_signature_present();
    test_truncated_key_signature_meta_event_fails_cleanly();

    test_build_all_fifteen_values();
    test_build_rejects_out_of_range();
    test_import_single_key_signature();
    test_import_key_changes_with_positions();
    test_import_fractional_position();
    test_import_merges_tracks_in_time_order();
    test_import_same_tick_orders_by_track();
    test_import_duplicates_and_conflicts_on_one_track();
    test_import_no_key_signature();
    test_import_skips_malformed_events();
    test_import_mode_hints();
    test_import_structural_errors();
    test_corpus_key_signatures_in_time_order();
    return 0;
}
