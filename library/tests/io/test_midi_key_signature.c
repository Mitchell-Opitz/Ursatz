#include "io/midi_key_signature.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/*
 * midi_key_signature tests: locating a Key Signature meta-event
 * (0xFF 0x59) in a sharp key, a flat key, a minor key, a file with no
 * such event, and a file with a truncated meta-event payload.
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

static size_t build_smf_type0(unsigned char *buf, const unsigned char *track_body, size_t track_body_size) {
    size_t pos = 0;
    append_bytes(buf, &pos, "MThd", 4);
    append_u32(buf, &pos, 6);
    append_u16(buf, &pos, 0);
    append_u16(buf, &pos, 1);
    append_u16(buf, &pos, 96);
    append_bytes(buf, &pos, "MTrk", 4);
    append_u32(buf, &pos, (uint32_t)track_body_size);
    append_bytes(buf, &pos, track_body, track_body_size);
    return pos;
}

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
    size_t file_size = build_smf_type0(file, track, sizeof(track));

    MidiKeySignature key = {0};
    MidiKeySignatureResult r = midi_key_signature_scan(file, file_size, &key);

    assert(r == MIDI_KEY_SIGNATURE_OK);
    assert(key.found);
    assert(key.sharps_flats == 1);
    assert(!key.is_minor);
}

static void test_finds_flat_key_signature(void) {
    /* Eb major: 3 flats, major. sf byte -3 as two's-complement 0xFD. */
    unsigned char track[] = {
        0x00, 0xFF, 0x59, 0x02, 0xFD, 0x00,
        0x00, 0xFF, 0x2F, 0x00
    };
    unsigned char file[64];
    size_t file_size = build_smf_type0(file, track, sizeof(track));

    MidiKeySignature key = {0};
    MidiKeySignatureResult r = midi_key_signature_scan(file, file_size, &key);

    assert(r == MIDI_KEY_SIGNATURE_OK);
    assert(key.found);
    assert(key.sharps_flats == -3);
    assert(!key.is_minor);
}

static void test_finds_minor_key_signature(void) {
    /* E minor: 1 sharp, minor. */
    unsigned char track[] = {
        0x00, 0xFF, 0x59, 0x02, 0x01, 0x01,
        0x00, 0xFF, 0x2F, 0x00
    };
    unsigned char file[64];
    size_t file_size = build_smf_type0(file, track, sizeof(track));

    MidiKeySignature key = {0};
    MidiKeySignatureResult r = midi_key_signature_scan(file, file_size, &key);

    assert(r == MIDI_KEY_SIGNATURE_OK);
    assert(key.found);
    assert(key.sharps_flats == 1);
    assert(key.is_minor);
}

static void test_no_key_signature_present(void) {
    unsigned char track[] = {
        0x00, 0x90, 0x3C, 0x64,
        0x60, 0x80, 0x3C, 0x40,
        0x00, 0xFF, 0x2F, 0x00
    };
    unsigned char file[64];
    size_t file_size = build_smf_type0(file, track, sizeof(track));

    MidiKeySignature key = {0};
    MidiKeySignatureResult r = midi_key_signature_scan(file, file_size, &key);

    assert(r == MIDI_KEY_SIGNATURE_OK);
    assert(!key.found);
}

static void test_truncated_key_signature_meta_event_fails_cleanly(void) {
    /* Declares a 2-byte payload but only supplies 1 byte before the
       buffer ends. */
    unsigned char track[] = {0x00, 0xFF, 0x59, 0x02, 0x01};
    unsigned char file[64];
    size_t file_size = build_smf_type0(file, track, sizeof(track));

    MidiKeySignature key = {0};
    MidiKeySignatureResult r = midi_key_signature_scan(file, file_size, &key);

    assert(r == MIDI_KEY_SIGNATURE_ERR_BAD_TRACK);
}

int main(void) {
    test_finds_sharp_major_key_signature();
    test_finds_flat_key_signature();
    test_finds_minor_key_signature();
    test_no_key_signature_present();
    test_truncated_key_signature_meta_event_fails_cleanly();
    return 0;
}
