#include "io/midi_renderer.h"

#include <stdbool.h>
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
    const unsigned char *text; /* a text meta-event's payload, borrowed from the caller; NULL for other events */
    size_t text_length;
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
    event->text = NULL;
    event->text_length = 0;
    events->count++;
    return 1;
}

/* A text meta-event (0xFF type, VLQ length, the bytes of text); text must outlive the render. */
static int add_text_event(TrackEvents *events, uint32_t tick, int order, unsigned char type, const char *text) {
    const unsigned char meta[2] = {0xFF, type};
    if (!add_event(events, tick, order, meta, sizeof(meta))) {
        return 0;
    }
    TrackEvent *event = &events->items[events->count - 1];
    event->text = (const unsigned char *)text;
    event->text_length = strlen(text);
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

static void sort_events(TrackEvents *events) {
    if (events->count > 1) {
        qsort(events->items, events->count, sizeof(TrackEvent), compare_events);
    }
}

/* Writes sorted events, each after its delta time, then End of Track. */
static void write_events(Bytes *bytes, const TrackEvents *events) {
    uint32_t previous = 0;
    for (size_t i = 0; i < events->count; i++) {
        put_vlq(bytes, events->items[i].tick - previous);
        put_bytes(bytes, events->items[i].bytes, events->items[i].length);
        if (events->items[i].text != NULL) {
            put_vlq(bytes, (uint32_t)events->items[i].text_length);
            put_bytes(bytes, events->items[i].text, events->items[i].text_length);
        }
        previous = events->items[i].tick;
    }
    put_end_of_track(bytes, 0);
}

/* --- the conductor track's meta-events --- */

/* ORDER_NAME is below every other order, so a name is first at tick 0. */
enum { ORDER_NAME = -1, ORDER_TEMPO = 0, ORDER_TIME_SIGNATURE = 1, ORDER_KEY_SIGNATURE = 2, ORDER_MARKER = 3 };

static MidiRenderResult collect_conductor(TrackEvents *events, const MeterMap *meter_map, const TempoMap *tempo_map,
                                          const MidiKeySignatureEvent *key_signatures, size_t key_signature_count,
                                          const MidiRenderOptions *options, unsigned division) {
    if (options != NULL && options->conductor_name != NULL &&
        !add_text_event(events, 0, ORDER_NAME, 0x03, options->conductor_name)) {
        return MIDI_RENDER_ERR_ALLOC;
    }
    if (tempo_map != NULL) {
        for (size_t i = 0; i < tempo_map_region_count(tempo_map); i++) {
            const TempoMapRegion *region = tempo_map_region(tempo_map, i);
            MusicalTime start = tempo_map_region_start(region);
            unsigned long microseconds = tempo_map_region_microseconds_per_quarter(region);
            uint32_t tick;
            if (microseconds == 0 || microseconds > MAX_MICROSECONDS_PER_QUARTER ||
                !to_tick((long)start.value.num, (unsigned long)start.value.den, division, &tick)) {
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
            MusicalTime start = meter_map_region_start(region);
            const TimeSignature *signature = meter_map_region_time_signature(region);
            unsigned top = time_signature_top_number(signature);
            int exponent = time_signature_bottom_unit_exponent(signature);
            uint32_t tick;
            if (top == 0 || top > 255 || exponent < 0 || exponent > 7 ||
                !to_tick((long)start.value.num, (unsigned long)start.value.den, division, &tick)) {
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
        int sharps_flats = key_signatures[i].sharps_flats;
        uint32_t tick;
        if (sharps_flats < -7 || sharps_flats > 7 ||
            !to_tick((long)key_signatures[i].position.value.num,
                     (unsigned long)key_signatures[i].position.value.den, division, &tick)) {
            return MIDI_RENDER_ERR_NOT_REPRESENTABLE;
        }
        unsigned char meta[5] = {0xFF, 0x59, 0x02, (unsigned char)(int8_t)sharps_flats, key_signatures[i].mode_byte};
        if (!add_event(events, tick, ORDER_KEY_SIGNATURE, meta, sizeof(meta))) {
            return MIDI_RENDER_ERR_ALLOC;
        }
    }
    if (options != NULL) {
        for (size_t i = 0; i < options->marker_count; i++) {
            const MusicalTime *position = &options->markers[i].position;
            uint32_t tick;
            if (!to_tick((long)position->value.num, (unsigned long)position->value.den, division, &tick)) {
                return MIDI_RENDER_ERR_NOT_REPRESENTABLE;
            }
            if (!add_text_event(events, tick, ORDER_MARKER, 0x06, options->markers[i].text)) {
                return MIDI_RENDER_ERR_ALLOC;
            }
        }
    }
    return MIDI_RENDER_OK;
}

/* --- the voice tracks --- */

/* The order of the events of a voice or part track at an equal tick: the name, then the program changes, then the note-offs, the
   sustain pedal up, the sustain pedal down and the note-ons, then (the sort's last key) the order they were added. */
enum { TRACK_ORDER_NAME = -2, TRACK_ORDER_PROGRAM = -1, TRACK_ORDER_OFF = 0, TRACK_ORDER_PEDAL_UP = 1, TRACK_ORDER_PEDAL_DOWN = 2, TRACK_ORDER_ON = 3 };

typedef struct {
    const char *voice_id; /* NULL: the notes with no voice */
    uint32_t first_tick;
    size_t first_index;
    size_t rank;    /* position among the voices by first sound, from 0: the channel follows it */
    size_t part;    /* with parts: the index of the part that holds the voice */
} Group;

static int same_voice(const char *a, const char *b) {
    return (a == NULL && b == NULL) || (a != NULL && b != NULL && strcmp(a, b) == 0);
}

/* The name the options give the voice's track, or NULL. */
static const char *track_name_for(const MidiRenderOptions *options, const char *voice_id) {
    if (options == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < options->track_name_count; i++) {
        if (same_voice(options->track_names[i].voice_id, voice_id)) {
            return options->track_names[i].name;
        }
    }
    return NULL;
}

static int compare_groups(const void *left, const void *right) {
    const Group *a = *(const Group *const *)left;
    const Group *b = *(const Group *const *)right;
    if (a->first_tick != b->first_tick) {
        return a->first_tick < b->first_tick ? -1 : 1;
    }
    if (a->first_index != b->first_index) {
        return a->first_index < b->first_index ? -1 : 1;
    }
    return 0;
}

/* Two notes of one pitch sounding together on one channel cannot be told apart by their note-offs. Events are sorted
   with note-offs first at an equal tick, so a note ending where another begins is not an overlap. */
static int overlaps_on_one_pitch(const TrackEvents *events) {
    unsigned char sounding[16][128] = {{0}};
    for (size_t i = 0; i < events->count; i++) {
        const unsigned char *event = events->items[i].bytes;
        unsigned kind = event[0] & 0xF0;
        if (kind == 0x90) {
            if (sounding[event[0] & 0x0F][event[1]]++ > 0) {
                return 1;
            }
        } else if (kind == 0x80) {
            sounding[event[0] & 0x0F][event[1]]--;
        }
    }
    return 0;
}

static void free_tracks(TrackEvents *tracks, size_t count) {
    if (tracks == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(tracks[i].items);
    }
    free(tracks);
}

/* The index of the part that lists the voice, or part_count when none does. */
static size_t part_of_voice(const MidiRenderOptions *options, const char *voice_id) {
    for (size_t p = 0; voice_id != NULL && p < options->part_count; p++) {
        for (size_t v = 0; v < options->parts[p].voice_count; v++) {
            if (strcmp(options->parts[p].voice_ids[v], voice_id) == 0) {
                return p;
            }
        }
    }
    return options->part_count;
}

static unsigned channel_of_rank(size_t rank) {
    return rank < 9 ? (unsigned)rank : (unsigned)rank + 1; /* skip percussion, channel 9 */
}

/* Writes the sustain pedal of each part onto every channel of that part. */
static MidiRenderResult add_pedals(TrackEvents *tracks, const MidiRenderOptions *options, const Group *groups, size_t group_count,
                                   unsigned division) {
    for (size_t p = 0; p < options->part_count; p++) {
        for (size_t k = 0; k < options->parts[p].pedal_count; k++) {
            uint32_t from, to;
            const TimeSpan *span = &options->parts[p].pedals[k];
            if (!to_tick((long)span->start.value.num, (unsigned long)span->start.value.den, division, &from) ||
                !to_tick((long)span->end.value.num, (unsigned long)span->end.value.den, division, &to) || to <= from) {
                return MIDI_RENDER_ERR_NOT_REPRESENTABLE;
            }
            for (size_t g = 0; g < group_count; g++) {
                if (groups[g].part != p) {
                    continue;
                }
                unsigned channel = channel_of_rank(groups[g].rank);
                unsigned char down[3] = {(unsigned char)(0xB0 | channel), 64, 127};
                unsigned char up[3] = {(unsigned char)(0xB0 | channel), 64, 0};
                if (!add_event(&tracks[p], from, TRACK_ORDER_PEDAL_DOWN, down, sizeof(down)) ||
                    !add_event(&tracks[p], to, TRACK_ORDER_PEDAL_UP, up, sizeof(up))) {
                    return MIDI_RENDER_ERR_ALLOC;
                }
            }
        }
    }
    return MIDI_RENDER_OK;
}

/*
 * Groups the notes by voice, checks every note, and builds each track's sorted events: one track per voice, or with
 * options->parts one track per part holding a channel for each of its voices. On OK *out_tracks is *out_count tracks in track
 * order (free with free_tracks); otherwise nothing is left allocated.
 */
static MidiRenderResult collect_voice_tracks(NoteEvent *const *events, PitchNumericValue *const *pitch_values,
                                             size_t count, const MidiRenderOptions *options, unsigned division,
                                             TrackEvents **out_tracks, size_t *out_count) {
    *out_tracks = NULL;
    *out_count = 0;
    bool with_parts = options != NULL && options->part_count > 0;
    if (count == 0 && !with_parts) {
        return MIDI_RENDER_OK;
    }

    uint32_t *starts = (uint32_t *)malloc((count + 1) * sizeof(uint32_t));
    uint32_t *ends = (uint32_t *)malloc((count + 1) * sizeof(uint32_t));
    unsigned char *pitches = (unsigned char *)malloc(count + 1);
    size_t *group_of = (size_t *)malloc((count + 1) * sizeof(size_t));
    Group groups[MIDI_RENDER_MAX_VOICE_TRACKS];
    size_t group_count = 0;
    MidiRenderResult status = MIDI_RENDER_OK;
    TrackEvents *tracks = NULL;
    size_t track_count = with_parts ? options->part_count : 0;

    if (starts == NULL || ends == NULL || pitches == NULL || group_of == NULL) {
        status = MIDI_RENDER_ERR_ALLOC;
    }
    for (size_t i = 0; i < count && status == MIDI_RENDER_OK; i++) {
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(events[i]));
        const Duration *duration = note_event_duration(events[i]);
        uint32_t length;
        if (!to_tick((long)position->value.num, (unsigned long)position->value.den, division, &starts[i]) ||
            duration->value.num <= 0 ||
            !to_tick((long)duration->value.num, (unsigned long)duration->value.den, division, &length) || length == 0 ||
            (uint64_t)starts[i] + length > MAX_TICK) {
            status = MIDI_RENDER_ERR_NOT_REPRESENTABLE;
            break;
        }
        ends[i] = starts[i] + length;

        long number = pitch_numeric_value_position_numerator(pitch_values[i]);
        if (pitch_numeric_value_position_denominator(pitch_values[i]) != 1 || number < 0 || number > 127) {
            status = MIDI_RENDER_ERR_NOT_REPRESENTABLE;
            break;
        }
        pitches[i] = (unsigned char)number;

        const VoiceReference *voice = note_event_optional_voice(events[i]);
        const char *voice_id = voice == NULL ? NULL : entity_id_value(voice_reference_target_id(voice));
        size_t g = 0;
        while (g < group_count && !same_voice(groups[g].voice_id, voice_id)) {
            g++;
        }
        if (g == group_count) {
            if (group_count == MIDI_RENDER_MAX_VOICE_TRACKS) {
                status = MIDI_RENDER_ERR_TOO_MANY_VOICES;
                break;
            }
            groups[g].voice_id = voice_id;
            groups[g].first_tick = starts[i];
            groups[g].first_index = i;
            groups[g].part = with_parts ? part_of_voice(options, voice_id) : 0;
            if (with_parts && groups[g].part == options->part_count) {
                status = MIDI_RENDER_ERR_NOT_REPRESENTABLE; /* a note with no voice, or a voice no part holds */
                break;
            }
            group_count++;
        } else if (starts[i] < groups[g].first_tick) {
            groups[g].first_tick = starts[i];
            groups[g].first_index = i;
        }
        group_of[i] = g;
    }

    if (status == MIDI_RENDER_OK) {
        /* Channels follow the order the voices first sound. */
        const Group *order[MIDI_RENDER_MAX_VOICE_TRACKS];
        for (size_t g = 0; g < group_count; g++) {
            order[g] = &groups[g];
        }
        if (group_count > 1) {
            qsort(order, group_count, sizeof(order[0]), compare_groups);
        }
        for (size_t rank = 0; rank < group_count; rank++) {
            groups[order[rank] - groups].rank = rank;
        }
        if (!with_parts) {
            track_count = group_count; /* a voice's track number is its rank */
        }

        tracks = (TrackEvents *)calloc(track_count > 0 ? track_count : 1, sizeof(TrackEvents));
        if (tracks == NULL) {
            status = MIDI_RENDER_ERR_ALLOC;
        }
        for (size_t i = 0; i < count && status == MIDI_RENDER_OK; i++) {
            size_t rank = groups[group_of[i]].rank;
            size_t track = with_parts ? groups[group_of[i]].part : rank;
            unsigned channel = channel_of_rank(rank);
            unsigned char on[3] = {(unsigned char)(0x90 | channel), pitches[i], MIDI_RENDER_NOTE_VELOCITY};
            unsigned char off[3] = {(unsigned char)(0x80 | channel), pitches[i], 0};
            /* Note-offs sort before note-ons at an equal tick. */
            if (!add_event(&tracks[track], starts[i], TRACK_ORDER_ON, on, sizeof(on)) ||
                !add_event(&tracks[track], ends[i], TRACK_ORDER_OFF, off, sizeof(off))) {
                status = MIDI_RENDER_ERR_ALLOC;
            }
        }
        if (with_parts) {
            for (size_t p = 0; p < options->part_count && status == MIDI_RENDER_OK; p++) {
                if (options->parts[p].name != NULL && !add_text_event(&tracks[p], 0, TRACK_ORDER_NAME, 0x03, options->parts[p].name)) {
                    status = MIDI_RENDER_ERR_ALLOC;
                }
            }
            for (size_t g = 0; g < group_count && status == MIDI_RENDER_OK; g++) {
                unsigned char change[2] = {(unsigned char)(0xC0 | channel_of_rank(groups[g].rank)),
                                           (unsigned char)options->parts[groups[g].part].program};
                if (!add_event(&tracks[groups[g].part], 0, TRACK_ORDER_PROGRAM, change, sizeof(change))) {
                    status = MIDI_RENDER_ERR_ALLOC;
                }
            }
            if (status == MIDI_RENDER_OK) {
                status = add_pedals(tracks, options, groups, group_count, division);
            }
        } else {
            for (size_t g = 0; g < group_count && status == MIDI_RENDER_OK; g++) {
                const char *name = track_name_for(options, groups[g].voice_id);
                if (name != NULL && !add_text_event(&tracks[groups[g].rank], 0, TRACK_ORDER_NAME, 0x03, name)) {
                    status = MIDI_RENDER_ERR_ALLOC;
                }
            }
        }
        for (size_t track = 0; track < track_count && status == MIDI_RENDER_OK; track++) {
            sort_events(&tracks[track]);
            if (overlaps_on_one_pitch(&tracks[track])) {
                status = MIDI_RENDER_ERR_NOT_REPRESENTABLE;
            }
        }
    }

    free(starts);
    free(ends);
    free(pitches);
    free(group_of);
    if (status != MIDI_RENDER_OK) {
        free_tracks(tracks, track_count);
        return status;
    }
    *out_tracks = tracks;
    *out_count = track_count;
    return MIDI_RENDER_OK;
}

/* Checks the options before anything is collected: NULL arguments, then values that cannot be written. */
static MidiRenderResult check_options(const MidiRenderOptions *options) {
    if (options == NULL) {
        return MIDI_RENDER_OK;
    }
    if (options->track_name_count > 0 && options->track_names == NULL) {
        return MIDI_RENDER_ERR_NULL_ARG;
    }
    for (size_t i = 0; i < options->track_name_count; i++) {
        if (options->track_names[i].name == NULL) {
            return MIDI_RENDER_ERR_NULL_ARG;
        }
    }
    if (options->marker_count > 0 && options->markers == NULL) {
        return MIDI_RENDER_ERR_NULL_ARG;
    }
    for (size_t i = 0; i < options->marker_count; i++) {
        if (options->markers[i].text == NULL) {
            return MIDI_RENDER_ERR_NULL_ARG;
        }
    }
    for (size_t i = 0; i < options->marker_count; i++) {
        if (strlen(options->markers[i].text) > MAX_TICK) {
            return MIDI_RENDER_ERR_NOT_REPRESENTABLE;
        }
    }
    if (options->conductor_name != NULL && strlen(options->conductor_name) > MAX_TICK) {
        return MIDI_RENDER_ERR_NOT_REPRESENTABLE;
    }
    if (options->part_count > 0 && options->parts == NULL) {
        return MIDI_RENDER_ERR_NULL_ARG;
    }
    for (size_t p = 0; p < options->part_count; p++) {
        const MidiRenderPart *part = &options->parts[p];
        if ((part->voice_count > 0 && part->voice_ids == NULL) || (part->pedal_count > 0 && part->pedals == NULL)) {
            return MIDI_RENDER_ERR_NULL_ARG;
        }
        for (size_t v = 0; v < part->voice_count; v++) {
            if (part->voice_ids[v] == NULL) {
                return MIDI_RENDER_ERR_NULL_ARG;
            }
        }
    }
    for (size_t p = 0; p < options->part_count; p++) {
        const MidiRenderPart *part = &options->parts[p];
        if (part->program < 0 || part->program > 127 || (part->name != NULL && strlen(part->name) > MAX_TICK)) {
            return MIDI_RENDER_ERR_NOT_REPRESENTABLE;
        }
        /* a voice belongs to one part, once */
        for (size_t v = 0; v < part->voice_count; v++) {
            for (size_t q = 0; q <= p; q++) {
                const MidiRenderPart *other = &options->parts[q];
                for (size_t w = 0; w < other->voice_count && (q < p || w < v); w++) {
                    if (strcmp(other->voice_ids[w], part->voice_ids[v]) == 0) {
                        return MIDI_RENDER_ERR_NOT_REPRESENTABLE;
                    }
                }
            }
        }
    }
    for (size_t i = 0; i < options->track_name_count; i++) {
        if (strlen(options->track_names[i].name) > MAX_TICK) {
            return MIDI_RENDER_ERR_NOT_REPRESENTABLE;
        }
        for (size_t j = 0; j < i; j++) {
            if (same_voice(options->track_names[i].voice_id, options->track_names[j].voice_id)) {
                return MIDI_RENDER_ERR_NOT_REPRESENTABLE;
            }
        }
    }
    return MIDI_RENDER_OK;
}

MidiRenderResult midi_renderer_render_with_options(NoteEvent *const *events, PitchNumericValue *const *pitch_values,
                                                   size_t count, const MeterMap *meter_map, const TempoMap *tempo_map,
                                                   const MidiKeySignatureEvent *key_signatures,
                                                   size_t key_signature_count, const MidiRenderOptions *options,
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

    /* Everything is checked and collected before a byte is written, so a failure writes nothing. */
    MidiRenderResult status = check_options(options);
    if (status != MIDI_RENDER_OK) {
        return status;
    }
    TrackEvents conductor_events = {NULL, 0, 0};
    status = collect_conductor(&conductor_events, meter_map, tempo_map, key_signatures, key_signature_count, options,
                               division);
    if (status != MIDI_RENDER_OK) {
        free(conductor_events.items);
        return status;
    }
    TrackEvents *voice_tracks = NULL;
    size_t voice_track_count = 0;
    status = collect_voice_tracks(events, pitch_values, count, options, division, &voice_tracks, &voice_track_count);
    if (status != MIDI_RENDER_OK) {
        free(conductor_events.items);
        return status;
    }
    sort_events(&conductor_events);

    Bytes bytes = {NULL, 0, 0, 0};

    /* The header chunk: format 1, the track count (the conductor and each voice), the division. */
    put_bytes(&bytes, (const unsigned char *)"MThd", 4);
    put_u32(&bytes, 6);
    put_u16(&bytes, 1);
    put_u16(&bytes, (unsigned)(1 + voice_track_count));
    put_u16(&bytes, division);

    size_t conductor = begin_track(&bytes);
    write_events(&bytes, &conductor_events);
    end_track(&bytes, conductor);
    for (size_t i = 0; i < voice_track_count; i++) {
        size_t track = begin_track(&bytes);
        write_events(&bytes, &voice_tracks[i]);
        end_track(&bytes, track);
    }
    free(conductor_events.items);
    free_tracks(voice_tracks, voice_track_count);

    if (bytes.failed) {
        free(bytes.data);
        return MIDI_RENDER_ERR_ALLOC;
    }
    *out_data = bytes.data;
    *out_size = bytes.size;
    return MIDI_RENDER_OK;
}
