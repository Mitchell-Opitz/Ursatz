#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "galant_analysis_input.h"
#include "semantics/analysis_key.h"
#include "semantics/analysis_segment.h"
#include "time/musical_time.h"
#include "time/time_span.h"

static void test_segment(void) {
    MusicalTime *from = NULL;
    MusicalTime *to = NULL;
    TimeSpan *span = NULL;
    assert(musical_time_create(0, 1, &from) == MUSICAL_TIME_OK && musical_time_create(1, 2, &to) == MUSICAL_TIME_OK);
    assert(time_span_create(from, to, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK);

    GalantSegment galant;
    memset(&galant, 0, sizeof(galant));
    galant.beat_numerator = 1;
    galant.beat_denominator = 4;
    galant.uncertainty = 0.5;
    AnalysisSegment segment = {"seg-3", span, NULL, BEAT_STRENGTH_STRONG};
    assert(galant_segment_set_analysis_segment(&galant, &segment));
    assert(strcmp(galant.id, "seg-3") == 0);
    assert(galant.span == span);
    assert(galant.position == NULL);
    assert(galant.strength == BEAT_STRENGTH_STRONG);
    assert(galant.beat_numerator == 1 && galant.beat_denominator == 4 && galant.uncertainty == 0.5);

    AnalysisSegment no_id = {NULL, span, NULL, BEAT_STRENGTH_WEAK};
    assert(!galant_segment_set_analysis_segment(&galant, &no_id));
    AnalysisSegment empty_id = {"", span, NULL, BEAT_STRENGTH_WEAK};
    assert(!galant_segment_set_analysis_segment(&galant, &empty_id));
    AnalysisSegment no_span = {"x", NULL, NULL, BEAT_STRENGTH_WEAK};
    assert(!galant_segment_set_analysis_segment(&galant, &no_span));
    char long_id[GALANT_ANALYSIS_ID_MAX + 8];
    memset(long_id, 'a', sizeof(long_id));
    long_id[sizeof(long_id) - 1] = '\0';
    AnalysisSegment too_long = {long_id, span, NULL, BEAT_STRENGTH_WEAK};
    assert(!galant_segment_set_analysis_segment(&galant, &too_long));
    assert(!galant_segment_set_analysis_segment(&galant, NULL));
    assert(!galant_segment_set_analysis_segment(NULL, &segment));
    assert(strcmp(galant.id, "seg-3") == 0);

    /* A segment set this way is accepted by the list when the rest is filled. */
    galant.key.degree = 1;
    GalantSegmentList list;
    galant_segments_init(&list);
    assert(galant_segments_add(&list, &galant) == GALANT_ANALYSIS_OK);
    galant_segments_destroy(&list);

    time_span_destroy(span);
    musical_time_destroy(from);
    musical_time_destroy(to);
}

static void test_key(void) {
    GalantSegment galant;
    memset(&galant, 0, sizeof(galant));

    char long_id[GALANT_ANALYSIS_ID_MAX + 8];
    memset(long_id, 'k', sizeof(long_id));
    long_id[sizeof(long_id) - 1] = '\0';
}

int main(void) {
    test_segment();
    test_key();
    printf("test_galant_analysis_segment: ok\n");
    return 0;
}
