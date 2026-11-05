#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>

#include "motive/motive_segments.h"

/* The segment proposal and the segments of a motive analysis (docs/motive-format.md, "Segments"). */

static const PlanMeter FOUR_FOUR = {4, 4, 0};
static const PlanMeter THREE_FOUR = {3, 4, 0};
static const AnalysisKey C_MAJOR = {NULL, PITCH_LETTER_C, 0, KEY_STRUCTURE_MODE_MAJOR};

static MotiveAnalysis analyze(const char *text, const PlanMeter *meter, const MotiveOptions *extra) {
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    if (extra) {
        options = *extra;
    }
    options.has_meter = true;
    options.meter = *meter;
    options.has_key = true;
    options.key = C_MAJOR;
    Motive motive;
    MotiveError error;
    MotiveResult result = motive_from_notes_text(text, &options, &motive, &error);
    if (result != MOTIVE_OK) {
        fprintf(stderr, "'%s': %s: %s\n", text, error.location, error.message);
    }
    assert(result == MOTIVE_OK);
    MotiveAnalysis analysis;
    assert(motive_analyze(&motive, &analysis));
    return analysis;
}

static void expect(const MotiveAnalysis *analysis, size_t index, const char *label, size_t first, size_t last) {
    assert(index < analysis->segment_count);
    assert(strcmp(analysis->segments[index].label, label) == 0);
    assert(analysis->segments[index].first == first && analysis->segments[index].last == last);
}

static void test_repeated_rhythm(void) {
    MotiveAnalysis analysis = analyze("C4:q D4:q E4:q F4:q | C4:q D4:q E4:q F4:q", &FOUR_FOUR, NULL);
    assert(!analysis.segments_given && analysis.segment_count == 2);
    expect(&analysis, 0, "a", 0, 3);
    expect(&analysis, 1, "a'", 4, 7);
}

static void test_rest_splits(void) {
    MotiveAnalysis analysis = analyze("C4:e D4:e r:q E4:e F4:e G4:q", &FOUR_FOUR, NULL);
    assert(analysis.segment_count == 2);
    expect(&analysis, 0, "a", 0, 1);
    expect(&analysis, 1, "b", 2, 4);
}

static void test_long_note_splits(void) {
    /* The quarter after four sixteenths is a beat long and four times the sixteenth before it. */
    MotiveAnalysis analysis = analyze("C4:s D4:s E4:s F4:s G4:q A4:e B4:e C5:q", &FOUR_FOUR, NULL);
    assert(analysis.segment_count == 2);
    expect(&analysis, 0, "a", 0, 4);
    expect(&analysis, 1, "b", 5, 7);
}

static void test_pickup_joins_first(void) {
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_pickup = true;
    options.pickup.num = 1;
    options.pickup.den = 4;
    MotiveAnalysis analysis = analyze("G4:q C5:q D5:q E5:q F5:q G5:q A5:q", &THREE_FOUR, &options);
    assert(analysis.rhythm[0].bar == -1);
    assert(analysis.segment_count == 2);
    expect(&analysis, 0, "a", 0, 3);
    expect(&analysis, 1, "b", 4, 6);
}

static void test_one_group(void) {
    MotiveAnalysis analysis = analyze("C4:q D4:q E4:q F4:q", &FOUR_FOUR, NULL);
    assert(analysis.segment_count == 1);
    expect(&analysis, 0, "a", 0, 3);
}

static void test_cap(void) {
    MotiveAnalysis analysis =
        analyze("C4:w D4:w E4:w F4:w G4:w A4:w B4:w C5:w D5:w E5:w", &FOUR_FOUR, NULL);
    assert(analysis.segment_count == 8);
    size_t next = 0;
    for (size_t i = 0; i < analysis.segment_count; i++) {
        assert(analysis.segments[i].first == next);
        next = analysis.segments[i].last + 1;
    }
    assert(next == 10);
    /* Earliest first among the smallest pairs: the first two segments are two notes each. */
    expect(&analysis, 0, "a", 0, 1);
    expect(&analysis, 1, "a'", 2, 3);
}

static void test_given_segments_kept(void) {
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_features = true;
    options.feature_count = 2;
    snprintf(options.features[0].label, sizeof(options.features[0].label), "x");
    options.features[0].first = 0;
    options.features[0].last = 1;
    snprintf(options.features[1].label, sizeof(options.features[1].label), "y");
    options.features[1].first = 2;
    options.features[1].last = 3;
    MotiveAnalysis analysis = analyze("C4:q D4:q E4:q F4:q | C4:q D4:q E4:q F4:q", &FOUR_FOUR, &options);
    assert(analysis.segments_given && analysis.segment_count == 2);
    expect(&analysis, 0, "x", 0, 1);
    expect(&analysis, 1, "y", 2, 3);
}

static void test_deterministic(void) {
    MotiveAnalysis first = analyze("C4:q D4:q E4:q F4:q | C4:q D4:q E4:q F4:q", &FOUR_FOUR, NULL);
    MotiveAnalysis second = analyze("C4:q D4:q E4:q F4:q | C4:q D4:q E4:q F4:q", &FOUR_FOUR, NULL);
    assert(first.segment_count == second.segment_count);
    for (size_t i = 0; i < first.segment_count; i++) {
        assert(strcmp(first.segments[i].label, second.segments[i].label) == 0);
        assert(first.segments[i].first == second.segments[i].first && first.segments[i].last == second.segments[i].last);
    }
    MotiveSketchFeature out[MOTIVE_MAX_FEATURES];
    assert(motive_segments_propose(&first, out, MOTIVE_MAX_FEATURES) == first.segment_count);
    assert(motive_segments_propose(&first, out, 1) == 1 && out[0].first == 0 && out[0].last == 7);
}

int main(void) {
    test_repeated_rhythm();
    test_rest_splits();
    test_long_note_splits();
    test_pickup_joins_first();
    test_one_group();
    test_cap();
    test_given_segments_kept();
    test_deterministic();
    return 0;
}
