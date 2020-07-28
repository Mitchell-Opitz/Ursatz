#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "repeat_navigation.h"

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

static NavigationMarker mk(size_t measure, StructuralKind kind) {
    NavigationMarker marker;
    marker.measure = measure;
    marker.kind = kind;
    marker.parameter = 0;
    return marker;
}

static NavigationMarker ending(size_t measure, unsigned int number) {
    NavigationMarker marker = mk(measure, STRUCTURAL_KIND_ENDING);
    marker.parameter = number;
    return marker;
}

/* Runs the navigation and checks the exact order. */
static void expect_order(const NavigationMarker *markers, size_t marker_count, size_t measure_count,
                         const NavigationOptions *options, const size_t *expected, size_t expected_count) {
    size_t order[64];
    size_t length = 0;
    NavigationResult result = standard_markings_playback_order(markers, marker_count, measure_count, options,
                                                               order, ARRAY_COUNT(order), &length);
    assert(result == NAVIGATION_OK);
    assert(length == expected_count);
    for (size_t i = 0; i < expected_count; i++) {
        assert(order[i] == expected[i]);
    }
}

static void test_no_markers_plays_straight_through(void) {
    size_t expected[] = {0, 1, 2, 3};
    expect_order(NULL, 0, 4, NULL, expected, 4);

    size_t length = 99;
    size_t order[4];
    assert(standard_markings_playback_order(NULL, 0, 0, NULL, order, 4, &length) == NAVIGATION_OK);
    assert(length == 0);
}

static void test_simple_repeat(void) {
    NavigationMarker markers[] = {mk(1, STRUCTURAL_KIND_REPEAT_START), mk(2, STRUCTURAL_KIND_REPEAT_END)};
    size_t expected[] = {0, 1, 2, 1, 2, 3, 4};
    expect_order(markers, 2, 5, NULL, expected, 7);
}

/* Without a repeat-start sign, a repeat-end goes back to the beginning. */
static void test_repeat_without_start_goes_to_beginning(void) {
    NavigationMarker markers[] = {mk(1, STRUCTURAL_KIND_REPEAT_END)};
    size_t expected[] = {0, 1, 0, 1, 2, 3};
    expect_order(markers, 1, 4, NULL, expected, 6);
}

/* Markers may be given in any order. */
static void test_marker_order_does_not_matter(void) {
    NavigationMarker markers[] = {mk(2, STRUCTURAL_KIND_REPEAT_END), mk(1, STRUCTURAL_KIND_REPEAT_START)};
    size_t expected[] = {0, 1, 2, 1, 2, 3, 4};
    expect_order(markers, 2, 5, NULL, expected, 7);
}

static void test_two_repeated_sections(void) {
    NavigationMarker markers[] = {mk(0, STRUCTURAL_KIND_REPEAT_START), mk(1, STRUCTURAL_KIND_REPEAT_END),
                                  mk(2, STRUCTURAL_KIND_REPEAT_START), mk(3, STRUCTURAL_KIND_REPEAT_END)};
    size_t expected[] = {0, 1, 0, 1, 2, 3, 2, 3};
    expect_order(markers, 4, 4, NULL, expected, 8);
}

/* First and second endings: the first ending the first time, the second the second. */
static void test_first_and_second_endings(void) {
    NavigationMarker markers[] = {mk(0, STRUCTURAL_KIND_REPEAT_START), ending(2, 1),
                                  mk(2, STRUCTURAL_KIND_REPEAT_END), ending(3, 2)};
    size_t expected[] = {0, 1, 2, 0, 1, 3, 4, 5};
    expect_order(markers, 4, 6, NULL, expected, 8);
}

/* A third ending is played after the third time through. */
static void test_third_ending(void) {
    NavigationMarker markers[] = {mk(0, STRUCTURAL_KIND_REPEAT_START), ending(1, 1),
                                  mk(1, STRUCTURAL_KIND_REPEAT_END),   ending(2, 2),
                                  mk(2, STRUCTURAL_KIND_REPEAT_END),   ending(3, 3)};
    size_t expected[] = {0, 1, 0, 2, 0, 3, 4};
    expect_order(markers, 6, 5, NULL, expected, 7);
}

/* Example 19: D.C. al Fine -- from the beginning, until the Fine. */
static void test_da_capo_al_fine(void) {
    NavigationMarker markers[] = {mk(1, STRUCTURAL_KIND_FINE), mk(3, STRUCTURAL_KIND_DC_AL_FINE)};
    size_t expected[] = {0, 1, 2, 3, 0, 1};
    expect_order(markers, 2, 4, NULL, expected, 6);
}

/* Example 18: D.C. al Coda -- beginning, to the "to coda" sign, then the coda.
   The Fine/to-coda signs are ignored the first time through. */
static void test_da_capo_al_coda(void) {
    NavigationMarker markers[] = {mk(2, STRUCTURAL_KIND_TO_CODA), mk(3, STRUCTURAL_KIND_DC_AL_CODA),
                                  mk(4, STRUCTURAL_KIND_CODA)};
    size_t expected[] = {0, 1, 2, 3, 0, 1, 2, 4, 5};
    expect_order(markers, 3, 6, NULL, expected, 9);
}

/* D.S. goes back to the segno instead of the beginning. */
static void test_dal_segno(void) {
    NavigationMarker al_coda[] = {mk(1, STRUCTURAL_KIND_SEGNO), mk(2, STRUCTURAL_KIND_TO_CODA),
                                  mk(3, STRUCTURAL_KIND_DS_AL_CODA), mk(4, STRUCTURAL_KIND_CODA)};
    size_t expected_coda[] = {0, 1, 2, 3, 1, 2, 4, 5};
    expect_order(al_coda, 4, 6, NULL, expected_coda, 8);

    NavigationMarker al_fine[] = {mk(1, STRUCTURAL_KIND_SEGNO), mk(2, STRUCTURAL_KIND_FINE),
                                  mk(3, STRUCTURAL_KIND_DS_AL_FINE)};
    size_t expected_fine[] = {0, 1, 2, 3, 1, 2};
    expect_order(al_fine, 3, 4, NULL, expected_fine, 6);
}

/* Convention: after the jump, repeat signs are not taken again. */
static void test_repeats_not_retaken_after_jump(void) {
    NavigationMarker markers[] = {mk(0, STRUCTURAL_KIND_REPEAT_START), mk(1, STRUCTURAL_KIND_REPEAT_END),
                                  mk(2, STRUCTURAL_KIND_FINE), mk(3, STRUCTURAL_KIND_DC_AL_FINE)};
    size_t expected[] = {0, 1, 0, 1, 2, 3, 0, 1, 2};
    expect_order(markers, 4, 4, NULL, expected, 9);

    NavigationOptions defaults = standard_markings_default_navigation_options();
    assert(!defaults.take_repeats_after_jump);
    expect_order(markers, 4, 4, &defaults, expected, 9);
}

/* The option: with repeats retaken after the jump, they replay by the ordinary rules. */
static void test_repeats_retaken_when_option_set(void) {
    NavigationMarker markers[] = {mk(0, STRUCTURAL_KIND_REPEAT_START), mk(1, STRUCTURAL_KIND_REPEAT_END),
                                  mk(2, STRUCTURAL_KIND_FINE), mk(3, STRUCTURAL_KIND_DC_AL_FINE)};
    NavigationOptions options;
    options.take_repeats_after_jump = true;
    size_t expected[] = {0, 1, 0, 1, 2, 3, 0, 1, 0, 1, 2};
    expect_order(markers, 4, 4, &options, expected, 11);
}

/* Convention: on the jump pass each ending group plays its last ending. */
static void test_last_ending_on_jump_pass(void) {
    NavigationMarker markers[] = {mk(0, STRUCTURAL_KIND_REPEAT_START), ending(1, 1),
                                  mk(1, STRUCTURAL_KIND_REPEAT_END),   ending(2, 2),
                                  mk(3, STRUCTURAL_KIND_FINE),         mk(4, STRUCTURAL_KIND_DC_AL_FINE)};
    size_t expected[] = {0, 1, 0, 2, 3, 4, 0, 2, 3};
    expect_order(markers, 6, 5, NULL, expected, 9);
}

static void test_errors(void) {
    size_t order[16];
    size_t length = 0;

    NavigationMarker dc_no_coda[] = {mk(1, STRUCTURAL_KIND_DC_AL_CODA)};
    assert(standard_markings_playback_order(dc_no_coda, 1, 3, NULL, order, 16, &length) ==
           NAVIGATION_ERR_MISSING_CODA);

    NavigationMarker ds_no_segno[] = {mk(1, STRUCTURAL_KIND_DS_AL_FINE)};
    assert(standard_markings_playback_order(ds_no_segno, 1, 3, NULL, order, 16, &length) ==
           NAVIGATION_ERR_MISSING_SEGNO);

    NavigationMarker out_of_range[] = {mk(5, STRUCTURAL_KIND_FERMATA)};
    assert(standard_markings_playback_order(out_of_range, 1, 3, NULL, order, 16, &length) ==
           NAVIGATION_ERR_MARKER_OUT_OF_RANGE);

    NavigationMarker zero_ending[] = {ending(1, 0)};
    assert(standard_markings_playback_order(zero_ending, 1, 3, NULL, order, 16, &length) ==
           NAVIGATION_ERR_MALFORMED_MARKER);

    NavigationMarker unknown[] = {mk(1, STRUCTURAL_KIND_UNKNOWN)};
    assert(standard_markings_playback_order(unknown, 1, 3, NULL, order, 16, &length) ==
           NAVIGATION_ERR_MALFORMED_MARKER);

    assert(standard_markings_playback_order(NULL, 1, 3, NULL, order, 16, &length) == NAVIGATION_ERR_NULL_ARGUMENT);
    assert(standard_markings_playback_order(NULL, 0, 3, NULL, order, 16, NULL) == NAVIGATION_ERR_NULL_ARGUMENT);

    assert(standard_markings_playback_order(NULL, 0, 4, NULL, order, 3, &length) == NAVIGATION_ERR_BUFFER_TOO_SMALL);
}

/* Fermata, caesura and breath marks do not change the order. */
static void test_non_navigating_markers_are_ignored(void) {
    NavigationMarker markers[] = {mk(0, STRUCTURAL_KIND_FERMATA), mk(1, STRUCTURAL_KIND_CAESURA),
                                  mk(2, STRUCTURAL_KIND_BREATH_MARK)};
    size_t expected[] = {0, 1, 2, 3};
    expect_order(markers, 3, 4, NULL, expected, 4);
}

static size_t source_of(const NavigationMarker *markers, size_t count, size_t measure) {
    size_t source = 99;
    assert(standard_markings_measure_content_source(markers, count, measure, &source));
    return source;
}

static void test_measure_content_source(void) {
    /* A measure with no repeat sign is its own source. */
    assert(source_of(NULL, 0, 3) == 3);

    /* A one-measure repeat plays the previous measure. */
    NavigationMarker one[] = {mk(2, STRUCTURAL_KIND_ONE_MEASURE_REPEAT)};
    assert(source_of(one, 1, 1) == 1);
    assert(source_of(one, 1, 2) == 1);

    /* Chains resolve through to written content. */
    NavigationMarker chain[] = {mk(1, STRUCTURAL_KIND_ONE_MEASURE_REPEAT), mk(2, STRUCTURAL_KIND_ONE_MEASURE_REPEAT)};
    assert(source_of(chain, 2, 2) == 0);
    assert(source_of(chain, 2, 1) == 0);

    /* A two-measure repeat at m covers m and m + 1, repeating m - 2 and m - 1. */
    NavigationMarker two[] = {mk(4, STRUCTURAL_KIND_TWO_MEASURE_REPEAT)};
    assert(source_of(two, 1, 4) == 2);
    assert(source_of(two, 1, 5) == 3);
    assert(source_of(two, 1, 3) == 3);
    assert(source_of(two, 1, 6) == 6);

    /* A repeat sign pointing before the first measure has no source. */
    size_t source = 99;
    NavigationMarker first[] = {mk(0, STRUCTURAL_KIND_ONE_MEASURE_REPEAT)};
    assert(!standard_markings_measure_content_source(first, 1, 0, &source));
    NavigationMarker early_two[] = {mk(1, STRUCTURAL_KIND_TWO_MEASURE_REPEAT)};
    assert(!standard_markings_measure_content_source(early_two, 1, 1, &source));
    assert(!standard_markings_measure_content_source(NULL, 0, 0, NULL));
    assert(source == 99);
}

int main(void) {
    test_no_markers_plays_straight_through();
    test_simple_repeat();
    test_repeat_without_start_goes_to_beginning();
    test_marker_order_does_not_matter();
    test_two_repeated_sections();
    test_first_and_second_endings();
    test_third_ending();
    test_da_capo_al_fine();
    test_da_capo_al_coda();
    test_dal_segno();
    test_repeats_not_retaken_after_jump();
    test_repeats_retaken_when_option_set();
    test_last_ending_on_jump_pass();
    test_errors();
    test_non_navigating_markers_are_ignored();
    test_measure_content_source();
    printf("test_repeat_navigation (standard-markings): ok\n");
    return 0;
}
