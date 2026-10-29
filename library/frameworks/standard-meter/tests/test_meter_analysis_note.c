#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "identity/entity_id.h"
#include "meter_analysis_input.h"
#include "semantics/analysis_note.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
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

int main(void) {
    EntityID *id = NULL;
    MeterMap *map = NULL;
    assert(entity_id_create("map", &id) == ENTITY_ID_OK && meter_map_create_defaulted(id, &map) == METER_MAP_OK);
    TimeSpan *span = make_span(2, 3);
    StandardMeterInput input;
    assert(standard_meter_input_init(&input, map) == STANDARD_METER_ANALYSIS_OK);

    AnalysisNote voiced = {"n-voiced", NULL, span, 64, NULL, NULL, 0, 2, 1.0};
    assert(standard_meter_input_add_analysis_note(&input, &voiced) == STANDARD_METER_ANALYSIS_OK);
    assert(input.note_count == 1);
    assert(strcmp(input.notes[0].id, "n-voiced") == 0);
    assert(input.notes[0].span == span);
    assert(input.notes[0].pitch == 64);
    assert(input.notes[0].line == 2);

    AnalysisNote unvoiced = {"n-unvoiced", NULL, span, 48, NULL, NULL, ANALYSIS_INDEX_NONE, ANALYSIS_INDEX_NONE, 1.0};
    assert(standard_meter_input_add_analysis_note(&input, &unvoiced) == STANDARD_METER_ANALYSIS_OK);
    assert(input.note_count == 2);
    assert(input.notes[1].pitch == 48);
    assert(input.notes[1].line == 0);

    AnalysisNote unknown = {"n-unknown", NULL, span, ANALYSIS_PITCH_UNKNOWN, NULL, NULL, 0, 0, 1.0};
    assert(standard_meter_input_add_analysis_note(&input, &unknown) == STANDARD_METER_ANALYSIS_ERR_INVALID);
    AnalysisNote no_span = {"n-no-span", NULL, NULL, 60, NULL, NULL, 0, 0, 1.0};
    assert(standard_meter_input_add_analysis_note(&input, &no_span) == STANDARD_METER_ANALYSIS_ERR_INVALID);
    assert(standard_meter_input_add_analysis_note(&input, NULL) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_meter_input_add_analysis_note(NULL, &voiced) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(input.note_count == 2);

    standard_meter_input_destroy(&input);
    time_span_destroy(span);
    meter_map_destroy(map);
    printf("test_meter_analysis_note: ok\n");
    return 0;
}
