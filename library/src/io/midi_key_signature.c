#include "io/midi_key_signature.h"

#include <stdlib.h>

#include "io/smf_reader.h"

typedef struct {
    const unsigned char *data;
    size_t size;
    size_t pos;
} Cursor;

static int cursor_has(const Cursor *c, size_t n) {
    return c->pos + n <= c->size;
}

static unsigned char cursor_byte(Cursor *c) {
    return c->data[c->pos++];
}

/* Reads a MIDI variable-length quantity. Returns 0 on success, -1 if
   the cursor runs out of bytes before a terminating byte (top bit
   clear) is found. Mirrors midi_event_decoder's read_vlq. */
static int read_vlq(Cursor *c, uint32_t *out) {
    uint32_t value = 0;
    for (int i = 0; i < 4; i++) {
        if (!cursor_has(c, 1)) {
            return -1;
        }
        unsigned char b = cursor_byte(c);
        value = (value << 7) | (uint32_t)(b & 0x7F);
        if ((b & 0x80) == 0) {
            *out = value;
            return 0;
        }
    }
    return -1;
}

/* Walks one track's raw bytes looking for a Key Signature meta-event
   (0xFF 0x59, 2-byte payload: sf as signed, mi as 0=major/1=minor).
   Skips every other event just enough to keep byte offsets correct
   (same event-length rules as midi_event_decoder_decode), without
   decoding note events at all. Returns 0 on success (out->found tells
   whether one was located in this track), -1 on malformed track bytes. */
static int scan_track_for_key_signature(const unsigned char *track_data, size_t track_size, MidiKeySignature *out) {
    Cursor c = {track_data, track_size, 0};
    unsigned char running_status = 0;

    while (c.pos < c.size) {
        uint32_t delta = 0;
        if (read_vlq(&c, &delta) != 0) {
            return -1;
        }

        if (!cursor_has(&c, 1)) {
            return -1;
        }

        unsigned char status = c.data[c.pos];
        if (status & 0x80) {
            c.pos++;
            running_status = status;
        } else {
            status = running_status;
        }

        if (status == 0xFF) {
            if (!cursor_has(&c, 1)) {
                return -1;
            }
            unsigned char meta_type = cursor_byte(&c);
            uint32_t len = 0;
            if (read_vlq(&c, &len) != 0 || !cursor_has(&c, len)) {
                return -1;
            }
            if (meta_type == 0x59 && len == 2 && !out->found) {
                out->found = true;
                out->sharps_flats = (int8_t)c.data[c.pos];
                out->is_minor = c.data[c.pos + 1] != 0;
            }
            c.pos += len;
            continue;
        }

        if (status == 0xF0 || status == 0xF7) {
            uint32_t len = 0;
            if (read_vlq(&c, &len) != 0 || !cursor_has(&c, len)) {
                return -1;
            }
            c.pos += len;
            continue;
        }

        unsigned char high_nibble = status & 0xF0;

        if (high_nibble == 0x80 || high_nibble == 0x90 || high_nibble == 0xA0 || high_nibble == 0xB0 ||
            high_nibble == 0xE0) {
            if (!cursor_has(&c, 2)) {
                return -1;
            }
            c.pos += 2;
        } else if (high_nibble == 0xC0 || high_nibble == 0xD0) {
            if (!cursor_has(&c, 1)) {
                return -1;
            }
            c.pos += 1;
        } else {
            return -1;
        }
    }

    return 0;
}

MidiKeySignatureResult midi_key_signature_scan(const unsigned char *data, size_t size, MidiKeySignature *out) {
    if (data == NULL || out == NULL) {
        return MIDI_KEY_SIGNATURE_ERR_NULL_BUFFER;
    }

    SmfHeader header;
    SmfTrack *tracks = NULL;
    size_t track_count = 0;
    SmfReaderResult smf_result = smf_reader_parse(data, size, &header, &tracks, &track_count);
    if (smf_result != SMF_READER_OK) {
        switch (smf_result) {
        case SMF_READER_ERR_NULL_BUFFER:
            return MIDI_KEY_SIGNATURE_ERR_NULL_BUFFER;
        case SMF_READER_ERR_TRUNCATED:
            return MIDI_KEY_SIGNATURE_ERR_TRUNCATED;
        case SMF_READER_ERR_UNSUPPORTED_FORMAT:
            return MIDI_KEY_SIGNATURE_ERR_UNSUPPORTED_FORMAT;
        case SMF_READER_ERR_UNSUPPORTED_DIVISION:
            return MIDI_KEY_SIGNATURE_ERR_UNSUPPORTED_DIVISION;
        case SMF_READER_ERR_BAD_TRACK:
            return MIDI_KEY_SIGNATURE_ERR_BAD_TRACK;
        case SMF_READER_ERR_BAD_HEADER:
        default:
            return MIDI_KEY_SIGNATURE_ERR_BAD_HEADER;
        }
    }

    MidiKeySignature result = {false, 0, false};
    MidiKeySignatureResult failure = MIDI_KEY_SIGNATURE_OK;

    for (size_t t = 0; t < track_count && !result.found; t++) {
        if (scan_track_for_key_signature(tracks[t].data, tracks[t].size, &result) != 0) {
            failure = MIDI_KEY_SIGNATURE_ERR_BAD_TRACK;
            break;
        }
    }

    free(tracks);

    if (failure != MIDI_KEY_SIGNATURE_OK) {
        return failure;
    }

    *out = result;
    return MIDI_KEY_SIGNATURE_OK;
}
