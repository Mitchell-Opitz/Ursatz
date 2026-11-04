#include "io/midi_key_signature.h"

#include <limits.h>
#include <stdlib.h>

#include "io/midi_event_decoder.h"
#include "io/smf_reader.h"
#include "pitch/midi_pitch.h"

typedef struct {
    uint64_t tick;
    size_t track_index;
    int8_t sharps_flats;
    uint8_t mode_byte;
} RawKeyEvent;

struct MidiKeySignatureCollector {
    MidiKeySignature first; /* the first well-formed length-2 event in track then byte order, sf unchecked */
    RawKeyEvent *items;
    size_t count;
    size_t capacity;
};

static MidiKeySignatureResult scan_failure(SmfReaderResult smf_result) {
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

/* One walk of every track by midi_event_decoder_scan, feeding a collector. The caller owns the
   collector and the tracks array. */
static MidiEventDecoderResult walk_tracks(const SmfTrack *tracks, size_t track_count,
                                          MidiKeySignatureCollector *collector) {
    MidiFileScan scan;
    MidiEventDecoderResult result = midi_event_decoder_scan(tracks, track_count, collector, &scan);
    if (result == MIDI_EVENT_DECODER_OK) {
        midi_file_scan_destroy(&scan);
    }
    return result;
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
        return scan_failure(smf_result);
    }

    MidiKeySignatureCollector *collector = midi_key_signature_collector_create();
    if (collector == NULL) {
        free(tracks);
        return MIDI_KEY_SIGNATURE_ERR_BAD_TRACK;
    }
    MidiEventDecoderResult walked = walk_tracks(tracks, track_count, collector);
    free(tracks);
    if (walked != MIDI_EVENT_DECODER_OK) {
        midi_key_signature_collector_destroy(collector);
        return MIDI_KEY_SIGNATURE_ERR_BAD_TRACK;
    }
    *out = collector->first;
    midi_key_signature_collector_destroy(collector);
    return MIDI_KEY_SIGNATURE_OK;
}

MidiKeySignatureBuildResult midi_key_signature_build(int sharps_flats, KeySignature **out) {
    if (out == NULL) {
        return MIDI_KEY_SIGNATURE_BUILD_ERR_NULL_OUT;
    }
    if (sharps_flats < -7 || sharps_flats > 7) {
        return MIDI_KEY_SIGNATURE_BUILD_ERR_SF_OUT_OF_RANGE;
    }

    return key_signature_from_fifths(sharps_flats, out) ? MIDI_KEY_SIGNATURE_BUILD_OK : MIDI_KEY_SIGNATURE_BUILD_ERR_ALLOC;
}

MidiKeySignatureCollector *midi_key_signature_collector_create(void) {
    return (MidiKeySignatureCollector *)calloc(1, sizeof(MidiKeySignatureCollector));
}

void midi_key_signature_collector_destroy(MidiKeySignatureCollector *collector) {
    if (collector == NULL) {
        return;
    }
    free(collector->items);
    free(collector);
}

static int collector_append(MidiKeySignatureCollector *collector, RawKeyEvent event) {
    if (collector->count == collector->capacity) {
        size_t capacity = collector->capacity == 0 ? 8 : collector->capacity * 2;
        RawKeyEvent *items = (RawKeyEvent *)realloc(collector->items, capacity * sizeof(RawKeyEvent));
        if (items == NULL) {
            return -1;
        }
        collector->items = items;
        collector->capacity = capacity;
    }
    collector->items[collector->count++] = event;
    return 0;
}

int midi_key_signature_collector_meta(MidiKeySignatureCollector *collector, size_t track_index, uint64_t tick,
                                      unsigned char meta_type, const unsigned char *data, uint32_t length) {
    if (meta_type != 0x59 || length != 2) {
        return 0;
    }
    int8_t sf = (int8_t)data[0];
    if (!collector->first.found) {
        collector->first.found = true;
        collector->first.sharps_flats = sf;
        collector->first.is_minor = data[1] != 0;
    }
    if (sf < -7 || sf > 7) {
        return 0;
    }
    RawKeyEvent event = {tick, track_index, sf, data[1]};
    return collector_append(collector, event);
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

MidiKeySignatureScanAllResult midi_key_signature_collector_finish(MidiKeySignatureCollector *collector,
                                                                   uint16_t division, MidiKeySignatureEvents *out) {
    if (collector == NULL || out == NULL) {
        return MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_NULL_BUFFER;
    }
    if (division == 0) {
        return MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_UNSUPPORTED_DIVISION;
    }

    MidiKeySignatureScanAllResult failure = MIDI_KEY_SIGNATURE_SCAN_ALL_OK;
    MidiKeySignatureEvents result = {NULL, 0};
    if (collector->count > 0) {
        SequencedRawEvent *sorted = (SequencedRawEvent *)malloc(collector->count * sizeof(SequencedRawEvent));
        result.events = (MidiKeySignatureEvent *)calloc(collector->count, sizeof(MidiKeySignatureEvent));
        if (sorted == NULL || result.events == NULL) {
            free(sorted);
            free(result.events);
            return MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_ALLOC;
        }
        for (size_t i = 0; i < collector->count; i++) {
            sorted[i].event = collector->items[i];
            sorted[i].sequence = i;
        }
        qsort(sorted, collector->count, sizeof(SequencedRawEvent), compare_sequenced);

        unsigned long denominator = 4UL * (unsigned long)division;
        for (size_t i = 0; i < collector->count && failure == MIDI_KEY_SIGNATURE_SCAN_ALL_OK; i++) {
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

    MidiKeySignatureCollector *collector = midi_key_signature_collector_create();
    if (collector == NULL) {
        free(tracks);
        return MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_ALLOC;
    }
    MidiEventDecoderResult walked = walk_tracks(tracks, track_count, collector);
    free(tracks);
    MidiKeySignatureScanAllResult failure = MIDI_KEY_SIGNATURE_SCAN_ALL_OK;
    if (walked == MIDI_EVENT_DECODER_ERR_ALLOC) {
        failure = MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_ALLOC;
    } else if (walked != MIDI_EVENT_DECODER_OK) {
        failure = MIDI_KEY_SIGNATURE_SCAN_ALL_ERR_BAD_TRACK;
    } else {
        failure = midi_key_signature_collector_finish(collector, header.division, out);
    }
    midi_key_signature_collector_destroy(collector);
    return failure;
}
