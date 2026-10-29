#ifndef URSATZ_STANDARD_FORM_TESTS_FORM_TEST_UTIL_H
#define URSATZ_STANDARD_FORM_TESTS_FORM_TEST_UTIL_H

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis_input.h"
#include "core/provenance_reference.h"
#include "events/note_event.h"
#include "ursatz_chain/form_bridge.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "time/duration.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/note_value.h"
#include "time/temporal_anchor.h"
#include "time/time_signature.h"

/*
 * A form-analysis input built by hand: a MeterMap of `top`/4 (with a pickup of `pickup_quarters` quarter notes when it is
 * not 0), a soprano of notes in quarter-note steps, and cadences. Times are in quarter notes (a whole note is 4 quarters), so a
 * measure of 4/4 is 4. Measure m (1-based) starts at pickup + (m - 1) * top quarters.
 */

#define FORM_MAX_NOTES 160
#define FORM_MAX_CADENCES 16

typedef struct {
    MeterMap *map;
    int top;
    int pickup;
    StandardFormInput input;
    NoteEvent *events[FORM_MAX_NOTES];
    TimeSpan *note_spans[FORM_MAX_NOTES];
    size_t note_count;
    TimeSpan *cadence_spans[FORM_MAX_CADENCES];
    size_t cadence_count;
} FormFixture;

static inline EntityID *form_id(const char *text) {
    EntityID *id = NULL;
    assert(entity_id_create(text, &id) == ENTITY_ID_OK);
    return id;
}

static inline TimeSpan *form_span(long start_sixteenths, long end_sixteenths) {
    MusicalTime *start = NULL;
    MusicalTime *end = NULL;
    TimeSpan *span = NULL;
    assert(musical_time_create(start_sixteenths, 16, &start) == MUSICAL_TIME_OK);
    assert(musical_time_create(end_sixteenths, 16, &end) == MUSICAL_TIME_OK);
    assert(time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK);
    musical_time_destroy(start);
    musical_time_destroy(end);
    return span;
}

static inline void form_init(FormFixture *f, int top, int pickup_quarters) {
    memset(f, 0, sizeof(*f));
    f->top = top;
    f->pickup = pickup_quarters;
    NoteValue *quarter = NULL;
    assert(note_value_create(2, &quarter) == NOTE_VALUE_OK);
    TimeSignature *signature = NULL;
    assert(time_signature_create((unsigned)top, quarter, &signature) == TIME_SIGNATURE_OK);
    note_value_destroy(quarter);
    MusicalTime *zero = NULL;
    MusicalTime *after_pickup = NULL;
    assert(musical_time_create(0, 1, &zero) == MUSICAL_TIME_OK);
    assert(musical_time_create(pickup_quarters, 4, &after_pickup) == MUSICAL_TIME_OK);
    MeterMapChangePoint points[2] = {{signature, zero}, {signature, after_pickup}};
    assert(meter_map_create(form_id("form-test-meter"), points, pickup_quarters > 0 ? 2 : 1, &f->map) == METER_MAP_OK);
    time_signature_destroy(signature);
    musical_time_destroy(zero);
    musical_time_destroy(after_pickup);
    assert(meter_map_has_pickup(f->map) == (pickup_quarters > 0));
    standard_form_input_init(&f->input, f->map);
}

/* A soprano note of `duration` quarters starting at `start` quarters from the piece's start. */
static inline void form_note(FormFixture *f, double start_quarters, double duration_quarters, int midi) {
    assert(f->note_count < FORM_MAX_NOTES);
    long start = (long)(start_quarters * 4.0 + 0.5);
    long length = (long)(duration_quarters * 4.0 + 0.5);
    char id[32];
    snprintf(id, sizeof(id), "n%zu", f->note_count);
    MusicalTime *position = NULL;
    assert(musical_time_create(start, 16, &position) == MUSICAL_TIME_OK);
    TemporalAnchor *anchor = NULL;
    assert(temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK);
    Duration *duration = NULL;
    assert(duration_create(length, 16, &duration) == DURATION_OK);
    PitchIdentity *pitch = NULL;
    assert(pitch_identity_create("pitch", 5, &pitch) == PITCH_IDENTITY_OK);
    ProvenanceReference *provenance = NULL;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    NoteEvent **event = &f->events[f->note_count];
    assert(note_event_create(form_id(id), anchor, duration, pitch, NULL, NULL, provenance, event) == NOTE_EVENT_OK);
    f->note_spans[f->note_count] = form_span(start, start + length);
    assert(standard_form_input_add_note(&f->input, id, *event, f->note_spans[f->note_count], NULL, midi) ==
           STANDARD_FORM_ANALYSIS_OK);
    f->note_count++;
}

/* The scale degrees of the soprano notes added so far, in order (0 for a note with none); alterations 0. */
static inline void form_degrees(FormFixture *f, const int *degrees) {
    for (size_t i = 0; i < f->note_count; i++) {
        assert(standard_form_input_set_degree(&f->input, false, i, degrees[i], 0) == STANDARD_FORM_ANALYSIS_OK);
    }
}

/* The first quarter of measure m (1-based; 0 is the pickup). */
static inline double form_measure_start(const FormFixture *f, int m) {
    return m == 0 ? 0.0 : (double)f->pickup + (double)(m - 1) * (double)f->top;
}

/* A measure of quarter notes: one note per beat with the given pitches (0 for a beat that rests). */
static inline void form_measure(FormFixture *f, int m, const int *pitches) {
    for (int beat = 0; beat < f->top; beat++) {
        if (pitches[beat] != 0) {
            form_note(f, form_measure_start(f, m) + (double)beat, 1.0, pitches[beat]);
        }
    }
}

/* The same pitches in every measure from first to last. */
static inline void form_measures(FormFixture *f, int first, int last, const int *pitches) {
    for (int m = first; m <= last; m++) {
        form_measure(f, m, pitches);
    }
}

/* A cadence whose arrival chord starts `beat` quarters into measure m and lasts `quarters`. */
static inline void form_cadence(FormFixture *f, const char *id, int m, double beat, double quarters, CadenceType type,
                                int rank, bool post_cadential, double uncertainty) {
    assert(f->cadence_count < FORM_MAX_CADENCES);
    long start = (long)((form_measure_start(f, m) + beat) * 4.0 + 0.5);
    long length = (long)(quarters * 4.0 + 0.5);
    f->cadence_spans[f->cadence_count] = form_span(start, start + length);
    assert(standard_form_input_add_cadence(&f->input, id, f->cadence_spans[f->cadence_count], type, rank, post_cadential,
                                           uncertainty) == STANDARD_FORM_ANALYSIS_OK);
    f->cadence_count++;
}

static inline void form_finish(FormFixture *f) {
    assert(standard_form_input_finish(&f->input) == STANDARD_FORM_ANALYSIS_OK);
}

static inline void form_destroy(FormFixture *f) {
    standard_form_input_destroy(&f->input);
    for (size_t i = 0; i < f->note_count; i++) {
        note_event_destroy(f->events[i]);
        time_span_destroy(f->note_spans[i]);
    }
    for (size_t i = 0; i < f->cadence_count; i++) {
        time_span_destroy(f->cadence_spans[i]);
    }
    meter_map_destroy(f->map);
}

#endif
