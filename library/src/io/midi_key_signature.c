#include "io/midi_key_signature.h"

#include <limits.h>
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

static const PitchLetter SHARP_ORDER[7] = {PITCH_LETTER_F, PITCH_LETTER_C, PITCH_LETTER_G, PITCH_LETTER_D,
                                           PITCH_LETTER_A, PITCH_LETTER_E, PITCH_LETTER_B};
static const PitchLetter FLAT_ORDER[7] = {PITCH_LETTER_B, PITCH_LETTER_E, PITCH_LETTER_A, PITCH_LETTER_D,
                                          PITCH_LETTER_G, PITCH_LETTER_C, PITCH_LETTER_F};

MidiKeySignatureBuildResult midi_key_signature_build(int sharps_flats, KeySignature **out) {
    if (out == NULL) {
        return MIDI_KEY_SIGNATURE_BUILD_ERR_NULL_OUT;
    }
    if (sharps_flats < -7 || sharps_flats > 7) {
        return MIDI_KEY_SIGNATURE_BUILD_ERR_SF_OUT_OF_RANGE;
    }

    size_t count = (size_t)(sharps_flats < 0 ? -sharps_flats : sharps_flats);
    const PitchLetter *order = sharps_flats < 0 ? FLAT_ORDER : SHARP_ORDER;
    int accidental = sharps_flats < 0 ? -1 : 1;

    KeySignatureEntry entries[7];
    for (size_t i = 0; i < count; i++) {
        entries[i].letter = order[i];
        entries[i].accidental = accidental;
    }

    return key_signature_create(entries, count, out) == KEY_SIGNATURE_OK ? MIDI_KEY_SIGNATURE_BUILD_OK
                                                                          : MIDI_KEY_SIGNATURE_BUILD_ERR_ALLOC;
}

typedef struct {
    uint64_t tick;
    size_t track_index;
    int8_t sharps_flats;
    uint8_t mode_byte;
} RawKeyEvent;

typedef struct {
    RawKeyEvent *items;
    size_t count;
    size_t capacity;
} RawKeyEventList;

static int raw_list_append(RawKeyEventList *list, RawKeyEvent event) {
    if (list->count == list->capacity) {
        size_t capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        RawKeyEvent *items = (RawKeyEvent *)realloc(list->items, capacity * sizeof(RawKeyEvent));
        if (items == NULL) {
            return -1;
        }
        list->items = items;
        list->capacity = capacity;
    }
    list->items[list->count++] = event;
    return 0;
}

/* Same walk as scan_track_for_key_signature, but accumulates ticks and records every well-formed
   key signature event instead of stopping at the first. A malformed 0xFF 0x59 (length other than
   2, sf outside -7..7) is skipped. Returns 0 on success, -1 on malformed track bytes (or a tick
   that does not fit a MusicalTime numerator), -2 on allocation failure. */
static int scan_track_for_all_key_signatures(const unsigned char *track_data, size_t track_size, size_t track_index,
                                              RawKeyEventList *list) {
    Cursor c = {track_data, track_size, 0};
    unsigned char running_status = 0;
    uint64_t tick = 0;

    while (c.pos < c.size) {
        uint32_t delta = 0;
        if (read_vlq(&c, &delta) != 0) {
            return -1;
        }
        tick += delta;
        if (tick > (uint64_t)LONG_MAX) {
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
            if (meta_type == 0x59 && len == 2) {
                int8_t sf = (int8_t)c.data[c.pos];
                if (sf >= -7 && sf <= 7) {
                    RawKeyEvent event = {tick, track_index, sf, c.data[c.pos + 1]};
                    if (raw_list_append(list, event) != 0) {
                        return -2;
                    }
                }
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

/* Tracks are appended in order and each track in byte order, so the array index is the final
   tie-break and keeps same-tick, same-track events in byte order. */
static int compare_raw_events(const void *a, const void *b) {
    const RawKeyEvent *left = (const RawKeyEvent *)a;
    const RawKeyEvent *right = (const RawKeyEvent *)b;
    if (left->tick != right->tick) {
        return left->tick < right->tick ? -1 : 1;
    }
    if (left->track_index != right->track_index) {
        return left->track_index < right->track_index ? -1 : 1;
    }
    return 0;
}

typedef struct {
    RawKeyEvent event;
    size_t sequence;
} SequencedRawEvent;

static int compare_sequenced(const void *a, const void *b) {
    const SequencedRawEvent *left = (const SequencedRawEvent *)a;
    const SequencedRawEvent *right = (const SequencedRawEvent *)b;
    int order = compare_raw_events(&left->event, &right->event);
    if (order != 0) {
        return order;
    }
    if (left->sequence != right->sequence) {
        return left->sequence < right->sequence ? -1 : 1;
    }
    return 0;
}

static MidiModeHint mode_hint_from_byte(uint8_t mode_byte) {
    if (mode_byte == 0) {
        return MIDI_MODE_HINT_MAJOR;
    }
    if (mode_byte == 1) {
        return MIDI_MODE_HINT_MINOR;
    }
    return MIDI_MODE_HINT_UNKNOWN;
}

void midi_key_signature_events_destroy(MidiKeySignatureEvents *events) {
    if (events == NULL) {
        return;
    }
    for (size_t i = 0; i < events->count; i++) {
        key_signature_destroy(events->events[i].key_signature);
        musical_time_destroy(events->events[i].position);
    }
    free(events->events);
    events->events = NULL;
    events->count = 0;
}

static MidiKeySignatureScanAllResult map_smf_failure(SmfReaderResult smf_result) {
    switch (smf_result) {
    case SMF_READER_ERR_NULL_BUFFER:
        return MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_NULL_BUFFER;
    case SMF_READER_ERR_TRUNCATED:
        return MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_TRUNCATED;
    case SMF_READER_ERR_UNSUPPORTED_FORMAT:
        return MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_UNSUPPORTED_FORMAT;
    case SMF_READER_ERR_UNSUPPORTED_DIVISION:
        return MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_UNSUPPORTED_DIVISION;
    case SMF_READER_ERR_BAD_TRACK:
        return MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_BAD_TRACK;
    case SMF_READER_ERR_BAD_HEADER:
    default:
        return MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_BAD_HEADER;
    }
}

MidiKeySignatureScanAllResult midi_key_signature_scan_all(const unsigned char *data, size_t size,
                                                           MidiKeySignatureEvents *out) {
    if (data == NULL || out == NULL) {
        return MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_NULL_BUFFER;
    }

    SmfHeader header;
    SmfTrack *tracks = NULL;
    size_t track_count = 0;
    SmfReaderResult smf_result = smf_reader_parse(data, size, &header, &tracks, &track_count);
    if (smf_result != SMF_READER_OK) {
        return map_smf_failure(smf_result);
    }
    if (header.division == 0) {
        free(tracks);
        return MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_UNSUPPORTED_DIVISION;
    }

    RawKeyEventList raw = {NULL, 0, 0};
    MidiKeySignatureScanAllResult failure = MIDI_KEY_SIGNATURE_SCAN_ALL_OK;
    for (size_t t = 0; t < track_count; t++) {
        int track_result = scan_track_for_all_key_signatures(tracks[t].data, tracks[t].size, t, &raw);
        if (track_result != 0) {
            failure = track_result == -2 ? MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_ALLOC
                                         : MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_BAD_TRACK;
            break;
        }
    }
    free(tracks);
    if (failure != MIDI_KEY_SIGNATURE_SCAN_ALL_OK) {
        free(raw.items);
        return failure;
    }

    MidiKeySignatureEvents result = {NULL, 0};
    if (raw.count > 0) {
        SequencedRawEvent *sorted = (SequencedRawEvent *)malloc(raw.count * sizeof(SequencedRawEvent));
        result.events = (MidiKeySignatureEvent *)calloc(raw.count, sizeof(MidiKeySignatureEvent));
        if (sorted == NULL || result.events == NULL) {
            free(sorted);
            free(result.events);
            free(raw.items);
            return MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_ALLOC;
        }
        for (size_t i = 0; i < raw.count; i++) {
            sorted[i].event = raw.items[i];
            sorted[i].sequence = i;
        }
        qsort(sorted, raw.count, sizeof(SequencedRawEvent), compare_sequenced);

        unsigned long denominator = 4UL * (unsigned long)header.division;
        for (size_t i = 0; i < raw.count && failure == MIDI_KEY_SIGNATURE_SCAN_ALL_OK; i++) {
            const RawKeyEvent *event = &sorted[i].event;

            bool duplicate = false;
            for (size_t k = result.count; k > 0; k--) {
                const MidiKeySignatureEvent *kept = &result.events[k - 1];
                if (kept->tick != event->tick) {
                    break;
                }
                if (kept->sharps_flats == event->sharps_flats && kept->mode_byte == event->mode_byte) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                continue;
            }

            MidiKeySignatureEvent *entry = &result.events[result.count];
            if (midi_key_signature_build(event->sharps_flats, &entry->key_signature) !=
                MIDI_KEY_SIGNATURE_BUILD_OK) {
                failure = MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_ALLOC;
                break;
            }
            if (musical_time_create((long)event->tick, denominator, &entry->position) != MUSICAL_TIME_OK) {
                key_signature_destroy(entry->key_signature);
                entry->key_signature = NULL;
                failure = MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_ALLOC;
                break;
            }
            entry->mode_hint = mode_hint_from_byte(event->mode_byte);
            entry->tick = event->tick;
            entry->track_index = event->track_index;
            entry->sharps_flats = event->sharps_flats;
            entry->mode_byte = event->mode_byte;
            result.count++;
        }
        free(sorted);
    }
    free(raw.items);

    if (failure != MIDI_KEY_SIGNATURE_SCAN_ALL_OK) {
        midi_key_signature_events_destroy(&result);
        return failure;
    }

    if (result.count == 0) {
        free(result.events);
        result.events = NULL;
    }
    *out = result;
    return MIDI_KEY_SIGNATURE_SCAN_ALL_OK;
}
