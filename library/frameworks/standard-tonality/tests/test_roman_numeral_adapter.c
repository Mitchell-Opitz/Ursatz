#include <math.h>

#include "adapter_test_util.h"
#include "roman_numeral_adapter.h"

/* Roman numeral Interpretations per chord segment, in the segment's local key. */

static StandardTonalityChordInput chord_in(int root, int bass, const int *intervals, size_t count, int tonic,
                                           StandardTonalityRomanKeyMode mode) {
    StandardTonalityChordInput input;
    memset(&input, 0, sizeof(input));
    input.segment_id = "segment";
    input.has_numeral = true;
    input.root_pitch_class = root;
    input.bass_pitch_class = bass;
    input.interval_count = count;
    for (size_t i = 0; i < count; i++) {
        input.intervals[i] = intervals[i];
    }
    input.tonic_pitch_class = tonic;
    input.mode = mode;
    return input;
}

static void expect_label(StandardTonalityChordInput input, const char *label) {
    StandardTonalityRomanReading reading;
    char buffer[STANDARD_TONALITY_ROMAN_LABEL_MAX];
    if (!standard_tonality_read_roman_numeral(&input, &reading) ||
        !standard_tonality_format_roman_reading(&reading, buffer, sizeof(buffer)) || strcmp(buffer, label) != 0) {
        fprintf(stderr, "expected %s, got %s\n", label, buffer);
        assert(0);
    }
}

static void expect_none(StandardTonalityChordInput input) {
    StandardTonalityRomanReading reading;
    assert(!standard_tonality_read_roman_numeral(&input, &reading));
}

#define C_MAJOR STANDARD_TONALITY_ROMAN_KEY_MAJOR
#define A_MINOR STANDARD_TONALITY_ROMAN_KEY_MINOR

static void test_triads_and_sevenths_in_c(void) {
    expect_label(chord_in(0, 0, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR), "I");
    expect_label(chord_in(0, 4, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR), "I6");
    expect_label(chord_in(0, 7, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR), "I6/4");
    expect_label(chord_in(5, 5, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR), "IV");
    expect_label(chord_in(5, 9, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR), "IV6");
    expect_label(chord_in(5, 0, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR), "IV6/4");
    expect_label(chord_in(7, 7, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR), "V");
    expect_label(chord_in(7, 7, ADAPTER_DOMINANT_SEVENTH, 4, 0, C_MAJOR), "V7");
    expect_label(chord_in(7, 11, ADAPTER_DOMINANT_SEVENTH, 4, 0, C_MAJOR), "V6/5");
    expect_label(chord_in(7, 2, ADAPTER_DOMINANT_SEVENTH, 4, 0, C_MAJOR), "V4/3");
    expect_label(chord_in(7, 5, ADAPTER_DOMINANT_SEVENTH, 4, 0, C_MAJOR), "V4/2");
    expect_label(chord_in(2, 2, ADAPTER_MINOR_TRIAD, 3, 0, C_MAJOR), "ii");
    expect_label(chord_in(11, 2, ADAPTER_DIM_TRIAD, 3, 0, C_MAJOR), "vii\xC2\xB0" "6");
    /* An unknown bass, and a bass that is not a chord member, read as root position. */
    expect_label(chord_in(7, -1, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR), "V");
    expect_label(chord_in(7, 9, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR), "V");
}

static void test_minor_key(void) {
    expect_label(chord_in(9, 9, ADAPTER_MINOR_TRIAD, 3, 9, A_MINOR), "i");
    expect_label(chord_in(2, 2, ADAPTER_MINOR_TRIAD, 3, 9, A_MINOR), "iv");
    expect_label(chord_in(4, 4, ADAPTER_MAJOR_TRIAD, 3, 9, A_MINOR), "V");           /* the raised 7th, G# */
    expect_label(chord_in(8, 8, ADAPTER_DIM_TRIAD, 3, 9, A_MINOR), "vii\xC2\xB0");   /* G# diminished */
    expect_label(chord_in(4, 8, ADAPTER_DOMINANT_SEVENTH, 4, 9, A_MINOR), "V6/5");
    expect_label(chord_in(7, 7, ADAPTER_MAJOR_TRIAD, 3, 9, A_MINOR), "VII");          /* the subtonic */
    expect_label(chord_in(4, 4, ADAPTER_MINOR_TRIAD, 3, 9, A_MINOR), "v");            /* natural minor v */
}

static void test_chords_the_key_does_not_give(void) {
    expect_none(chord_in(2, 2, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR));  /* D major in C: an applied chord */
    expect_none(chord_in(0, 0, ADAPTER_MINOR_TRIAD, 3, 0, C_MAJOR));  /* C minor in C major: mixture */
    expect_none(chord_in(1, 1, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR));  /* a chromatic root */
    expect_none(chord_in(8, 8, ADAPTER_MAJOR_TRIAD, 3, 9, A_MINOR));  /* G# major in A minor */
    StandardTonalityChordInput no_numeral = chord_in(0, 0, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR);
    no_numeral.has_numeral = false;
    expect_none(no_numeral);
}

static void test_key_boundary_switches_the_numeral_key(void) {
    /* The same G major triad, read in C major then in G major. */
    expect_label(chord_in(7, 7, ADAPTER_MAJOR_TRIAD, 3, 0, C_MAJOR), "V");
    expect_label(chord_in(7, 7, ADAPTER_MAJOR_TRIAD, 3, 7, C_MAJOR), "I");

    /* Through the builder: two segments, a region boundary between them. */
    Key *c_major = adapter_test_key("C major", 0, KEY_STRUCTURE_MODE_MAJOR);
    Key *g_major = adapter_test_key("G major", 7, KEY_STRUCTURE_MODE_MAJOR);
    MusicalTime *zero = adapter_test_time(0, 1);
    MusicalTime *two = adapter_test_time(2, 1);
    MusicalTime *one = adapter_test_time(1, 1);
    MusicalTime *three = adapter_test_time(3, 1);
    StandardTonalityKeyRegionInput regions[2] = {{c_major, zero, 0.0}, {g_major, two, 0.0}};
    Chord *g_chord = adapter_test_chord("g", 7, 7, ADAPTER_MAJOR_TRIAD, 3, 1000000UL);
    StandardTonalitySegmentInput segments[2] = {{"before", one, g_chord, NULL}, {"after", three, g_chord, NULL}};

    StandardTonalityChordInput *inputs = NULL;
    size_t count = 0;
    assert(standard_tonality_build_chord_inputs(segments, 2, regions, 2, &inputs, &count) ==
           STANDARD_TONALITY_CHORD_INPUT_OK);
    StandardTonalityRomanResult *results = NULL;
    assert(standard_tonality_build_roman_numerals(inputs, count, &results) == STANDARD_TONALITY_ROMAN_OK);
    assert(results[0].has_numeral && strcmp(results[0].claim.label, "V") == 0);
    assert(results[1].has_numeral && strcmp(results[1].claim.label, "I") == 0);
    standard_tonality_roman_results_destroy(results, count);
    standard_tonality_chord_inputs_destroy(inputs);
    chord_destroy(g_chord);
    key_destroy(c_major);
    key_destroy(g_major);
    musical_time_destroy(zero);
    musical_time_destroy(one);
    musical_time_destroy(two);
    musical_time_destroy(three);
}

static void test_no_match_segment_gives_no_numeral(void) {
    Key *c_major = adapter_test_key("C major", 0, KEY_STRUCTURE_MODE_MAJOR);
    MusicalTime *zero = adapter_test_time(0, 1);
    StandardTonalityKeyRegionInput regions[1] = {{c_major, zero, 0.0}};
    StandardTonalitySegmentInput segments[1] = {{"unmatched", zero, NULL, NULL}};
    StandardTonalityChordInput *inputs = NULL;
    size_t count = 0;
    assert(standard_tonality_build_chord_inputs(segments, 1, regions, 1, &inputs, &count) ==
           STANDARD_TONALITY_CHORD_INPUT_OK);
    StandardTonalityRomanResult *results = NULL;
    assert(standard_tonality_build_roman_numerals(inputs, count, &results) == STANDARD_TONALITY_ROMAN_OK);
    assert(count == 1 && !results[0].has_numeral && results[0].interpretation == NULL);
    assert(strcmp(results[0].segment_id, "unmatched") == 0);
    standard_tonality_roman_results_destroy(results, count);
    standard_tonality_chord_inputs_destroy(inputs);
    key_destroy(c_major);
    musical_time_destroy(zero);
}

static void test_interpretation_and_uncertainty(void) {
    StandardTonalityChordInput input = chord_in(7, 11, ADAPTER_DOMINANT_SEVENTH, 4, 0, C_MAJOR);
    input.segment_id = "sonority-12";
    input.chord_uncertainty = 0.2;
    input.key_uncertainty = 0.5;

    StandardTonalityRomanResult *results = NULL;
    assert(standard_tonality_build_roman_numerals(&input, 1, &results) == STANDARD_TONALITY_ROMAN_OK);
    assert(results[0].has_numeral);
    /* 1 - (1 - 0.2)(1 - 0.5) = 0.6 */
    assert(fabs(results[0].uncertainty - 0.6) < 1e-9);

    const Interpretation *interpretation = results[0].interpretation;
    assert(interpretation != NULL);
    assert(strcmp(entity_id_value(interpretation_target_id(interpretation)), "sonority-12") == 0);
    assert(interpretation_framework_reference(interpretation) != NULL);
    const Uncertainty *uncertainty = interpretation_uncertainty(interpretation);
    assert(uncertainty_kind(uncertainty) == UNCERTAINTY_KIND_SCALAR_CONFIDENCE);
    assert(uncertainty_scalar_confidence_numerator(uncertainty) == 400000UL);
    assert(uncertainty_scalar_confidence_denominator(uncertainty) == 1000000UL);

    StandardTonalityRomanClaim claim;
    assert(standard_tonality_roman_claim_read(interpretation, &claim));
    assert(strcmp(claim.label, "V6/5") == 0);
    assert(claim.degree == 5 && claim.seventh && claim.inversion == 1 && !claim.applied);
    assert(claim.quality == (int)STANDARD_TONALITY_SEVENTH_MAJOR_MINOR);
    standard_tonality_roman_results_destroy(results, 1);

    /* Either source of uncertainty alone carries through; none gives full confidence. */
    input.chord_uncertainty = 0.0;
    input.key_uncertainty = 0.0;
    assert(standard_tonality_build_roman_numerals(&input, 1, &results) == STANDARD_TONALITY_ROMAN_OK);
    assert(results[0].uncertainty == 0.0);
    standard_tonality_roman_results_destroy(results, 1);
    input.chord_uncertainty = 1.0;
    assert(standard_tonality_build_roman_numerals(&input, 1, &results) == STANDARD_TONALITY_ROMAN_OK);
    assert(results[0].uncertainty == 1.0);
    standard_tonality_roman_results_destroy(results, 1);
}

int main(void) {
    test_triads_and_sevenths_in_c();
    test_minor_key();
    test_chords_the_key_does_not_give();
    test_key_boundary_switches_the_numeral_key();
    test_no_match_segment_gives_no_numeral();
    test_interpretation_and_uncertainty();
    printf("standard-tonality roman numeral adapter: all tests passed\n");
    return 0;
}
