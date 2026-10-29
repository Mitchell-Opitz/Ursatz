#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "identity/entity_id.h"
#include "semantics/analysis_note.h"
#include "texture_analysis_input.h"
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
    TimeSpan *span = make_span(1, 3);
    StandardTextureInput input;
    assert(standard_texture_input_init(&input, map) == STANDARD_TEXTURE_ANALYSIS_OK);

    AnalysisNote note = {"n1", NULL, span, 67, NULL, NULL, 1, 3, 0.5};
    assert(standard_texture_input_add_analysis_note(&input, &note) == STANDARD_TEXTURE_ANALYSIS_OK);
    assert(input.note_count == 1);
    assert(strcmp(input.notes[0].id, "n1") == 0);
    assert(input.notes[0].span == span);
    assert(input.notes[0].pitch == 67);
    assert(input.notes[0].track == 1);
    assert(input.notes[0].voice == 3);
    assert(input.notes[0].voice_confidence == 0.5);

    AnalysisNote unknown = {"n2", NULL, span, ANALYSIS_PITCH_UNKNOWN, NULL, NULL, 0, 0, 1.0};
    assert(standard_texture_input_add_analysis_note(&input, &unknown) == STANDARD_TEXTURE_ANALYSIS_ERR_INVALID);
    AnalysisNote bad_confidence = {"n3", NULL, span, 60, NULL, NULL, 0, 0, 2.0};
    assert(standard_texture_input_add_analysis_note(&input, &bad_confidence) == STANDARD_TEXTURE_ANALYSIS_ERR_INVALID);
    assert(standard_texture_input_add_analysis_note(&input, NULL) == STANDARD_TEXTURE_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_texture_input_add_analysis_note(NULL, &note) == STANDARD_TEXTURE_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(input.note_count == 1);

    standard_texture_input_destroy(&input);
    time_span_destroy(span);
    meter_map_destroy(map);
    printf("test_texture_analysis_note: ok\n");
    return 0;
}
