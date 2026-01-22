#include "io/midi_renderer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* The largest tick a variable-length quantity holds (four bytes of seven bits). */
#define MAX_TICK 0x0FFFFFFFu

/* --- a growing byte buffer --- */

typedef struct {
    unsigned char *data;
    size_t size;
    size_t capacity;
    int failed; /* an allocation failed; everything after is ignored */
} Bytes;

static void bytes_reserve(Bytes *bytes, size_t extra) {
    if (bytes->failed || bytes->size + extra <= bytes->capacity) {
        return;
    }
    size_t capacity = bytes->capacity == 0 ? 256 : bytes->capacity;
    while (capacity < bytes->size + extra) {
        capacity *= 2;
    }
    unsigned char *grown = (unsigned char *)realloc(bytes->data, capacity);
    if (grown == NULL) {
        bytes->failed = 1;
        return;
    }
    bytes->data = grown;
    bytes->capacity = capacity;
}

static void put_u8(Bytes *bytes, unsigned value) {
    bytes_reserve(bytes, 1);
    if (!bytes->failed) {
        bytes->data[bytes->size++] = (unsigned char)value;
    }
}

static void put_u16(Bytes *bytes, unsigned value) {
    put_u8(bytes, (value >> 8) & 0xFF);
    put_u8(bytes, value & 0xFF);
}

static void put_u32(Bytes *bytes, uint32_t value) {
    put_u8(bytes, (value >> 24) & 0xFF);
    put_u8(bytes, (value >> 16) & 0xFF);
    put_u8(bytes, (value >> 8) & 0xFF);
    put_u8(bytes, value & 0xFF);
}

static void put_bytes(Bytes *bytes, const unsigned char *data, size_t length) {
    bytes_reserve(bytes, length);
    if (!bytes->failed && length > 0) {
        memcpy(bytes->data + bytes->size, data, length);
        bytes->size += length;
    }
}

/* A variable-length quantity: seven bits per byte, most significant first, the high bit set on all but the last. */
static void put_vlq(Bytes *bytes, uint32_t value) {
    unsigned char groups[4];
    size_t count = 0;
    do {
        groups[count++] = (unsigned char)(value & 0x7F);
        value >>= 7;
    } while (value > 0);
    while (count > 0) {
        count--;
        put_u8(bytes, groups[count] | (count > 0 ? 0x80 : 0));
    }
}

/* Starts a track chunk, returning where its length field is, to patch once the track is written. */
static size_t begin_track(Bytes *bytes) {
    put_bytes(bytes, (const unsigned char *)"MTrk", 4);
    size_t length_at = bytes->size;
    put_u32(bytes, 0);
    return length_at;
}

static void end_track(Bytes *bytes, size_t length_at) {
    if (bytes->failed) {
        return;
    }
    uint32_t length = (uint32_t)(bytes->size - length_at - 4);
    bytes->data[length_at] = (unsigned char)((length >> 24) & 0xFF);
    bytes->data[length_at + 1] = (unsigned char)((length >> 16) & 0xFF);
    bytes->data[length_at + 2] = (unsigned char)((length >> 8) & 0xFF);
    bytes->data[length_at + 3] = (unsigned char)(length & 0xFF);
}

/* The End of Track meta-event, at the delta given. */
static void put_end_of_track(Bytes *bytes, uint32_t delta) {
    put_vlq(bytes, delta);
    put_u8(bytes, 0xFF);
    put_u8(bytes, 0x2F);
    put_u8(bytes, 0x00);
}

MidiRenderResult midi_renderer_render(NoteEvent *const *events, PitchNumericValue *const *pitch_values, size_t count,
                                      const MeterMap *meter_map, const TempoMap *tempo_map,
                                      const MidiKeySignatureEvent *key_signatures, size_t key_signature_count,
                                      unsigned division, unsigned char **out_data, size_t *out_size) {
    if (out_data == NULL || out_size == NULL || (count > 0 && (events == NULL || pitch_values == NULL)) ||
        (key_signature_count > 0 && key_signatures == NULL)) {
        return MIDI_RENDER_ERR_NULL_ARG;
    }
    for (size_t i = 0; i < count; i++) {
        if (events[i] == NULL || pitch_values[i] == NULL) {
            return MIDI_RENDER_ERR_NULL_ARG;
        }
    }
    if (division == 0 || division > MIDI_RENDER_MAX_DIVISION) {
        return MIDI_RENDER_ERR_BAD_DIVISION;
    }
    (void)meter_map;
    (void)tempo_map;

    Bytes bytes = {NULL, 0, 0, 0};

    /* The header chunk: format 1, the track count (the conductor alone so far), the division. */
    put_bytes(&bytes, (const unsigned char *)"MThd", 4);
    put_u32(&bytes, 6);
    put_u16(&bytes, 1);
    put_u16(&bytes, 1);
    put_u16(&bytes, division);

    /* The conductor track. */
    size_t conductor = begin_track(&bytes);
    put_end_of_track(&bytes, 0);
    end_track(&bytes, conductor);

    if (bytes.failed) {
        free(bytes.data);
        return MIDI_RENDER_ERR_ALLOC;
    }
    *out_data = bytes.data;
    *out_size = bytes.size;
    return MIDI_RENDER_OK;
}
