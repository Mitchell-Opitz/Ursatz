#ifndef URSATZ_TESTS_IO_MIDI_RENDER_TEST_UTIL_H
#define URSATZ_TESTS_IO_MIDI_RENDER_TEST_UTIL_H

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "core/voice_reference.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "io/midi_importer.h"
#include "midi_import_split.h"
#include "io/midi_key_signature.h"
#include "io/midi_renderer.h"
#include "io/smf_reader.h"
#include "pitch/pitch_numeric_value.h"
#include "pitch/tuning_system.h"
#include "time/duration.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/tempo_map.h"
#include "time/temporal_anchor.h"
#include "time/time_signature.h"

/*
 * Helpers for the MIDI renderer tests: notes built from whole-note fractions and MIDI note numbers, maps built from
 * plain values, and a round trip that renders, re-imports through the existing importer and compares everything the
 * renderer is meant to preserve.
 */

typedef struct {
    NoteEvent **events;
    PitchNumericValue **pitches;
    size_t count;
    size_t capacity;
    TuningSystem *tuning;
} TestNotes;

static inline void test_notes_init(TestNotes *notes) {
    memset(notes, 0, sizeof(*notes));
    EntityID *tuning_id = NULL;
    assert(entity_id_create("midi-note-number", &tuning_id) == ENTITY_ID_OK);
    assert(tuning_system_create(tuning_id, "MIDI note number", &notes->tuning) == TUNING_SYSTEM_OK);
}

/* A note at position / duration (whole-note fractions), pitch as a position numerator/denominator, in a voice (NULL: none). */
static inline void test_notes_add_exact(TestNotes *notes, long position_numerator, unsigned long position_denominator,
                                        long duration_numerator, unsigned long duration_denominator,
                                        long pitch_numerator, unsigned long pitch_denominator, const char *voice) {
    if (notes->count == notes->capacity) {
        notes->capacity = notes->capacity == 0 ? 16 : notes->capacity * 2;
        notes->events = (NoteEvent **)realloc(notes->events, notes->capacity * sizeof(NoteEvent *));
        notes->pitches = (PitchNumericValue **)realloc(notes->pitches, notes->capacity * sizeof(PitchNumericValue *));
        assert(notes->events != NULL && notes->pitches != NULL);
    }
    char id_text[64];
    snprintf(id_text, sizeof(id_text), "test-note-%zu", notes->count);
    EntityID *id = NULL;
    assert(entity_id_create(id_text, &id) == ENTITY_ID_OK);
    MusicalTime *position = NULL;
    assert(musical_time_create(position_numerator, position_denominator, &position) == MUSICAL_TIME_OK);
    TemporalAnchor *anchor = NULL;
    assert(temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK);
    Duration *duration = NULL;
    assert(duration_create(duration_numerator, duration_denominator, &duration) == DURATION_OK);
    PitchNumericValue *numeric = NULL;
    assert(pitch_numeric_value_create(notes->tuning, pitch_numerator, pitch_denominator, &numeric) ==
           PITCH_NUMERIC_VALUE_OK);
    PitchIdentity *pitch = NULL;
    assert(pitch_numeric_value_to_identity(numeric, &pitch) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK);
    VoiceReference *voice_reference = NULL;
    if (voice != NULL) {
        EntityID *voice_id = NULL;
        assert(entity_id_create(voice, &voice_id) == ENTITY_ID_OK);
        assert(voice_reference_create(voice_id, &voice_reference) == VOICE_REFERENCE_OK);
    }
    ProvenanceReference *provenance = NULL;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    NoteEvent *event = NULL;
    assert(note_event_create(id, anchor, duration, pitch, NULL, voice_reference, provenance, &event) == NOTE_EVENT_OK);
    notes->events[notes->count] = event;
    notes->pitches[notes->count] = numeric;
    notes->count++;
}

/* The usual note: a MIDI note number from 0 to 127. */
static inline void test_notes_add(TestNotes *notes, long position_numerator, unsigned long position_denominator,
                                  long duration_numerator, unsigned long duration_denominator, long midi_note,
                                  const char *voice) {
    test_notes_add_exact(notes, position_numerator, position_denominator, duration_numerator, duration_denominator,
                         midi_note, 1, voice);
}

static inline void test_notes_free(TestNotes *notes) {
    for (size_t i = 0; i < notes->count; i++) {
        note_event_destroy(notes->events[i]);
        pitch_numeric_value_destroy(notes->pitches[i]);
    }
    free(notes->events);
    free(notes->pitches);
    tuning_system_destroy(notes->tuning);
    memset(notes, 0, sizeof(*notes));
}

static inline TimeSignature *test_time_signature(unsigned top, int bottom_number) {
    int exponent = 0;
    while ((1 << exponent) < bottom_number) {
        exponent++;
    }
    NoteValue *unit = NULL;
    assert(note_value_create(exponent, &unit) == NOTE_VALUE_OK);
    TimeSignature *signature = NULL;
    assert(time_signature_create(top, unit, &signature) == TIME_SIGNATURE_OK);
    note_value_destroy(unit);
    return signature;
}

static inline EntityID *test_id(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static inline MusicalTime *test_time(long numerator, unsigned long denominator) {
    MusicalTime *time = NULL;
    assert(musical_time_create(numerator, denominator, &time) == MUSICAL_TIME_OK);
    return time;
}

/* A MeterMap from (signature, start) pairs given as plain values. */
typedef struct {
    unsigned top;
    int bottom;
    long start_numerator;
    unsigned long start_denominator;
} TestMeterChange;

static inline MeterMap *test_meter_map(const TestMeterChange *changes, size_t count) {
    TimeSignature *signatures[16];
    MusicalTime *starts[16];
    MeterMapChangePoint points[16];
    assert(count <= 16);
    for (size_t i = 0; i < count; i++) {
        signatures[i] = test_time_signature(changes[i].top, changes[i].bottom);
        starts[i] = test_time(changes[i].start_numerator, changes[i].start_denominator);
        points[i].time_signature = signatures[i];
        points[i].start = starts[i];
    }
    MeterMap *map = NULL;
    assert(meter_map_create(test_id("test-meter-map"), points, count, &map) == METER_MAP_OK);
    for (size_t i = 0; i < count; i++) {
        time_signature_destroy(signatures[i]);
        musical_time_destroy(starts[i]);
    }
    return map;
}

typedef struct {
    long start_numerator;
    unsigned long start_denominator;
    unsigned long microseconds_per_quarter;
} TestTempoChange;

static inline TempoMap *test_tempo_map(const TestTempoChange *changes, size_t count) {
    MusicalTime *starts[16];
    TempoMapChangePoint points[16];
    assert(count <= 16);
    for (size_t i = 0; i < count; i++) {
        starts[i] = test_time(changes[i].start_numerator, changes[i].start_denominator);
        points[i].start = starts[i];
        points[i].microseconds_per_quarter = changes[i].microseconds_per_quarter;
    }
    TempoMap *map = NULL;
    assert(tempo_map_create(test_id("test-tempo-map"), points, count, &map) == TEMPO_MAP_OK);
    for (size_t i = 0; i < count; i++) {
        musical_time_destroy(starts[i]);
    }
    return map;
}

/* --- reading what was written --- */

typedef struct {
    SmfHeader header;
    SmfTrack *tracks;
    size_t track_count;
} TestSmf;

static inline void test_smf_parse(const unsigned char *data, size_t size, TestSmf *smf) {
    assert(smf_reader_parse(data, size, &smf->header, &smf->tracks, &smf->track_count) == SMF_READER_OK);
}

/* Reads a variable-length quantity at *position, advancing it. */
static inline uint32_t test_read_vlq(const SmfTrack *track, size_t *position) {
    uint32_t value = 0;
    for (;;) {
        assert(*position < track->size);
        unsigned char byte = track->data[(*position)++];
        value = (value << 7) | (byte & 0x7F);
        if ((byte & 0x80) == 0) {
            return value;
        }
    }
}

/* How many note-on events a track holds, and the channel of the first (or -1). */
static inline size_t test_track_note_ons(const SmfTrack *track, int *first_channel) {
    size_t position = 0;
    size_t note_ons = 0;
    *first_channel = -1;
    while (position < track->size) {
        test_read_vlq(track, &position);
        unsigned char status = track->data[position++];
        if (status == 0xFF) {
            position++;
            uint32_t length = test_read_vlq(track, &position);
            position += length;
        } else {
            assert((status & 0x80) != 0); /* running status is never used */
            if ((status & 0xF0) == 0x90) {
                if (*first_channel < 0) {
                    *first_channel = status & 0x0F;
                }
                note_ons++;
            }
            position += 2;
        }
    }
    return note_ons;
}

/* --- comparing a re-import with the original --- */

typedef struct {
    long position_numerator;
    unsigned long position_denominator;
    long duration_numerator;
    unsigned long duration_denominator;
    long pitch;
} TestNoteFact;

static inline int test_compare_facts(const void *left, const void *right) {
    const TestNoteFact *a = (const TestNoteFact *)left;
    const TestNoteFact *b = (const TestNoteFact *)right;
    /* position, then pitch, then duration, by cross-multiplication (the values are small) */
    long long position_a = (long long)a->position_numerator * (long long)b->position_denominator;
    long long position_b = (long long)b->position_numerator * (long long)a->position_denominator;
    if (position_a != position_b) {
        return position_a < position_b ? -1 : 1;
    }
    if (a->pitch != b->pitch) {
        return a->pitch < b->pitch ? -1 : 1;
    }
    long long duration_a = (long long)a->duration_numerator * (long long)b->duration_denominator;
    long long duration_b = (long long)b->duration_numerator * (long long)a->duration_denominator;
    if (duration_a != duration_b) {
        return duration_a < duration_b ? -1 : 1;
    }
    return 0;
}

static inline TestNoteFact *test_facts_of(NoteEvent *const *events, PitchNumericValue *const *pitches, size_t count) {
    TestNoteFact *facts = (TestNoteFact *)calloc(count == 0 ? 1 : count, sizeof(TestNoteFact));
    assert(facts != NULL);
    for (size_t i = 0; i < count; i++) {
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(events[i]));
        const Duration *duration = note_event_duration(events[i]);
        facts[i].position_numerator = musical_time_numerator(position);
        facts[i].position_denominator = musical_time_denominator(position);
        facts[i].duration_numerator = duration_numerator(duration);
        facts[i].duration_denominator = duration_denominator(duration);
        facts[i].pitch = pitch_numeric_value_position_numerator(pitches[i]);
    }
    qsort(facts, count, sizeof(TestNoteFact), test_compare_facts);
    return facts;
}

static inline int test_sizes_compare(const void *left, const void *right) {
    size_t a = *(const size_t *)left;
    size_t b = *(const size_t *)right;
    return a < b ? -1 : (a > b ? 1 : 0);
}

/* The sizes of the voice groups of the notes (notes with no voice are one group), sorted. */
static inline size_t test_group_sizes(NoteEvent *const *events, size_t count, size_t **out_sizes) {
    const char **ids = (const char **)calloc(count == 0 ? 1 : count, sizeof(char *));
    size_t *sizes = (size_t *)calloc(count == 0 ? 1 : count, sizeof(size_t));
    int *has_voice = (int *)calloc(count == 0 ? 1 : count, sizeof(int));
    assert(ids != NULL && sizes != NULL && has_voice != NULL);
    size_t groups = 0;
    for (size_t i = 0; i < count; i++) {
        const VoiceReference *voice = note_event_optional_voice(events[i]);
        const char *id = voice == NULL ? NULL : entity_id_value(voice_reference_target_id(voice));
        size_t g = 0;
        while (g < groups && !((ids[g] == NULL && id == NULL) || (ids[g] != NULL && id != NULL && strcmp(ids[g], id) == 0))) {
            g++;
        }
        if (g == groups) {
            ids[g] = id;
            has_voice[g] = id != NULL;
            groups++;
        }
        sizes[g]++;
    }
    qsort(sizes, groups, sizeof(size_t), test_sizes_compare);
    free(ids);
    free(has_voice);
    *out_sizes = sizes;
    return groups;
}

static inline void test_assert_same_meter(const MeterMap *a, const MeterMap *b) {
    assert(meter_map_region_count(a) == meter_map_region_count(b));
    assert(meter_map_has_pickup(a) == meter_map_has_pickup(b));
    assert(meter_map_first_measure_number(a) == meter_map_first_measure_number(b));
    for (size_t i = 0; i < meter_map_region_count(a); i++) {
        const MeterMapRegion *x = meter_map_region(a, i);
        const MeterMapRegion *y = meter_map_region(b, i);
        assert(time_signature_equals(meter_map_region_time_signature(x), meter_map_region_time_signature(y)));
        assert(musical_time_equals(meter_map_region_start(x), meter_map_region_start(y)));
        assert((meter_map_region_end(x) == NULL) == (meter_map_region_end(y) == NULL));
        if (meter_map_region_end(x) != NULL) {
            assert(musical_time_equals(meter_map_region_end(x), meter_map_region_end(y)));
        }
        assert(meter_map_region_first_measure_number(x) == meter_map_region_first_measure_number(y));
        assert(meter_map_region_is_pickup(x) == meter_map_region_is_pickup(y));
        /* meter_map_region_is_defaulted is not compared: a defaulted region is written as a declared one. */
    }
}

static inline void test_assert_same_tempo(const TempoMap *a, const TempoMap *b) {
    assert(tempo_map_region_count(a) == tempo_map_region_count(b));
    for (size_t i = 0; i < tempo_map_region_count(a); i++) {
        const TempoMapRegion *x = tempo_map_region(a, i);
        const TempoMapRegion *y = tempo_map_region(b, i);
        assert(tempo_map_region_microseconds_per_quarter(x) == tempo_map_region_microseconds_per_quarter(y));
        assert(musical_time_equals(tempo_map_region_start(x), tempo_map_region_start(y)));
        assert((tempo_map_region_end(x) == NULL) == (tempo_map_region_end(y) == NULL));
        if (tempo_map_region_end(x) != NULL) {
            assert(musical_time_equals(tempo_map_region_end(x), tempo_map_region_end(y)));
        }
    }
}

/*
 * Renders, re-imports through the existing importer and compares: note count, and every note's position, pitch and
 * duration (as sorted multisets: ids and tie order are not preserved); the voice grouping, as the partition of notes
 * into tracks, read from the written file; the MeterMap and TempoMap regions; and the key signatures. keys may be NULL.
 */
static inline void test_round_trip(NoteEvent *const *events, PitchNumericValue *const *pitches, size_t count,
                                   const MeterMap *meter_map, const TempoMap *tempo_map,
                                   const MidiKeySignatureEvent *keys, size_t key_count, unsigned division) {
    unsigned char *data = NULL;
    size_t size = 0;
    assert(midi_renderer_render(events, pitches, count, meter_map, tempo_map, keys, key_count, division, &data, &size) ==
           MIDI_RENDER_OK);

    /* The file: format 1, this division, the conductor and one track per voice group. */
    TestSmf smf;
    test_smf_parse(data, size, &smf);
    assert(smf.header.format == 1 && smf.header.division == division);
    size_t *sizes = NULL;
    size_t groups = test_group_sizes(events, count, &sizes);
    assert(smf.track_count == 1 + groups);
    size_t *track_sizes = (size_t *)calloc(groups == 0 ? 1 : groups, sizeof(size_t));
    assert(track_sizes != NULL);
    int channel;
    assert(test_track_note_ons(&smf.tracks[0], &channel) == 0); /* the conductor holds no notes */
    for (size_t t = 0; t < groups; t++) {
        track_sizes[t] = test_track_note_ons(&smf.tracks[1 + t], &channel);
    }
    qsort(track_sizes, groups, sizeof(size_t), test_sizes_compare);
    assert(memcmp(track_sizes, sizes, groups * sizeof(size_t)) == 0);
    free(track_sizes);
    free(sizes);
    free(smf.tracks);

    /* The notes, the meter and the tempo, read back by the importer. */
    NoteEvent **back_events = NULL;
    PitchNumericValue **back_pitches = NULL;
    size_t back_count = 0;
    MidiMeterImport meter = {NULL, NULL, NULL, 0};
    assert(test_midi_import_parts(data, size, &back_events, &back_pitches, &back_count, &meter, NULL) == MIDI_IMPORT_OK);
    assert(back_count == count);
    TestNoteFact *original_facts = test_facts_of(events, pitches, count);
    TestNoteFact *back_facts = test_facts_of(back_events, back_pitches, back_count);
    for (size_t i = 0; i < count; i++) {
        assert(test_compare_facts(&original_facts[i], &back_facts[i]) == 0);
    }
    free(original_facts);
    free(back_facts);
    for (size_t i = 0; i < back_count; i++) {
        note_event_destroy(back_events[i]);
        pitch_numeric_value_destroy(back_pitches[i]);
    }
    free(back_events);
    free(back_pitches);
    if (meter_map != NULL) {
        test_assert_same_meter(meter_map, meter.meter_map);
    } else {
        /* No meter written: the importer's default, one defaulted 4/4 region. */
        assert(meter_map_is_defaulted(meter.meter_map) && meter_map_region_count(meter.meter_map) == 1);
    }
    midi_meter_import_destroy(&meter);

    back_events = NULL;
    back_pitches = NULL;
    MidiTempoImport tempo = {NULL, NULL, 0};
    assert(test_midi_import_parts(data, size, &back_events, &back_pitches, &back_count, NULL, &tempo) == MIDI_IMPORT_OK);
    for (size_t i = 0; i < back_count; i++) {
        note_event_destroy(back_events[i]);
        pitch_numeric_value_destroy(back_pitches[i]);
    }
    free(back_events);
    free(back_pitches);
    if (tempo_map != NULL) {
        test_assert_same_tempo(tempo_map, tempo.tempo_map);
    } else {
        assert(tempo_map_is_defaulted(tempo.tempo_map) && tempo_map_region_count(tempo.tempo_map) == 1);
    }
    midi_tempo_import_destroy(&tempo);

    MidiKeySignatureEvents back_keys = {NULL, 0};
    assert(midi_key_signature_scan_all(data, size, &back_keys) == MIDI_KEY_SIGNATURE_SCAN_ALL_OK);
    assert(back_keys.count == key_count);
    for (size_t i = 0; i < key_count; i++) {
        assert(musical_time_equals(back_keys.events[i].position, keys[i].position));
        assert(back_keys.events[i].sharps_flats == keys[i].sharps_flats);
        assert(back_keys.events[i].mode_byte == keys[i].mode_byte);
    }
    midi_key_signature_events_destroy(&back_keys);
    free(data);
}

#endif
