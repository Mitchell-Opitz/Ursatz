#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "model/identity/entity_id.h"
#include "analysis/meter/meter_analysis_input.h"
#include "analysis/notes/analysis_note.h"
#include "model/time/meter_map.h"
#include "model/time/musical_time.h"
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

/* The adapter reads the neutral AnalysisNote: its id, span and voice (the line, 0 when it has none) reach the analyses. */
int main(void) {
    EntityID *id = NULL;
    MeterMap *map = NULL;
    assert(entity_id_create("map", &id) == ENTITY_ID_OK && meter_map_create_defaulted(id, &map) == METER_MAP_OK);
    TimeSpan *span = make_span(2, 3);

    AnalysisNote notes[2] = {{"n-voiced", NULL, span, 64, NULL, NULL, 0, 2, 1.0},
                             {"n-unvoiced", NULL, span, 48, NULL, NULL, ANALYSIS_INDEX_NONE, ANALYSIS_INDEX_NONE, 1.0}};
    StandardMeterInput input;
    assert(standard_meter_input_init(&input, map, notes, 2) == STANDARD_METER_ANALYSIS_OK);
    assert(input.note_count == 2 && input.notes == notes && input.map == map);
    assert(strcmp(input.notes[0].note_id, "n-voiced") == 0);
    assert(input.notes[0].span == span);
    assert(standard_meter_note_line(&input.notes[0]) == 2);
    assert(standard_meter_note_line(&input.notes[1]) == 0);
    standard_meter_input_destroy(&input);

    AnalysisNote no_span = {"n-no-span", NULL, NULL, 60, NULL, NULL, 0, 0, 1.0};
    AnalysisNote empty_id = {"", NULL, span, 60, NULL, NULL, 0, 0, 1.0};
    AnalysisNote no_id = {NULL, NULL, span, 60, NULL, NULL, 0, 0, 1.0};
    assert(standard_meter_input_init(&input, map, &no_span, 1) == STANDARD_METER_ANALYSIS_ERR_INVALID);
    assert(standard_meter_input_init(&input, map, &empty_id, 1) == STANDARD_METER_ANALYSIS_ERR_INVALID);
    assert(standard_meter_input_init(&input, map, &no_id, 1) == STANDARD_METER_ANALYSIS_ERR_INVALID);
    assert(standard_meter_input_init(&input, map, NULL, 1) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_meter_input_init(NULL, map, notes, 2) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_meter_input_init(&input, NULL, notes, 2) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_meter_input_init(&input, map, NULL, 0) == STANDARD_METER_ANALYSIS_OK && input.note_count == 0);
    standard_meter_input_destroy(&input);

    time_span_destroy(span);
    meter_map_destroy(map);
    printf("test_meter_analysis_note: ok\n");
    return 0;
}
