#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis_input.h"
#include "semantics/analysis_key.h"
#include "semantics/analysis_segment.h"
#include "time/musical_time.h"
#include "time/time_span.h"

static void test_mode(void) {
    HarmonyMode mode = HARMONY_MODE_COUNT;
    AnalysisKey major = {"k", PITCH_LETTER_D, 0, KEY_STRUCTURE_MODE_MAJOR};
    assert(harmony_mode_of_analysis_key(&major, &mode) && mode == HARMONY_MODE_MAJOR);
    AnalysisKey minor = {"k", PITCH_LETTER_D, 0, KEY_STRUCTURE_MODE_MINOR};
    assert(harmony_mode_of_analysis_key(&minor, &mode) && mode == HARMONY_MODE_MINOR);

    mode = HARMONY_MODE_COUNT;
    AnalysisKey bad = {"k", PITCH_LETTER_D, 0, (KeyStructureMode)7};
    assert(!harmony_mode_of_analysis_key(&bad, &mode) && mode == HARMONY_MODE_COUNT);
    assert(!harmony_mode_of_analysis_key(NULL, &mode));
    assert(!harmony_mode_of_analysis_key(&major, NULL));
}

static void test_segment(void) {
    MusicalTime *from = NULL;
    MusicalTime *to = NULL;
    TimeSpan *span = NULL;
    assert(musical_time_create(0, 1, &from) == MUSICAL_TIME_OK && musical_time_create(1, 2, &to) == MUSICAL_TIME_OK);
    assert(time_span_create(from, to, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) == TIME_SPAN_OK);

    HarmonySegmentInput input;
    memset(&input, 0, sizeof(input));
    input.degree = 5;
    input.inversion = 1;
    input.uncertainty = 0.25;
    AnalysisSegment segment = {"seg-7", span, NULL, BEAT_STRENGTH_MEDIUM};
    assert(harmony_segment_input_set_segment(&input, &segment));
    assert(strcmp(input.segment_id, "seg-7") == 0);
    assert(input.span == span);
    assert(input.position == NULL);
    assert(input.beat_strength == BEAT_STRENGTH_MEDIUM);
    /* Nothing else is touched. */
    assert(input.degree == 5 && input.inversion == 1 && input.uncertainty == 0.25);

    assert(!harmony_segment_input_set_segment(&input, NULL));
    assert(!harmony_segment_input_set_segment(NULL, &segment));

    time_span_destroy(span);
    musical_time_destroy(from);
    musical_time_destroy(to);
}

int main(void) {
    test_mode();
    test_segment();
    printf("test_harmony_analysis_key: ok\n");
    return 0;
}
