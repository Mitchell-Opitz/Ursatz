#include "io/midi_event_decoder.h"

#include <stdlib.h>

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
   clear) is found. */
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

typedef struct {
    MidiNoteEvent *items;
    size_t count;
    size_t capacity;
} EventList;

static int event_list_push(EventList *list, MidiNoteEvent event) {
    if (list->count == list->capacity) {
        size_t new_capacity = (list->capacity == 0) ? 16 : list->capacity * 2;
        MidiNoteEvent *grown = realloc(list->items, sizeof(MidiNoteEvent) * new_capacity);
        if (grown == NULL) {
            return -1;
        }
        list->items = grown;
        list->capacity = new_capacity;
    }
    list->items[list->count++] = event;
    return 0;
}

MidiEventDecoderResult midi_event_decoder_decode(const unsigned char *track_data, size_t track_size,
                                                  MidiNoteEvent **out_events, size_t *out_count) {
    if (track_data == NULL || out_events == NULL || out_count == NULL) {
        return MIDI_EVENT_DECODER_ERR_NULL_TRACK;
    }

    Cursor c = {track_data, track_size, 0};
    EventList list = {NULL, 0, 0};

    uint32_t tick = 0;
    unsigned char running_status = 0;

    while (c.pos < c.size) {
        uint32_t delta = 0;
        if (read_vlq(&c, &delta) != 0) {
            free(list.items);
            return MIDI_EVENT_DECODER_ERR_TRUNCATED;
        }
        tick += delta;

        if (!cursor_has(&c, 1)) {
            free(list.items);
            return MIDI_EVENT_DECODER_ERR_TRUNCATED;
        }

        unsigned char status = c.data[c.pos];
        if (status & 0x80) {
            c.pos++;
            running_status = status;
        } else {
            /* Running status: reuse the previous status byte, and this
               byte is the first data byte. */
            status = running_status;
        }

        if (status == 0xFF) {
            /* Meta event: type byte + VLQ length + data. */
            if (!cursor_has(&c, 1)) {
                free(list.items);
                return MIDI_EVENT_DECODER_ERR_TRUNCATED;
            }
            c.pos++; /* meta type, not interpreted */
            uint32_t len = 0;
            if (read_vlq(&c, &len) != 0 || !cursor_has(&c, len)) {
                free(list.items);
                return MIDI_EVENT_DECODER_ERR_TRUNCATED;
            }
            c.pos += len;
            continue;
        }

        if (status == 0xF0 || status == 0xF7) {
            /* Sysex / escape: VLQ length + data. */
            uint32_t len = 0;
            if (read_vlq(&c, &len) != 0 || !cursor_has(&c, len)) {
                free(list.items);
                return MIDI_EVENT_DECODER_ERR_TRUNCATED;
            }
            c.pos += len;
            continue;
        }

        unsigned char high_nibble = status & 0xF0;
        unsigned char channel = status & 0x0F;

        if (high_nibble == 0x80 || high_nibble == 0x90) {
            /* Note off / note on: 2 data bytes. */
            if (!cursor_has(&c, 2)) {
                free(list.items);
                return MIDI_EVENT_DECODER_ERR_TRUNCATED;
            }
            unsigned char note = cursor_byte(&c);
            unsigned char velocity = cursor_byte(&c);

            MidiNoteEvent event;
            event.tick = tick;
            event.channel = channel;
            event.note = note;
            event.velocity = velocity;
            event.kind = (high_nibble == 0x90 && velocity > 0) ? MIDI_NOTE_ON : MIDI_NOTE_OFF;

            if (event_list_push(&list, event) != 0) {
                free(list.items);
                return MIDI_EVENT_DECODER_ERR_ALLOC;
            }
        } else if (high_nibble == 0xA0 || high_nibble == 0xB0 || high_nibble == 0xE0) {
            /* Polyphonic aftertouch, CC, pitch bend: 2 data bytes, ignored. */
            if (!cursor_has(&c, 2)) {
                free(list.items);
                return MIDI_EVENT_DECODER_ERR_TRUNCATED;
            }
            c.pos += 2;
        } else if (high_nibble == 0xC0 || high_nibble == 0xD0) {
            /* Program change, channel aftertouch: 1 data byte, ignored. */
            if (!cursor_has(&c, 1)) {
                free(list.items);
                return MIDI_EVENT_DECODER_ERR_TRUNCATED;
            }
            c.pos += 1;
        } else {
            /* Unrecognized status byte with no defined length: cannot
               safely skip, so fail cleanly rather than guess. */
            free(list.items);
            return MIDI_EVENT_DECODER_ERR_TRUNCATED;
        }
    }

    *out_events = list.items;
    *out_count = list.count;
    return MIDI_EVENT_DECODER_OK;
}
