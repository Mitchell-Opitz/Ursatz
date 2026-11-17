#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/harmony/analysis_input.h"
#include "analysis/notes/analysis_key.h"
#include "analysis/notes/analysis_segment.h"
#include "model/identity/entity_id.h"
#include "model/time/meter_map.h"
#include "model/time/metrical_position.h"
#include "model/time/musical_time.h"
#include "model/time/time_span.h"

/* The adapter reads the neutral AnalysisSegment it embeds: its id, span, position and strength reach the prepared segment. */
static void test_segment(void) {
    MusicalTime *from = NULL;
    MusicalTime *to = NULL;
    TimeSpan *span = NULL;
    MetricalPosition *position = NULL;
    assert(musical_time_create(0, 1, &from) == MUSICAL_TIME_OK && musical_time_create(1, 2, &to) == MUSICAL_TIME_OK);
    assert(time_span_create(from, to, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK);
    MeterMap *map = NULL;
    EntityID *map_id = NULL;
    assert(entity_id_create("map", &map_id) == ENTITY_ID_OK && meter_map_create_defaulted(map_id, &map) == METER_MAP_OK);
    assert(metrical_position_from_time(map, from, &position) == METRICAL_POSITION_OK);

    HarmonySegmentInput input;
    memset(&input, 0, sizeof(input));
    input.segment = (AnalysisSegment){"seg-7", span, position, BEAT_STRENGTH_MEDIUM};
    input.degree = 5;
    input.intervals[1] = 4;
    input.intervals[2] = 7;
    input.interval_count = 3;
    input.inversion = 0;
    input.key_ref = "key";
    input.mode = HARMONY_MODE_MAJOR;
    input.uncertainty = 0.25;
    HarmonySegment *prepared = NULL;
    assert(harmony_analysis_prepare(&input, 1, &prepared) == HARMONY_ANALYSIS_OK);
    assert(prepared[0].input == &input && strcmp(prepared[0].input->segment.id, "seg-7") == 0);
    assert(prepared[0].input->segment.span == span && prepared[0].input->segment.position == position);
    assert(prepared[0].input->segment.strength == BEAT_STRENGTH_MEDIUM);
    harmony_analysis_destroy(prepared);

    /* a segment with no id, span or position is bad input */
    HarmonySegmentInput bad = input;
    bad.segment.id = "";
    assert(harmony_analysis_prepare(&bad, 1, &prepared) == HARMONY_ANALYSIS_ERR_BAD_INPUT);
    bad = input;
    bad.segment.span = NULL;
    assert(harmony_analysis_prepare(&bad, 1, &prepared) == HARMONY_ANALYSIS_ERR_BAD_INPUT);
    bad = input;
    bad.segment.position = NULL;
    assert(harmony_analysis_prepare(&bad, 1, &prepared) == HARMONY_ANALYSIS_ERR_BAD_INPUT);

    metrical_position_destroy(position);
    meter_map_destroy(map);
    time_span_destroy(span);
    musical_time_destroy(from);
    musical_time_destroy(to);
}

int main(void) {
    test_segment();
    printf("test_harmony_analysis_key: ok\n");
    return 0;
}
