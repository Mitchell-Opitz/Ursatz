#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/form/analysis_input.h"
#include "analysis/form/analysis_input_internal.h"
#include "model/core/provenance_reference.h"
#include "model/events/note_event.h"
#include "model/identity/entity_id.h"
#include "model/pitch/pitch_identity.h"
#include "analysis/notes/analysis_note.h"
#include "analysis/notes/analysis_segment.h"
#include "model/time/duration.h"
#include "model/time/meter_map.h"
#include "model/time/musical_time.h"
#include "model/time/temporal_anchor.h"
#include "model/time/time_span.h"

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
    MusicalTime position;
    Duration duration;
    PitchIdentity *pitch = NULL;
    ProvenanceReference *provenance = NULL;
    NoteEvent *event = NULL;
    assert(entity_id_create(name, &id) == ENTITY_ID_OK);
    assert(musical_time_make(start, 4, &position) == MUSICAL_TIME_OK);
    assert(duration_make(1, 4, &duration) == DURATION_OK);
    assert(pitch_identity_create("pitch", 5, &pitch) == PITCH_IDENTITY_OK);
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    assert(note_event_create(id, position, duration, pitch, NULL, NULL, provenance, &event) == NOTE_EVENT_OK);
    return event;
}

int main(void) {
    EntityID *id = NULL;
    MeterMap *map = NULL;
    assert(entity_id_create("map", &id) == ENTITY_ID_OK && meter_map_create_defaulted(id, &map) == METER_MAP_OK);
    TimeSpan *first_span = make_span(0, 1);
    TimeSpan *second_span = make_span(1, 2);
    TimeSpan *later_span = make_span(5, 6);
    NoteEvent *first_event = make_event("e1", 0);
    NoteEvent *second_event = make_event("e2", 1);
    NoteEvent *bass_event = make_event("e3", 0);

    /* the adapter reads the neutral AnalysisNote: id, event, span and midi reach the input unchanged */
    AnalysisNote soprano[2] = {{"s1", first_event, first_span, 72, NULL, NULL, 0, 0, 1.0},
                               {"s2", second_event, second_span, 74, NULL, NULL, 0, 0, 1.0}};
    AnalysisNote bass[1] = {{"b1", bass_event, first_span, 48, NULL, NULL, 0, 1, 1.0}};
    StandardFormInput input;
    standard_form_input_init(&input, map, soprano, 2);
    input.bass_notes = bass;
    input.bass_count = 1;
    int degrees[2] = {3, 0};
    input.degrees = degrees;
    assert(input.map == map && input.note_count == 2 && input.notes == soprano && input.bass_count == 1);
    assert(standard_form_input_finish(&input) == STANDARD_FORM_ANALYSIS_OK && input.finished);
    assert(strcmp(input.notes[0].note_id, "s1") == 0 && input.notes[0].event == first_event && input.notes[0].span == first_span);
    assert(input.notes[0].midi == 72 && input.bass_notes[0].midi == 48);
    /* the pitch change to the next note, where no rest lies between them */
    long step = 0;
    assert(standard_form_next_note_step(input.notes, input.note_count, 0, &step) && step == 2);
    assert(!standard_form_next_note_step(input.notes, input.note_count, 1, &step));
    standard_form_input_destroy(&input);

    /* what a note must hold is checked when the input is finished */
    AnalysisNote unknown = {"s3", second_event, second_span, ANALYSIS_PITCH_UNKNOWN, NULL, NULL, 0, 0, 1.0};
    AnalysisNote no_event = {"s4", NULL, second_span, 60, NULL, NULL, 0, 0, 1.0};
    AnalysisNote empty_id = {"", second_event, second_span, 60, NULL, NULL, 0, 0, 1.0};
    AnalysisNote no_span = {"s5", second_event, NULL, 60, NULL, NULL, 0, 0, 1.0};
    AnalysisNote *refused[4] = {&unknown, &no_event, &empty_id, &no_span};
    for (size_t i = 0; i < 4; i++) {
        standard_form_input_init(&input, map, refused[i], 1);
        assert(standard_form_input_finish(&input) == STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT);
        standard_form_input_destroy(&input);
    }
    /* starts that go backwards are refused, a rest between notes is not a gap in the line */
    AnalysisNote backwards[2] = {{"s6", second_event, second_span, 60, NULL, NULL, 0, 0, 1.0},
                                 {"s7", first_event, first_span, 62, NULL, NULL, 0, 0, 1.0}};
    standard_form_input_init(&input, map, backwards, 2);
    assert(standard_form_input_finish(&input) == STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT);
    standard_form_input_destroy(&input);
    AnalysisNote with_rest[2] = {{"s8", first_event, first_span, 60, NULL, NULL, 0, 0, 1.0},
                                 {"s9", second_event, later_span, 67, NULL, NULL, 0, 0, 1.0}};
    standard_form_input_init(&input, map, with_rest, 2);
    assert(standard_form_input_finish(&input) == STANDARD_FORM_ANALYSIS_OK);
    assert(!standard_form_next_note_step(input.notes, input.note_count, 0, &step));
    standard_form_input_destroy(&input);
    int bad_degree[1] = {9};
    standard_form_input_init(&input, map, soprano, 1);
    input.degrees = bad_degree;
    assert(standard_form_input_finish(&input) == STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT);
    standard_form_input_destroy(&input);
    assert(standard_form_input_finish(NULL) == STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT);

    note_event_destroy(first_event);
    note_event_destroy(second_event);
    note_event_destroy(bass_event);
    time_span_destroy(first_span);
    time_span_destroy(second_span);
    time_span_destroy(later_span);
    meter_map_destroy(map);
    printf("test_form_analysis_note: ok\n");
    return 0;
}
