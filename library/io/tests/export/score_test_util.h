#ifndef URSATZ_TESTS_EXPORT_SCORE_TEST_UTIL_H
#define URSATZ_TESTS_EXPORT_SCORE_TEST_UTIL_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/core/provenance_reference.h"
#include "model/core/voice_reference.h"
#include "model/events/dynamics.h"
#include "model/events/note_event.h"
#include "model/events/tempo_marking.h"
#include "io/musicxml_export.h"
#include "model/identity/entity_id.h"
#include "model/pitch/pitch_identity.h"
#include "model/pitch/pitch_spelling.h"
#include "model/time/duration.h"
#include "model/time/meter_map.h"
#include "model/time/metronome_marking.h"
#include "model/time/musical_time.h"
#include "model/time/note_value.h"
#include "model/time/temporal_anchor.h"
#include "model/time/tempo_map.h"
#include "model/time/time_signature.h"
#include "model/time/time_span.h"

/*
 * Helpers for the score export tests: notes built from whole-note fractions and spellings, maps built from plain
 * values, and substring checks on the written document.
 */

typedef struct {
    NoteEvent **events;
    size_t *part;
    int *staff;
    size_t count;
    size_t capacity;
} TestBag;

static inline EntityID *tu_id(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static inline MusicalTime tu_time_value(long long numerator, long long denominator) {
    MusicalTime time;
    assert(musical_time_make(numerator, denominator, &time) == MUSICAL_TIME_OK);
    return time;
}

/* A note at position / duration (whole-note fractions), spelled letter + accidental + octave, in a voice (NULL: none). */
static inline NoteEvent *tu_note(const char *id, long pos_n, unsigned long pos_d, long dur_n, unsigned long dur_d,
                                 PitchLetter letter, int accidental, int octave, const char *voice) {
    Duration duration;
    assert(duration_make(dur_n, (long long)dur_d, &duration) == DURATION_OK);
    PitchIdentity *pitch = NULL;
    unsigned char token[1] = {(unsigned char)(60 + octave)};
    assert(pitch_identity_create(token, sizeof(token), &pitch) == PITCH_IDENTITY_OK);
    PitchSpelling *spelling = NULL;
    assert(pitch_spelling_create(letter, accidental, octave, &spelling) == PITCH_SPELLING_OK);
    VoiceReference *voice_reference = NULL;
    if (voice != NULL) {
        assert(voice_reference_create(tu_id(voice), &voice_reference) == VOICE_REFERENCE_OK);
    }
    ProvenanceReference *provenance = NULL;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    NoteEvent *event = NULL;
    assert(note_event_create(tu_id(id), tu_time_value(pos_n, (long long)pos_d), duration, pitch, spelling, voice_reference, provenance, &event) ==
           NOTE_EVENT_OK);
    return event;
}

static inline void bag_add(TestBag *bag, NoteEvent *event, size_t part, int staff) {
    if (bag->count == bag->capacity) {
        bag->capacity = bag->capacity == 0 ? 16 : bag->capacity * 2;
        bag->events = (NoteEvent **)realloc(bag->events, bag->capacity * sizeof(NoteEvent *));
        bag->part = (size_t *)realloc(bag->part, bag->capacity * sizeof(size_t));
        bag->staff = (int *)realloc(bag->staff, bag->capacity * sizeof(int));
        assert(bag->events != NULL && bag->part != NULL && bag->staff != NULL);
    }
    bag->events[bag->count] = event;
    bag->part[bag->count] = part;
    bag->staff[bag->count] = staff;
    bag->count++;
}

/* The common note: C4 (or the given step) in part 0, staff 1. */
static inline void bag_add_note(TestBag *bag, const char *id, long pos_n, unsigned long pos_d, long dur_n,
                                unsigned long dur_d, PitchLetter letter, const char *voice) {
    bag_add(bag, tu_note(id, pos_n, pos_d, dur_n, dur_d, letter, 0, 4, voice), 0, 1);
}

/* A note with a notated tuplet: actual notes in the time of normal of the same written value. */
static inline void bag_add_tuplet_note(TestBag *bag, const char *id, long pos_n, unsigned long pos_d, long dur_n,
                                       unsigned long dur_d, PitchLetter letter, unsigned actual, unsigned normal) {
    NoteEvent *event = tu_note(id, pos_n, pos_d, dur_n, dur_d, letter, 0, 4, NULL);
    assert(note_event_set_tuplet(event, actual, normal) == NOTE_EVENT_OK);
    bag_add(bag, event, 0, 1);
}

static inline void bag_free(TestBag *bag) {
    for (size_t i = 0; i < bag->count; i++) {
        note_event_destroy(bag->events[i]);
    }
    free(bag->events);
    free(bag->part);
    free(bag->staff);
    memset(bag, 0, sizeof(*bag));
}

/* A MeterMap from (top, bottom number, start) triples. */
typedef struct {
    unsigned top;
    int bottom;
    long start_n;
    unsigned long start_d;
} TuMeter;

static inline MeterMap *tu_meter_map(const TuMeter *changes, size_t count) {
    TimeSignature *signatures[8];
    MusicalTime *starts[8];
    MeterMapChangePoint points[8];
    assert(count <= 8);
    for (size_t i = 0; i < count; i++) {
        int exponent = 0;
        while ((1 << exponent) < changes[i].bottom) {
            exponent++;
        }
        NoteValue *unit = NULL;
        assert(note_value_create(exponent, &unit) == NOTE_VALUE_OK);
        assert(time_signature_create(changes[i].top, unit, &signatures[i]) == TIME_SIGNATURE_OK);
        note_value_destroy(unit);
        points[i].time_signature = signatures[i];
        points[i].start = tu_time_value(changes[i].start_n, (long long)changes[i].start_d);
    }
    MeterMap *map = NULL;
    assert(meter_map_create(tu_id("tu-meter-map"), points, count, &map) == METER_MAP_OK);
    for (size_t i = 0; i < count; i++) {
        time_signature_destroy(signatures[i]);
    }
    return map;
}

typedef struct {
    long start_n;
    unsigned long start_d;
    unsigned long microseconds;
} TuTempo;

static inline TempoMap *tu_tempo_map(const TuTempo *changes, size_t count) {
    TempoMapChangePoint points[8];
    assert(count <= 8);
    for (size_t i = 0; i < count; i++) {
        points[i].start = tu_time_value(changes[i].start_n, (long long)changes[i].start_d);
        points[i].microseconds_per_quarter = changes[i].microseconds;
        points[i].kind = TEMPO_MAP_CHANGE_ABSOLUTE;
    }
    TempoMap *map = NULL;
    assert(tempo_map_create(tu_id("tu-tempo-map"), points, count, &map) == TEMPO_MAP_OK);
    return map;
}

/* A TempoMarking at a position: text (or NULL) and a metronome beat unit n/d and bpm (or beat_n 0 for none). */
static inline TempoMarking *tu_tempo_marking(long pos_n, unsigned long pos_d, const char *text, unsigned long beat_n,
                                             unsigned long beat_d, unsigned long bpm_n, unsigned long bpm_d) {
    TemporalAnchor *anchor = NULL;
    assert(temporal_anchor_create(tu_time_value(pos_n, (long long)pos_d), &anchor) == TEMPORAL_ANCHOR_OK);
    MetronomeMarking *metronome = NULL;
    if (beat_n != 0) {
        Duration beat;
        assert(duration_make((long long)beat_n, (long long)beat_d, &beat) == DURATION_OK);
        assert(metronome_marking_create(&beat, bpm_n, bpm_d, &metronome) == METRONOME_MARKING_OK);
    }
    TempoMarking *marking = NULL;
    assert(tempo_marking_create(anchor, text, metronome, &marking) == TEMPO_MARKING_OK);
    return marking;
}

/* A Dynamics over the span [start, end) of whole notes. */
static inline Dynamics *tu_dynamics(const char *marking, long start_n, unsigned long start_d, long end_n,
                                    unsigned long end_d) {
    TimeSpan span;
    assert(time_span_make(tu_time_value(start_n, (long long)start_d), tu_time_value(end_n, (long long)end_d), &span) ==
           TIME_SPAN_OK);
    Dynamics *dynamics = NULL;
    assert(dynamics_create_for_time_span(marking, span, &dynamics) == DYNAMICS_OK);
    return dynamics;
}

/* --- reading the document --- */

static inline size_t tu_count(const char *xml, const char *needle) {
    size_t n = 0;
    for (const char *at = strstr(xml, needle); at != NULL; at = strstr(at + strlen(needle), needle)) {
        n++;
    }
    return n;
}

#define TU_HAS(xml, needle) (strstr((xml), (needle)) != NULL)

#endif
