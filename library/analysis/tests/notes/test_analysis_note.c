#include "analysis/notes/analysis_note.h"
#include "analysis/notes/analysis_segment.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "model/identity/entity_id.h"
#include "model/time/meter_map.h"
#include "model/time/musical_time.h"
#include "model/time/time_span.h"

static TimeSpan make_span(long start_num, long end_num, unsigned long den) {
    MusicalTime start;
    MusicalTime end;
    assert(musical_time_make(start_num, den, &start) == MUSICAL_TIME_OK);
    assert(musical_time_make(end_num, den, &end) == MUSICAL_TIME_OK);
    TimeSpan span;
    assert(time_span_make(start, end, &span) == TIME_SPAN_OK);
    return span;
}

static AnalysisNote note_of(const char *id, TimeSpan span, int midi) {
    AnalysisNote note = {id, NULL, span, midi, NULL, NULL, ANALYSIS_INDEX_NONE, ANALYSIS_INDEX_NONE, 1.0};
    return note;
}

static void test_note_valid(void) {
    TimeSpan span = make_span(0, 1, 4);
    TimeSpan empty = make_span(1, 1, 4);
    AnalysisNote ok = note_of("n1", span, 60);
    assert(analysis_note_valid(&ok));

    AnalysisNote unknown = note_of("n1", span, ANALYSIS_PITCH_UNKNOWN);
    assert(analysis_note_valid(&unknown));
    AnalysisNote low = note_of("n1", span, -2);
    assert(!analysis_note_valid(&low));
    AnalysisNote high = note_of("n1", span, 128);
    assert(!analysis_note_valid(&high));
    AnalysisNote edge = note_of("n1", span, 127);
    assert(analysis_note_valid(&edge));

    AnalysisNote no_id = note_of(NULL, span, 60);
    assert(!analysis_note_valid(&no_id));
    AnalysisNote no_span = note_of("n1", (TimeSpan){0}, 60);
    assert(!analysis_note_valid(&no_span));
    AnalysisNote zero_length = note_of("n1", empty, 60);
    assert(!analysis_note_valid(&zero_length));

    AnalysisNote over = note_of("n1", span, 60);
    over.voice_confidence = 1.5;
    assert(!analysis_note_valid(&over));
    AnalysisNote under = note_of("n1", span, 60);
    under.voice_confidence = -0.1;
    assert(!analysis_note_valid(&under));
    assert(!analysis_note_valid(NULL));

}

static void test_note_compare(void) {
    TimeSpan early = make_span(0, 1, 4);
    TimeSpan late = make_span(1, 2, 4);
    AnalysisNote a = note_of("a", early, 60);
    AnalysisNote b = note_of("b", late, 50);
    assert(analysis_note_compare(&a, &b) < 0);
    assert(analysis_note_compare(&b, &a) > 0);

    AnalysisNote low = note_of("z", early, 55);
    AnalysisNote high = note_of("a", early, 64);
    assert(analysis_note_compare(&low, &high) < 0);

    AnalysisNote first = note_of("a", early, 60);
    AnalysisNote second = note_of("b", early, 60);
    assert(analysis_note_compare(&first, &second) < 0);
    assert(analysis_note_compare(&first, &first) == 0);

    AnalysisNote items[4] = {b, second, a, first};
    qsort(items, 4, sizeof(AnalysisNote), analysis_note_compare);
    assert(items[0].note_id[0] == 'a' && items[1].note_id[0] == 'a' && items[2].note_id[0] == 'b' &&
           items[3].note_id[0] == 'b');

}

static void test_note_set_valid(void) {
    TimeSpan s0 = make_span(0, 1, 4);
    TimeSpan s1 = make_span(1, 2, 4);
    EntityID *id = NULL;
    MeterMap *map = NULL;
    assert(entity_id_create("map", &id) == ENTITY_ID_OK);
    assert(meter_map_create_defaulted(id, &map) == METER_MAP_OK);

    AnalysisNote sorted[2] = {note_of("a", s0, 60), note_of("b", s1, 62)};
    AnalysisNoteSet set = {sorted, 2, map};
    assert(analysis_note_set_valid(&set));

    AnalysisNote unsorted[2] = {note_of("b", s1, 62), note_of("a", s0, 60)};
    AnalysisNoteSet bad_order = {unsorted, 2, map};
    assert(!analysis_note_set_valid(&bad_order));

    AnalysisNote invalid[2] = {note_of("a", s0, 60), note_of(NULL, s1, 62)};
    AnalysisNoteSet bad_note = {invalid, 2, map};
    assert(!analysis_note_set_valid(&bad_note));

    AnalysisNoteSet no_map = {sorted, 2, NULL};
    assert(!analysis_note_set_valid(&no_map));
    AnalysisNoteSet empty = {NULL, 0, map};
    assert(analysis_note_set_valid(&empty));
    AnalysisNoteSet no_notes = {NULL, 2, map};
    assert(!analysis_note_set_valid(&no_notes));
    assert(!analysis_note_set_valid(NULL));

    meter_map_destroy(map);
}

int main(void) {
    test_note_valid();
    test_note_compare();
    test_note_set_valid();
    printf("test_analysis_note: ok\n");
    return 0;
}
