#include "io/midi_renderer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* The largest tick a variable-length quantity holds (four bytes of seven bits). */
#define MAX_TICK 0x0FFFFFFFu

/* The MIDI defaults written with every time signature: clocks per metronome click, and notated 32nds per quarter. */
#define CLOCKS_PER_CLICK 24
#define THIRTY_SECONDS_PER_QUARTER 8

/* The largest tempo a set-tempo event holds (three bytes), in microseconds per quarter note. */
#define MAX_MICROSECONDS_PER_QUARTER 0xFFFFFFu

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

/* --- exact ticks --- */

/* tick = numerator / denominator * 4 * division, which must be a whole number of ticks within range. */
static int to_tick(long numerator, unsigned long denominator, unsigned division, uint32_t *out) {
    if (numerator < 0 || denominator == 0) {
        return 0;
    }
    uint64_t scale = 4ull * division;
    if ((uint64_t)numerator > UINT64_MAX / scale) {
        return 0;
    }
    uint64_t scaled = (uint64_t)numerator * scale;
    if (scaled % denominator != 0) {
        return 0;
    }
    uint64_t tick = scaled / denominator;
    if (tick > MAX_TICK) {
        return 0;
    }
    *out = (uint32_t)tick;
    return 1;
}

/* --- a track's events, sorted into tick order before they are written --- */

typedef struct {
    uint32_t tick;
    int order;    /* at an equal tick, lower first */
    size_t index; /* then input order, which makes the sort a total order */
    unsigned char bytes[8];
    size_t length;
} TrackEvent;

typedef struct {
    TrackEvent *items;
    size_t count;
    size_t capacity;
} TrackEvents;

static int add_event(TrackEvents *events, uint32_t tick, int order, const unsigned char *bytes, size_t length) {
    if (events->count == events->capacity) {
        size_t capacity = events->capacity == 0 ? 16 : events->capacity * 2;
        TrackEvent *grown = (TrackEvent *)realloc(events->items, capacity * sizeof(*grown));
        if (grown == NULL) {
            return 0;
        }
        events->items = grown;
        events->capacity = capacity;
    }
    TrackEvent *event = &events->items[events->count];
    event->tick = tick;
    event->order = order;
    event->index = events->count;
    memcpy(event->bytes, bytes, length);
    event->length = length;
    events->count++;
    return 1;
}

static int compare_events(const void *left, const void *right) {
    const TrackEvent *a = (const TrackEvent *)left;
    const TrackEvent *b = (const TrackEvent *)right;
    if (a->tick != b->tick) {
        return a->tick < b->tick ? -1 : 1;
    }
    if (a->order != b->order) {
        return a->order < b->order ? -1 : 1;
    }
    if (a->index != b->index) {
        return a->index < b->index ? -1 : 1;
    }
    return 0;
}

/* Writes the events in order, each after its delta time, then End of Track. */
static void write_events(Bytes *bytes, TrackEvents *events) {
    if (events->count > 1) {
        qsort(events->items, events->count, sizeof(TrackEvent), compare_events);
    }
    uint32_t previous = 0;
    for (size_t i = 0; i < events->count; i++) {
        put_vlq(bytes, events->items[i].tick - previous);
        put_bytes(bytes, events->items[i].bytes, events->items[i].length);
        previous = events->items[i].tick;
    }
    put_end_of_track(bytes, 0);
}

/* --- the conductor track's meta-events --- */

enum { ORDER_TEMPO = 0, ORDER_TIME_SIGNATURE = 1, ORDER_KEY_SIGNATURE = 2 };

static MidiRenderResult collect_conductor(TrackEvents *events, const MeterMap *meter_map, const TempoMap *tempo_map,
                                          const MidiKeySignatureEvent *key_signatures, size_t key_signature_count,
                                          unsigned division) {
    if (tempo_map != NULL) {
        for (size_t i = 0; i < tempo_map_region_count(tempo_map); i++) {
            const TempoMapRegion *region = tempo_map_region(tempo_map, i);
            const MusicalTime *start = tempo_map_region_start(region);
            unsigned long microseconds = tempo_map_region_microseconds_per_quarter(region);
            uint32_t tick;
            if (microseconds == 0 || microseconds > MAX_MICROSECONDS_PER_QUARTER ||
                !to_tick(musical_time_numerator(start), musical_time_denominator(start), division, &tick)) {
                return MIDI_RENDER_ERR_NOT_REPRESENTABLE;
            }
            unsigned char meta[6] = {0xFF, 0x51, 0x03, (unsigned char)((microseconds >> 16) & 0xFF),
                                     (unsigned char)((microseconds >> 8) & 0xFF), (unsigned char)(microseconds & 0xFF)};
            if (!add_event(events, tick, ORDER_TEMPO, meta, sizeof(meta))) {
                return MIDI_RENDER_ERR_ALLOC;
            }
        }
    }
    if (meter_map != NULL) {
        for (size_t i = 0; i < meter_map_region_count(meter_map); i++) {
            const MeterMapRegion *region = meter_map_region(meter_map, i);
            const MusicalTime *start = meter_map_region_start(region);
            const TimeSignature *signature = meter_map_region_time_signature(region);
            unsigned top = time_signature_top_number(signature);
            int exponent = time_signature_bottom_unit_exponent(signature);
            uint32_t tick;
            if (top == 0 || top > 255 || exponent < 0 || exponent > 7 ||
                !to_tick(musical_time_numerator(start), musical_time_denominator(start), division, &tick)) {
                return MIDI_RENDER_ERR_NOT_REPRESENTABLE;
            }
            unsigned char meta[7] = {0xFF, 0x58, 0x04, (unsigned char)top, (unsigned char)exponent,
                                     CLOCKS_PER_CLICK, THIRTY_SECONDS_PER_QUARTER};
            if (!add_event(events, tick, ORDER_TIME_SIGNATURE, meta, sizeof(meta))) {
                return MIDI_RENDER_ERR_ALLOC;
            }
        }
    }
    for (size_t i = 0; i < key_signature_count; i++) {
        if (key_signatures[i].position == NULL) {
            return MIDI_RENDER_ERR_NULL_ARG;
        }
        int sharps_flats = key_signatures[i].sharps_flats;
        uint32_t tick;
        if (sharps_flats < -7 || sharps_flats > 7 ||
            !to_tick(musical_time_numerator(key_signatures[i].position),
                     musical_time_denominator(key_signatures[i].position), division, &tick)) {
            return MIDI_RENDER_ERR_NOT_REPRESENTABLE;
        }
        unsigned char meta[5] = {0xFF, 0x59, 0x02, (unsigned char)(int8_t)sharps_flats, key_signatures[i].mode_byte};
        if (!add_event(events, tick, ORDER_KEY_SIGNATURE, meta, sizeof(meta))) {
            return MIDI_RENDER_ERR_ALLOC;
        }
    }
    return MIDI_RENDER_OK;
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

    /* The conductor's events are checked and collected first, so a failure writes nothing. */
    TrackEvents conductor_events = {NULL, 0, 0};
    MidiRenderResult status =
        collect_conductor(&conductor_events, meter_map, tempo_map, key_signatures, key_signature_count, division);
    if (status != MIDI_RENDER_OK) {
        free(conductor_events.items);
        return status;
    }

    Bytes bytes = {NULL, 0, 0, 0};

    /* The header chunk: format 1, the track count (the conductor alone so far), the division. */
    put_bytes(&bytes, (const unsigned char *)"MThd", 4);
    put_u32(&bytes, 6);
    put_u16(&bytes, 1);
    put_u16(&bytes, 1);
    put_u16(&bytes, division);

    /* The conductor track. */
    size_t conductor = begin_track(&bytes);
    write_events(&bytes, &conductor_events);
    end_track(&bytes, conductor);
    free(conductor_events.items);

    if (bytes.failed) {
        free(bytes.data);
        return MIDI_RENDER_ERR_ALLOC;
    }
    *out_data = bytes.data;
    *out_size = bytes.size;
    return MIDI_RENDER_OK;
}
