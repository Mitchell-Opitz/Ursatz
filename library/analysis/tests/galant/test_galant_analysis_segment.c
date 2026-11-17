#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/galant/galant_analysis_input.h"
#include "analysis/notes/analysis_key.h"
#include "analysis/notes/analysis_segment.h"
#include "model/time/musical_time.h"
#include "model/time/time_span.h"

/* The adapter reads the neutral AnalysisSegment it embeds: the list takes its id, span, position and strength, and refuses what it cannot read. */
static void test_segment(void) {
    MusicalTime from;
    MusicalTime to;
    TimeSpan span;
    assert(musical_time_make(0, 1, &from) == MUSICAL_TIME_OK && musical_time_make(1, 2, &to) == MUSICAL_TIME_OK);
    assert(time_span_make(from, to, &span) == TIME_SPAN_OK);

    GalantSegment galant;
    memset(&galant, 0, sizeof(galant));
    galant.beat_numerator = 1;
    galant.beat_denominator = 4;
    galant.uncertainty = 0.5;
    galant.key.degree = 1;
    galant.segment = (AnalysisSegment){"seg-3", span, NULL, BEAT_STRENGTH_STRONG};
    GalantSegmentList list;
    galant_segments_init(&list);
    assert(galant_segments_add(&list, &galant) == GALANT_ANALYSIS_OK && list.count == 1);
    assert(strcmp(list.items[0].segment.id, "seg-3") == 0 && musical_time_same(list.items[0].segment.span.start, span.start) && musical_time_same(list.items[0].segment.span.end, span.end));
    assert(list.items[0].segment.position == NULL && list.items[0].segment.strength == BEAT_STRENGTH_STRONG);
    assert(list.items[0].beat_numerator == 1 && list.items[0].beat_denominator == 4 && list.items[0].uncertainty == 0.5);

    GalantSegment bad = galant;
    bad.segment.id = NULL;
    assert(galant_segments_add(&list, &bad) == GALANT_ANALYSIS_ERR_INVALID);
    bad = galant;
    bad.segment.id = "";
    assert(galant_segments_add(&list, &bad) == GALANT_ANALYSIS_ERR_INVALID);
    bad = galant;
    memset(&bad.segment.span, 0, sizeof(bad.segment.span)); /* an empty span */
    assert(galant_segments_add(&list, &bad) == GALANT_ANALYSIS_ERR_INVALID);
    char long_id[GALANT_ANALYSIS_ID_MAX + 8];
    memset(long_id, 'a', sizeof(long_id));
    long_id[sizeof(long_id) - 1] = '\0';
    bad = galant;
    bad.segment.id = long_id;
    assert(galant_segments_add(&list, &bad) == GALANT_ANALYSIS_ERR_INVALID);
    assert(galant_segments_add(&list, NULL) == GALANT_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(list.count == 1);
    galant_segments_destroy(&list);

}

int main(void) {
    test_segment();
    printf("test_galant_analysis_segment: ok\n");
    return 0;
}
