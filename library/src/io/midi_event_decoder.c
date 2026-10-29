#include "io/midi_event_decoder.h"

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "time/note_value.h"

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

typedef struct {
    uint32_t tick;
    unsigned char nn;
    unsigned char dd;
} RawTimeSignature;

typedef struct {
    RawTimeSignature *items;
    size_t count;
    size_t capacity;
} RawTimeSignatureList;

static int raw_time_signature_push(RawTimeSignatureList *list, RawTimeSignature event) {
    if (list->count == list->capacity) {
        size_t new_capacity = (list->capacity == 0) ? 8 : list->capacity * 2;
        RawTimeSignature *grown = realloc(list->items, sizeof(RawTimeSignature) * new_capacity);
        if (grown == NULL) {
            return -1;
        }
        list->items = grown;
        list->capacity = new_capacity;
    }
    list->items[list->count++] = event;
    return 0;
}

typedef struct {
    uint32_t tick;
    uint32_t microseconds_per_quarter;
} RawTempo;

typedef struct {
    RawTempo *items;
    size_t count;
    size_t capacity;
} RawTempoList;

static int raw_tempo_push(RawTempoList *list, RawTempo event) {
    if (list->count == list->capacity) {
        size_t new_capacity = (list->capacity == 0) ? 8 : list->capacity * 2;
        RawTempo *grown = realloc(list->items, sizeof(RawTempo) * new_capacity);
        if (grown == NULL) {
            return -1;
        }
        list->items = grown;
        list->capacity = new_capacity;
    }
    list->items[list->count++] = event;
    return 0;
}

typedef struct {
    MidiProgramEvent *items;
    size_t count;
    size_t capacity;
} ProgramList;

static int program_list_push(ProgramList *list, MidiProgramEvent event) {
    if (list->count == list->capacity) {
        size_t new_capacity = (list->capacity == 0) ? 4 : list->capacity * 2;
        MidiProgramEvent *grown = realloc(list->items, sizeof(MidiProgramEvent) * new_capacity);
        if (grown == NULL) {
            return -1;
        }
        list->items = grown;
        list->capacity = new_capacity;
    }
    list->items[list->count++] = event;
    return 0;
}

/* Where one walk of a track puts what it finds; every sink is optional (NULL: that kind of event is
   skipped). The caller owns everything the sinks point at, also on failure. */
typedef struct {
    EventList *notes;
    RawTimeSignatureList *time_signatures;
    RawTempoList *tempos;
    ProgramList *programs;
    char **name; /* the first track name (0xFF 0x03) of the track is stored here; *name starts NULL */
    MidiKeySignatureCollector *keys;
    size_t track_index; /* for keys */
} TrackSinks;

/* Walks one track once, handing note events, well-formed time signature meta-events (0xFF 0x58),
   set-tempo meta-events (0xFF 0x51), program changes (0xC0), the first track name and key signature
   meta-events (0xFF 0x59) to whichever sinks are set. */
static MidiEventDecoderResult walk_track(const unsigned char *track_data, size_t track_size,
                                          const TrackSinks *sinks) {
    Cursor c = {track_data, track_size, 0};

    uint32_t tick = 0;
    uint64_t wide_tick = 0; /* the key signature collector's tick, which does not wrap */
    unsigned char running_status = 0;

    while (c.pos < c.size) {
        uint32_t delta = 0;
        if (read_vlq(&c, &delta) != 0) {
            return MIDI_EVENT_DECODER_ERR_TRUNCATED;
        }
        tick += delta;
        wide_tick += delta;
        if (sinks->keys != NULL && wide_tick > (uint64_t)LONG_MAX) {
            return MIDI_EVENT_DECODER_ERR_TRUNCATED;
        }

        if (!cursor_has(&c, 1)) {
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
                return MIDI_EVENT_DECODER_ERR_TRUNCATED;
            }
            unsigned char meta_type = cursor_byte(&c);
            uint32_t len = 0;
            if (read_vlq(&c, &len) != 0 || !cursor_has(&c, len)) {
                return MIDI_EVENT_DECODER_ERR_TRUNCATED;
            }
            if (meta_type == 0x58 && sinks->time_signatures != NULL && len == 4) {
                /* nn dd cc bb: cc and bb are only required to be present. A signature the
                   TimeSignature type cannot hold is malformed and skipped. */
                unsigned char nn = c.data[c.pos];
                unsigned char dd = c.data[c.pos + 1];
                if (nn >= 1 && nn <= TIME_SIGNATURE_MAX_TOP_NUMBER && dd <= TIME_SIGNATURE_MAX_BOTTOM_UNIT_EXPONENT) {
                    RawTimeSignature raw = {tick, nn, dd};
                    if (raw_time_signature_push(sinks->time_signatures, raw) != 0) {
                        return MIDI_EVENT_DECODER_ERR_ALLOC;
                    }
                }
            }
            if (meta_type == 0x51 && sinks->tempos != NULL && len == 3) {
                /* Three bytes, big-endian: microseconds per quarter note. Zero is malformed. */
                uint32_t value = ((uint32_t)c.data[c.pos] << 16) | ((uint32_t)c.data[c.pos + 1] << 8) |
                                 (uint32_t)c.data[c.pos + 2];
                if (value != 0) {
                    RawTempo raw = {tick, value};
                    if (raw_tempo_push(sinks->tempos, raw) != 0) {
                        return MIDI_EVENT_DECODER_ERR_ALLOC;
                    }
                }
            }
            if (meta_type == 0x03 && sinks->name != NULL && *sinks->name == NULL) {
                char *text = malloc((size_t)len + 1);
                if (text == NULL) {
                    return MIDI_EVENT_DECODER_ERR_ALLOC;
                }
                memcpy(text, c.data + c.pos, len);
                text[len] = '\0';
                *sinks->name = text;
            }
            if (sinks->keys != NULL &&
                midi_key_signature_collector_meta(sinks->keys, sinks->track_index, wide_tick, meta_type,
                                                  c.data + c.pos, len) != 0) {
                return MIDI_EVENT_DECODER_ERR_ALLOC;
            }
            c.pos += len;
            continue;
        }

        if (status == 0xF0 || status == 0xF7) {
            /* Sysex / escape: VLQ length + data. */
            uint32_t len = 0;
            if (read_vlq(&c, &len) != 0 || !cursor_has(&c, len)) {
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

            if (sinks->notes != NULL && event_list_push(sinks->notes, event) != 0) {
                return MIDI_EVENT_DECODER_ERR_ALLOC;
            }
        } else if (high_nibble == 0xA0 || high_nibble == 0xB0 || high_nibble == 0xE0) {
            /* Polyphonic aftertouch, CC, pitch bend: 2 data bytes, ignored. */
            if (!cursor_has(&c, 2)) {
                return MIDI_EVENT_DECODER_ERR_TRUNCATED;
            }
            c.pos += 2;
        } else if (high_nibble == 0xC0 || high_nibble == 0xD0) {
            /* Program change (0xC0) is read when a sink wants it; channel aftertouch (0xD0) is ignored.
               Both have 1 data byte. */
            if (!cursor_has(&c, 1)) {
                return MIDI_EVENT_DECODER_ERR_TRUNCATED;
            }
            unsigned char program = cursor_byte(&c);
            if (high_nibble == 0xC0 && sinks->programs != NULL) {
                MidiProgramEvent event = {tick, channel, program};
                if (program_list_push(sinks->programs, event) != 0) {
                    return MIDI_EVENT_DECODER_ERR_ALLOC;
                }
            }
        } else {
            /* Unrecognized status byte with no defined length: cannot
               safely skip, so fail cleanly rather than guess. */
            return MIDI_EVENT_DECODER_ERR_TRUNCATED;
        }
    }

    return MIDI_EVENT_DECODER_OK;
}

MidiEventDecoderResult midi_event_decoder_decode(const unsigned char *track_data, size_t track_size,
                                                  MidiNoteEvent **out_events, size_t *out_count) {
    if (track_data == NULL || out_events == NULL || out_count == NULL) {
        return MIDI_EVENT_DECODER_ERR_NULL_TRACK;
    }

    EventList list = {NULL, 0, 0};
    MidiEventDecoderResult result = walk_track(track_data, track_size, &(TrackSinks){.notes = &list});
    if (result != MIDI_EVENT_DECODER_OK) {
        free(list.items);
        return result;
    }

    *out_events = list.items;
    *out_count = list.count;
    return MIDI_EVENT_DECODER_OK;
}

typedef struct {
    RawTimeSignature raw;
    size_t track_index;
    size_t sequence;
} SequencedTimeSignature;

static int compare_sequenced_time_signatures(const void *a, const void *b) {
    const SequencedTimeSignature *left = (const SequencedTimeSignature *)a;
    const SequencedTimeSignature *right = (const SequencedTimeSignature *)b;
    if (left->raw.tick != right->raw.tick) {
        return left->raw.tick < right->raw.tick ? -1 : 1;
    }
    if (left->track_index != right->track_index) {
        return left->track_index < right->track_index ? -1 : 1;
    }
    if (left->sequence != right->sequence) {
        return left->sequence < right->sequence ? -1 : 1;
    }
    return 0;
}

void midi_time_signature_events_destroy(MidiTimeSignatureEvents *events) {
    if (events == NULL) {
        return;
    }
    for (size_t i = 0; i < events->count; i++) {
        time_signature_destroy(events->events[i].time_signature);
    }
    free(events->events);
    free(events->conflict_ticks);
    events->events = NULL;
    events->count = 0;
    events->conflict_ticks = NULL;
    events->conflict_count = 0;
}

static int build_time_signature(const RawTimeSignature *raw, TimeSignature **out) {
    NoteValue *unit = NULL;
    if (note_value_create((int)raw->dd, &unit) != NOTE_VALUE_OK) {
        return -1;
    }
    TimeSignatureResult r = time_signature_create(raw->nn, unit, out);
    note_value_destroy(unit);
    return r == TIME_SIGNATURE_OK ? 0 : -1;
}

static MidiEventDecoderResult append_time_signatures(SequencedTimeSignature **all, size_t *all_count,
                                                      size_t *all_capacity, size_t track_index,
                                                      const RawTimeSignatureList *raw) {
    if (*all_count + raw->count > *all_capacity) {
        size_t new_capacity = *all_count + raw->count;
        SequencedTimeSignature *grown = realloc(*all, sizeof(SequencedTimeSignature) * new_capacity);
        if (grown == NULL) {
            return MIDI_EVENT_DECODER_ERR_ALLOC;
        }
        *all = grown;
        *all_capacity = new_capacity;
    }
    for (size_t i = 0; i < raw->count; i++) {
        (*all)[*all_count].raw = raw->items[i];
        (*all)[*all_count].track_index = track_index;
        (*all)[*all_count].sequence = *all_count;
        (*all_count)++;
    }
    return MIDI_EVENT_DECODER_OK;
}

/* Merges the collected events into *out (see the header for the rules). Frees all. */
static MidiEventDecoderResult finish_time_signatures(SequencedTimeSignature *all, size_t all_count,
                                                      MidiTimeSignatureEvents *out) {
    MidiEventDecoderResult failure = MIDI_EVENT_DECODER_OK;
    MidiTimeSignatureEvents result = {NULL, 0, NULL, 0};
    if (all_count > 0) {
        qsort(all, all_count, sizeof(SequencedTimeSignature), compare_sequenced_time_signatures);
        result.events = (MidiTimeSignatureEvent *)calloc(all_count, sizeof(MidiTimeSignatureEvent));
        result.conflict_ticks = (uint64_t *)calloc(all_count, sizeof(uint64_t));
        if (result.events == NULL || result.conflict_ticks == NULL) {
            free(result.events);
            free(result.conflict_ticks);
            free(all);
            return MIDI_EVENT_DECODER_ERR_ALLOC;
        }

        /* One event per tick. Exact duplicates merge; if the signatures at a tick differ, the last
           in (track, byte) order wins and the tick is recorded as a conflict. */
        size_t i = 0;
        while (i < all_count && failure == MIDI_EVENT_DECODER_OK) {
            size_t j = i;
            bool conflict = false;
            while (j + 1 < all_count && all[j + 1].raw.tick == all[i].raw.tick) {
                j++;
            }
            const SequencedTimeSignature *winner = &all[j];
            for (size_t k = i; k < j; k++) {
                if (all[k].raw.nn != winner->raw.nn || all[k].raw.dd != winner->raw.dd) {
                    conflict = true;
                }
            }

            MidiTimeSignatureEvent *entry = &result.events[result.count];
            if (build_time_signature(&winner->raw, &entry->time_signature) != 0) {
                failure = MIDI_EVENT_DECODER_ERR_ALLOC;
                break;
            }
            entry->tick = winner->raw.tick;
            entry->track_index = winner->track_index;
            result.count++;
            if (conflict) {
                result.conflict_ticks[result.conflict_count++] = winner->raw.tick;
            }
            i = j + 1;
        }
    }
    free(all);

    if (failure != MIDI_EVENT_DECODER_OK) {
        midi_time_signature_events_destroy(&result);
        return failure;
    }

    if (result.count == 0) {
        free(result.events);
        free(result.conflict_ticks);
        result.events = NULL;
        result.conflict_ticks = NULL;
    }
    *out = result;
    return MIDI_EVENT_DECODER_OK;
}

MidiEventDecoderResult midi_event_decoder_collect_time_signatures(const SmfTrack *tracks, size_t track_count,
                                                                    MidiTimeSignatureEvents *out) {
    if ((tracks == NULL && track_count > 0) || out == NULL) {
        return MIDI_EVENT_DECODER_ERR_NULL_TRACK;
    }

    SequencedTimeSignature *all = NULL;
    size_t all_count = 0;
    size_t all_capacity = 0;
    MidiEventDecoderResult failure = MIDI_EVENT_DECODER_OK;

    for (size_t t = 0; t < track_count && failure == MIDI_EVENT_DECODER_OK; t++) {
        RawTimeSignatureList raw = {NULL, 0, 0};
        failure = walk_track(tracks[t].data, tracks[t].size, &(TrackSinks){.time_signatures = &raw});
        if (failure == MIDI_EVENT_DECODER_OK) {
            failure = append_time_signatures(&all, &all_count, &all_capacity, t, &raw);
        }
        free(raw.items);
    }
    if (failure != MIDI_EVENT_DECODER_OK) {
        free(all);
        return failure;
    }
    return finish_time_signatures(all, all_count, out);
}

typedef struct {
    RawTempo raw;
    size_t track_index;
    size_t sequence;
} SequencedTempo;

static int compare_sequenced_tempos(const void *a, const void *b) {
    const SequencedTempo *left = (const SequencedTempo *)a;
    const SequencedTempo *right = (const SequencedTempo *)b;
    if (left->raw.tick != right->raw.tick) {
        return left->raw.tick < right->raw.tick ? -1 : 1;
    }
    if (left->track_index != right->track_index) {
        return left->track_index < right->track_index ? -1 : 1;
    }
    if (left->sequence != right->sequence) {
        return left->sequence < right->sequence ? -1 : 1;
    }
    return 0;
}

void midi_tempo_events_destroy(MidiTempoEvents *events) {
    if (events == NULL) {
        return;
    }
    free(events->events);
    free(events->conflict_ticks);
    events->events = NULL;
    events->count = 0;
    events->conflict_ticks = NULL;
    events->conflict_count = 0;
}

static MidiEventDecoderResult append_tempos(SequencedTempo **all, size_t *all_count, size_t *all_capacity,
                                             size_t track_index, const RawTempoList *raw) {
    if (*all_count + raw->count > *all_capacity) {
        size_t new_capacity = *all_count + raw->count;
        SequencedTempo *grown = realloc(*all, sizeof(SequencedTempo) * new_capacity);
        if (grown == NULL) {
            return MIDI_EVENT_DECODER_ERR_ALLOC;
        }
        *all = grown;
        *all_capacity = new_capacity;
    }
    for (size_t i = 0; i < raw->count; i++) {
        (*all)[*all_count].raw = raw->items[i];
        (*all)[*all_count].track_index = track_index;
        (*all)[*all_count].sequence = *all_count;
        (*all_count)++;
    }
    return MIDI_EVENT_DECODER_OK;
}

/* Merges the collected events into *out (see the header for the rules). Frees all. */
static MidiEventDecoderResult finish_tempos(SequencedTempo *all, size_t all_count, MidiTempoEvents *out) {
    MidiTempoEvents result = {NULL, 0, NULL, 0};
    if (all_count > 0) {
        qsort(all, all_count, sizeof(SequencedTempo), compare_sequenced_tempos);
        result.events = (MidiTempoEvent *)calloc(all_count, sizeof(MidiTempoEvent));
        result.conflict_ticks = (uint64_t *)calloc(all_count, sizeof(uint64_t));
        if (result.events == NULL || result.conflict_ticks == NULL) {
            free(result.events);
            free(result.conflict_ticks);
            free(all);
            return MIDI_EVENT_DECODER_ERR_ALLOC;
        }

        /* One event per tick. Exact duplicates merge; if the tempos at a tick differ, the last in
           (track, byte) order wins and the tick is recorded as a conflict. */
        size_t i = 0;
        while (i < all_count) {
            size_t j = i;
            while (j + 1 < all_count && all[j + 1].raw.tick == all[i].raw.tick) {
                j++;
            }
            const SequencedTempo *winner = &all[j];
            bool conflict = false;
            for (size_t k = i; k < j; k++) {
                if (all[k].raw.microseconds_per_quarter != winner->raw.microseconds_per_quarter) {
                    conflict = true;
                }
            }

            MidiTempoEvent *entry = &result.events[result.count++];
            entry->microseconds_per_quarter = winner->raw.microseconds_per_quarter;
            entry->tick = winner->raw.tick;
            entry->track_index = winner->track_index;
            if (conflict) {
                result.conflict_ticks[result.conflict_count++] = winner->raw.tick;
            }
            i = j + 1;
        }
    }
    free(all);

    if (result.count == 0) {
        free(result.events);
        free(result.conflict_ticks);
        result.events = NULL;
        result.conflict_ticks = NULL;
    }
    *out = result;
    return MIDI_EVENT_DECODER_OK;
}

MidiEventDecoderResult midi_event_decoder_collect_tempos(const SmfTrack *tracks, size_t track_count,
                                                          MidiTempoEvents *out) {
    if ((tracks == NULL && track_count > 0) || out == NULL) {
        return MIDI_EVENT_DECODER_ERR_NULL_TRACK;
    }

    SequencedTempo *all = NULL;
    size_t all_count = 0;
    size_t all_capacity = 0;
    MidiEventDecoderResult failure = MIDI_EVENT_DECODER_OK;

    for (size_t t = 0; t < track_count && failure == MIDI_EVENT_DECODER_OK; t++) {
        RawTempoList raw = {NULL, 0, 0};
        failure = walk_track(tracks[t].data, tracks[t].size, &(TrackSinks){.tempos = &raw});
        if (failure == MIDI_EVENT_DECODER_OK) {
            failure = append_tempos(&all, &all_count, &all_capacity, t, &raw);
        }
        free(raw.items);
    }
    if (failure != MIDI_EVENT_DECODER_OK) {
        free(all);
        return failure;
    }
    return finish_tempos(all, all_count, out);
}

void midi_file_scan_destroy(MidiFileScan *scan) {
    if (scan == NULL) {
        return;
    }
    for (size_t t = 0; t < scan->track_count; t++) {
        free(scan->tracks[t].notes);
        free(scan->tracks[t].programs);
        free(scan->tracks[t].name);
    }
    free(scan->tracks);
    midi_time_signature_events_destroy(&scan->time_signatures);
    midi_tempo_events_destroy(&scan->tempos);
    memset(scan, 0, sizeof(*scan));
}

MidiEventDecoderResult midi_event_decoder_scan(const SmfTrack *tracks, size_t track_count,
                                                MidiKeySignatureCollector *keys, MidiFileScan *out) {
    if ((tracks == NULL && track_count > 0) || out == NULL) {
        return MIDI_EVENT_DECODER_ERR_NULL_TRACK;
    }

    MidiFileScan result;
    memset(&result, 0, sizeof(result));
    SequencedTimeSignature *signatures = NULL;
    size_t signature_count = 0;
    size_t signature_capacity = 0;
    SequencedTempo *tempos = NULL;
    size_t tempo_count = 0;
    size_t tempo_capacity = 0;
    MidiEventDecoderResult failure = MIDI_EVENT_DECODER_OK;

    if (track_count > 0) {
        result.tracks = calloc(track_count, sizeof(MidiTrackScan));
        if (result.tracks == NULL) {
            return MIDI_EVENT_DECODER_ERR_ALLOC;
        }
    }
    result.track_count = track_count;

    for (size_t t = 0; t < track_count && failure == MIDI_EVENT_DECODER_OK; t++) {
        EventList notes = {NULL, 0, 0};
        RawTimeSignatureList raw_signatures = {NULL, 0, 0};
        RawTempoList raw_tempos = {NULL, 0, 0};
        ProgramList programs = {NULL, 0, 0};
        char *name = NULL;
        TrackSinks sinks = {.notes = &notes,
                            .time_signatures = &raw_signatures,
                            .tempos = &raw_tempos,
                            .programs = &programs,
                            .name = &name,
                            .keys = keys,
                            .track_index = t};
        failure = walk_track(tracks[t].data, tracks[t].size, &sinks);
        if (failure == MIDI_EVENT_DECODER_OK) {
            failure = append_time_signatures(&signatures, &signature_count, &signature_capacity, t, &raw_signatures);
        }
        if (failure == MIDI_EVENT_DECODER_OK) {
            failure = append_tempos(&tempos, &tempo_count, &tempo_capacity, t, &raw_tempos);
        }
        free(raw_signatures.items);
        free(raw_tempos.items);
        result.tracks[t].notes = notes.items;
        result.tracks[t].note_count = notes.count;
        result.tracks[t].programs = programs.items;
        result.tracks[t].program_count = programs.count;
        result.tracks[t].name = name;
    }
    if (failure != MIDI_EVENT_DECODER_OK) {
        free(signatures);
        free(tempos);
        midi_file_scan_destroy(&result);
        return failure;
    }

    failure = finish_time_signatures(signatures, signature_count, &result.time_signatures);
    if (failure != MIDI_EVENT_DECODER_OK) {
        free(tempos);
        midi_file_scan_destroy(&result);
        return failure;
    }
    failure = finish_tempos(tempos, tempo_count, &result.tempos);
    if (failure != MIDI_EVENT_DECODER_OK) {
        midi_file_scan_destroy(&result);
        return failure;
    }

    *out = result;
    return MIDI_EVENT_DECODER_OK;
}
