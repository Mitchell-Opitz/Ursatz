#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "model/identity/entity_id.h"
#include "analysis/notes/analysis_note.h"
#include "analysis/texture/texture_analysis_features.h"
#include "model/time/meter_map.h"
#include "model/time/musical_time.h"
#include "model/time/time_span.h"

static TimeSpan make_span(long start, long end) {
    MusicalTime from;
    MusicalTime to;
    TimeSpan span;
    assert(musical_time_make(start, 4, &from) == MUSICAL_TIME_OK && musical_time_make(end, 4, &to) == MUSICAL_TIME_OK);
    assert(time_span_make(from, to, &span) == TIME_SPAN_OK);
    return span;
}

/* The adapter reads the neutral AnalysisNote: its span, midi, track, voice and voice confidence reach the windows. */
int main(void) {
    EntityID *id = NULL;
    MeterMap *map = NULL;
    assert(entity_id_create("map", &id) == ENTITY_ID_OK && meter_map_create_defaulted(id, &map) == METER_MAP_OK);
    TimeSpan span = make_span(1, 3);

    /* two voices of one track: a note's track and voice make its voice */
    AnalysisNote notes[2] = {{"n1", NULL, span, 67, NULL, NULL, 1, 3, 0.5}, {"n2", NULL, span, 60, NULL, NULL, 1, 4, 1.0}};
    AnalysisNoteSet set = {notes, 2, map};
    StandardTextureWindows windows = {NULL, 0};
    assert(standard_texture_measure_windows(&set, NULL, &windows) == STANDARD_TEXTURE_ANALYSIS_OK);
    assert(windows.count == 1 && windows.items[0].events == 2 && windows.items[0].voices_max == 2);
    standard_texture_windows_destroy(&windows);

    /* what a note must hold is checked where the windows are measured */
    AnalysisNote unknown = {"n3", NULL, span, ANALYSIS_PITCH_UNKNOWN, NULL, NULL, 0, 0, 1.0};
    AnalysisNote bad_confidence = {"n4", NULL, span, 60, NULL, NULL, 0, 0, 2.0};
    AnalysisNote no_id = {NULL, NULL, span, 60, NULL, NULL, 0, 0, 1.0};
    AnalysisNote *refused[3] = {&unknown, &bad_confidence, &no_id};
    for (size_t i = 0; i < 3; i++) {
        AnalysisNoteSet one = {refused[i], 1, map};
        assert(standard_texture_measure_windows(&one, NULL, &windows) == STANDARD_TEXTURE_ANALYSIS_ERR_INVALID);
    }
    assert(standard_texture_measure_windows(NULL, NULL, &windows) == STANDARD_TEXTURE_ANALYSIS_ERR_NULL_ARGUMENT);
    AnalysisNoteSet no_map = {notes, 2, NULL};
    assert(standard_texture_measure_windows(&no_map, NULL, &windows) == STANDARD_TEXTURE_ANALYSIS_ERR_NULL_ARGUMENT);
    AnalysisNoteSet empty = {NULL, 0, map};
    assert(standard_texture_measure_windows(&empty, NULL, &windows) == STANDARD_TEXTURE_ANALYSIS_OK && windows.count == 0);

    meter_map_destroy(map);
    printf("test_texture_analysis_note: ok\n");
    return 0;
}
