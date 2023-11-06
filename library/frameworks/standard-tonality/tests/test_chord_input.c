#include "adapter_test_util.h"

/* The adapter input builder: kernel Chord and Key structure in, a plain record per segment out. */

static void test_reads_chord_and_local_key(void) {
    Key *c_major = adapter_test_key("C major", 0, KEY_STRUCTURE_MODE_MAJOR);
    Key *g_major = adapter_test_key("G major", 7, KEY_STRUCTURE_MODE_MAJOR);
    MusicalTime *zero = adapter_test_time(0, 1);
    MusicalTime *two = adapter_test_time(2, 1);
    MusicalTime *one = adapter_test_time(1, 1);
    MusicalTime *three = adapter_test_time(3, 1);
    StandardTonalityKeyRegionInput regions[2] = {{c_major, zero, 0.25}, {g_major, two, 0.5}};

    Chord *first = adapter_test_chord("s1", 7, 11, ADAPTER_MAJOR_TRIAD, 3, 800000UL);
    Chord *second = adapter_test_chord("s2", 7, 7, ADAPTER_MAJOR_TRIAD, 3, 1000000UL);
    double weights[12] = {0};
    weights[7] = 2.0;
    StandardTonalitySegmentInput segments[2] = {{"s1", one, first, weights}, {"s2", three, second, NULL}};

    StandardTonalityChordInput *inputs = NULL;
    size_t count = 0;
    assert(standard_tonality_build_chord_inputs(segments, 2, regions, 2, &inputs, &count) ==
           STANDARD_TONALITY_CHORD_INPUT_OK);
    assert(count == 2);

    assert(inputs[0].has_numeral && inputs[0].region_index == 0);
    assert(inputs[0].root_pitch_class == 7 && inputs[0].bass_pitch_class == 11);
    assert(inputs[0].tonic_pitch_class == 0 && inputs[0].mode == STANDARD_TONALITY_ROMAN_KEY_MAJOR);
    assert(inputs[0].interval_count == 3 && inputs[0].intervals[1] == 4);
    assert(inputs[0].chord_uncertainty > 0.19 && inputs[0].chord_uncertainty < 0.21);
    assert(inputs[0].key_uncertainty == 0.25);
    assert(inputs[0].has_weights && inputs[0].weights[7] == 2.0);

    /* The second segment starts inside the second region: read in G major. */
    assert(inputs[1].has_numeral && inputs[1].region_index == 1 && inputs[1].tonic_pitch_class == 7);
    assert(inputs[1].key_uncertainty == 0.5 && !inputs[1].has_weights);

    standard_tonality_chord_inputs_destroy(inputs);
    chord_destroy(first);
    chord_destroy(second);
    key_destroy(c_major);
    key_destroy(g_major);
    musical_time_destroy(zero);
    musical_time_destroy(one);
    musical_time_destroy(two);
    musical_time_destroy(three);
}

static void test_segment_without_match_or_key_passes_through(void) {
    Key *c_major = adapter_test_key("C major", 0, KEY_STRUCTURE_MODE_MAJOR);
    MusicalTime *four = adapter_test_time(4, 1);
    MusicalTime *five = adapter_test_time(5, 1);
    MusicalTime *one = adapter_test_time(1, 1);
    StandardTonalityKeyRegionInput regions[1] = {{c_major, four, 0.0}};
    Chord *chord = adapter_test_chord("s", 0, 0, ADAPTER_MAJOR_TRIAD, 3, 1000000UL);
    StandardTonalitySegmentInput segments[3] = {
        {"unmatched", five, NULL, NULL},   /* no chord identified */
        {"before", one, chord, NULL},      /* starts before the first region */
        {"ok", five, chord, NULL}};

    StandardTonalityChordInput *inputs = NULL;
    size_t count = 0;
    assert(standard_tonality_build_chord_inputs(segments, 3, regions, 1, &inputs, &count) ==
           STANDARD_TONALITY_CHORD_INPUT_OK);
    assert(count == 3 && !inputs[0].has_numeral && !inputs[1].has_numeral && inputs[2].has_numeral);
    assert(strcmp(inputs[0].segment_id, "unmatched") == 0);
    standard_tonality_chord_inputs_destroy(inputs);

    /* No regions at all: everything is no-numeral. */
    assert(standard_tonality_build_chord_inputs(segments, 3, NULL, 0, &inputs, &count) ==
           STANDARD_TONALITY_CHORD_INPUT_OK);
    assert(!inputs[0].has_numeral && !inputs[1].has_numeral && !inputs[2].has_numeral);
    standard_tonality_chord_inputs_destroy(inputs);

    chord_destroy(chord);
    key_destroy(c_major);
    musical_time_destroy(one);
    musical_time_destroy(four);
    musical_time_destroy(five);
}

static void test_rejects_bad_input(void) {
    StandardTonalityChordInput *inputs = NULL;
    size_t count = 0;
    assert(standard_tonality_build_chord_inputs(NULL, 0, NULL, 0, NULL, &count) ==
           STANDARD_TONALITY_CHORD_INPUT_ERR_NULL_ARGUMENT);
    StandardTonalitySegmentInput no_start[1] = {{"s", NULL, NULL, NULL}};
    assert(standard_tonality_build_chord_inputs(no_start, 1, NULL, 0, &inputs, &count) ==
           STANDARD_TONALITY_CHORD_INPUT_ERR_BAD_SEGMENT);
    Key *key = adapter_test_key("C major", 0, KEY_STRUCTURE_MODE_MAJOR);
    MusicalTime *zero = adapter_test_time(0, 1);
    StandardTonalityKeyRegionInput twice[2] = {{key, zero, 0.0}, {key, zero, 0.0}};
    assert(standard_tonality_build_chord_inputs(NULL, 0, twice, 2, &inputs, &count) ==
           STANDARD_TONALITY_CHORD_INPUT_ERR_BAD_REGION);
    key_destroy(key);
    musical_time_destroy(zero);
}

int main(void) {
    test_reads_chord_and_local_key();
    test_segment_without_match_or_key_passes_through();
    test_rejects_bad_input();
    printf("standard-tonality chord input: all tests passed\n");
    return 0;
}
