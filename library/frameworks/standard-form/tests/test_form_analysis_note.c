#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis_input.h"
#include "core/provenance_reference.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "semantics/analysis_note.h"
#include "semantics/analysis_segment.h"
#include "time/duration.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_span.h"

static TimeSpan *make_span(long start, long end) {
    MusicalTime *from = NULL;
    MusicalTime *to = NULL;
    TimeSpan *span = NULL;
    assert(musical_time_create(start, 4, &from) == MUSICAL_TIME_OK && musical_time_create(end, 4, &to) == MUSICAL_TIME_OK);
    assert(time_span_create(from, to, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK);
    musical_time_destroy(from);
    musical_time_destroy(to);
    return span;
}

static NoteEvent *make_event(const char *name, long start) {
    EntityID *id = NULL;
    MusicalTime *position = NULL;
    TemporalAnchor *anchor = NULL;
    Duration *duration = NULL;
    PitchIdentity *pitch = NULL;
    ProvenanceReference *provenance = NULL;
    NoteEvent *event = NULL;
    assert(entity_id_create(name, &id) == ENTITY_ID_OK);
    assert(musical_time_create(start, 4, &position) == MUSICAL_TIME_OK);
    assert(temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK);
    assert(duration_create(1, 4, &duration) == DURATION_OK);
    assert(pitch_identity_create("pitch", 5, &pitch) == PITCH_IDENTITY_OK);
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    assert(note_event_create(id, anchor, duration, pitch, NULL, NULL, provenance, &event) == NOTE_EVENT_OK);
    return event;
}

int main(void) {
    EntityID *id = NULL;
    MeterMap *map = NULL;
    assert(entity_id_create("map", &id) == ENTITY_ID_OK && meter_map_create_defaulted(id, &map) == METER_MAP_OK);
    TimeSpan *first_span = make_span(0, 1);
    TimeSpan *second_span = make_span(1, 2);
    NoteEvent *first_event = make_event("e1", 0);
    NoteEvent *second_event = make_event("e2", 1);
    NoteEvent *bass_event = make_event("e3", 0);
    StandardFormInput input;
    standard_form_input_init(&input, map);

    AnalysisNote soprano = {"s1", first_event, first_span, 72, NULL, NULL, 0, 0, 1.0};
    assert(standard_form_input_add_analysis_note(&input, &soprano, false) == STANDARD_FORM_ANALYSIS_OK);
    assert(input.note_count == 1 && input.bass_count == 0);
    assert(strcmp(input.notes[0].note_id, "s1") == 0);
    assert(input.notes[0].event == first_event);
    assert(input.notes[0].span == first_span);
    assert(input.notes[0].position == NULL);
    assert(input.notes[0].semitone == 72);

    AnalysisNote bass = {"b1", bass_event, first_span, 48, NULL, NULL, 0, 1, 1.0};
    assert(standard_form_input_add_analysis_note(&input, &bass, true) == STANDARD_FORM_ANALYSIS_OK);
    assert(input.note_count == 1 && input.bass_count == 1);
    assert(strcmp(input.bass_notes[0].note_id, "b1") == 0);
    assert(input.bass_notes[0].semitone == 48);
    assert(input.bass_notes[0].position == NULL);

    AnalysisNote next = {"s2", second_event, second_span, 74, NULL, NULL, 0, 0, 1.0};
    assert(standard_form_input_add_analysis_note(&input, &next, false) == STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_input_finish(&input) == STANDARD_FORM_ANALYSIS_OK);
    assert(input.notes[0].has_next && input.notes[0].semitones_to_next == 2);

    AnalysisNote unknown = {"s3", second_event, second_span, ANALYSIS_PITCH_UNKNOWN, NULL, NULL, 0, 0, 1.0};
    assert(standard_form_input_add_analysis_note(&input, &unknown, false) == STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT);
    AnalysisNote no_event = {"s4", NULL, second_span, 60, NULL, NULL, 0, 0, 1.0};
    assert(standard_form_input_add_analysis_note(&input, &no_event, false) == STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT);
    assert(standard_form_input_add_analysis_note(&input, NULL, false) == STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(input.note_count == 2);

    AnalysisSegment segment = {"seg-1", second_span, NULL, BEAT_STRENGTH_WEAK};
    assert(standard_form_input_add_analysis_segment(&input, &segment, 2, 5) == STANDARD_FORM_ANALYSIS_OK);
    assert(input.segment_count == 1);
    assert(strcmp(input.segments[0].id, "seg-1") == 0);
    assert(input.segments[0].span == second_span);
    assert(input.segments[0].function == 2);
    assert(input.segments[0].phrase_model_label == 5);
    AnalysisSegment no_span = {"seg-2", NULL, NULL, BEAT_STRENGTH_WEAK};
    assert(standard_form_input_add_analysis_segment(&input, &no_span, 0, 0) == STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT);
    assert(standard_form_input_add_analysis_segment(&input, NULL, 0, 0) == STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);

    standard_form_input_destroy(&input);
    note_event_destroy(first_event);
    note_event_destroy(second_event);
    note_event_destroy(bass_event);
    time_span_destroy(first_span);
    time_span_destroy(second_span);
    meter_map_destroy(map);
    printf("test_form_analysis_note: ok\n");
    return 0;
}
