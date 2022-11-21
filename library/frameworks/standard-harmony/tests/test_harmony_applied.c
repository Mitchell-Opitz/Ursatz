#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_applied.h"
#include "harmony_test_util.h"

#define SD APPLIED_CHORD_KIND_SECONDARY_DOMINANT
#define SLT APPLIED_CHORD_KIND_SECONDARY_LEADING_TONE
#define MAJ HARMONY_MODE_MAJOR
#define MIN HARMONY_MODE_MINOR

static HarmonyAppliedChord applied(AppliedChordKind kind, HarmonyAppliedQuality quality, int target) {
    HarmonyAppliedChord chord = {kind, quality, target};
    return chord;
}

static bool label_is(HarmonyMode mode, HarmonyAppliedChord chord, const char *expected) {
    char out[32];
    return harmony_applied_label(mode, &chord, out, sizeof(out)) && strcmp(out, expected) == 0;
}

static void test_qualities(void) {
    const char *const names[] = {"major-triad", "dominant-seventh", "diminished-triad", "fully-diminished-seventh",
                                 "half-diminished-seventh"};
    for (int i = 0; i < HARMONY_APPLIED_QUALITY_COUNT; i++) {
        assert(harmony_applied_quality_valid((HarmonyAppliedQuality)i));
        assert(strcmp(harmony_applied_quality_name((HarmonyAppliedQuality)i), names[i]) == 0);
    }
    assert(!harmony_applied_quality_valid(HARMONY_APPLIED_QUALITY_COUNT));
    assert(!harmony_applied_quality_valid((HarmonyAppliedQuality)-1));
    assert(harmony_applied_quality_name(HARMONY_APPLIED_QUALITY_COUNT) == NULL);
    assert(!harmony_applied_quality_is_seventh(HARMONY_APPLIED_MAJOR_TRIAD));
    assert(harmony_applied_quality_is_seventh(HARMONY_APPLIED_DOMINANT_SEVENTH));
    assert(!harmony_applied_quality_is_seventh(HARMONY_APPLIED_DIMINISHED_TRIAD));
    assert(harmony_applied_quality_is_seventh(HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH));
    assert(harmony_applied_quality_is_seventh(HARMONY_APPLIED_HALF_DIMINISHED_SEVENTH));
    assert(!harmony_applied_quality_is_seventh(HARMONY_APPLIED_QUALITY_COUNT));
}

static void test_a_quality_belongs_to_a_kind(void) {
    AppliedChordKind kind = APPLIED_CHORD_KIND_COUNT;
    assert(harmony_applied_quality_kind(HARMONY_APPLIED_MAJOR_TRIAD, &kind) && kind == SD);
    assert(harmony_applied_quality_kind(HARMONY_APPLIED_DOMINANT_SEVENTH, &kind) && kind == SD);
    assert(harmony_applied_quality_kind(HARMONY_APPLIED_DIMINISHED_TRIAD, &kind) && kind == SLT);
    assert(harmony_applied_quality_kind(HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH, &kind) && kind == SLT);
    assert(harmony_applied_quality_kind(HARMONY_APPLIED_HALF_DIMINISHED_SEVENTH, &kind) && kind == SLT);
    kind = APPLIED_CHORD_KIND_COUNT;
    assert(!harmony_applied_quality_kind(HARMONY_APPLIED_QUALITY_COUNT, &kind));
    assert(!harmony_applied_quality_kind(HARMONY_APPLIED_MAJOR_TRIAD, NULL));
    assert(kind == APPLIED_CHORD_KIND_COUNT);
}

static void test_the_targets_that_can_be_tonicized(void) {
    for (int degree = 0; degree <= 8; degree++) {
        assert(harmony_applied_target_valid(MAJ, degree) == (degree >= 2 && degree <= 6));
        assert(harmony_applied_target_valid(MIN, degree) == (degree >= 3 && degree <= 7));
    }
    assert(!harmony_applied_target_valid(HARMONY_MODE_COUNT, 5));
}

static void test_a_valid_applied_chord(void) {
    assert(harmony_applied_valid(MAJ, &(HarmonyAppliedChord){SD, HARMONY_APPLIED_DOMINANT_SEVENTH, 5}));
    assert(harmony_applied_valid(MIN, &(HarmonyAppliedChord){SLT, HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH, 6}));
    assert(!harmony_applied_valid(MAJ, &(HarmonyAppliedChord){SLT, HARMONY_APPLIED_MAJOR_TRIAD, 5}));
    assert(!harmony_applied_valid(MAJ, &(HarmonyAppliedChord){SD, HARMONY_APPLIED_DIMINISHED_TRIAD, 5}));
    assert(!harmony_applied_valid(MAJ, &(HarmonyAppliedChord){SD, HARMONY_APPLIED_MAJOR_TRIAD, 1}));
    assert(!harmony_applied_valid(MAJ, &(HarmonyAppliedChord){SD, HARMONY_APPLIED_MAJOR_TRIAD, 7}));
    assert(!harmony_applied_valid(MIN, &(HarmonyAppliedChord){SD, HARMONY_APPLIED_MAJOR_TRIAD, 2}));
    assert(!harmony_applied_valid(HARMONY_MODE_COUNT, &(HarmonyAppliedChord){SD, HARMONY_APPLIED_MAJOR_TRIAD, 5}));
    assert(!harmony_applied_valid(MAJ, &(HarmonyAppliedChord){APPLIED_CHORD_KIND_COUNT, HARMONY_APPLIED_MAJOR_TRIAD, 5}));
    assert(!harmony_applied_valid(MAJ, NULL));
}

static void test_labels(void) {
    assert(label_is(MAJ, applied(SD, HARMONY_APPLIED_DOMINANT_SEVENTH, 5), "V7/V"));
    assert(label_is(MAJ, applied(SD, HARMONY_APPLIED_MAJOR_TRIAD, 6), "V/vi"));
    assert(label_is(MAJ, applied(SD, HARMONY_APPLIED_DOMINANT_SEVENTH, 4), "V7/IV"));
    assert(label_is(MAJ, applied(SD, HARMONY_APPLIED_MAJOR_TRIAD, 3), "V/iii"));
    assert(label_is(MAJ, applied(SLT, HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH, 5), "viio7/V"));
    assert(label_is(MAJ, applied(SLT, HARMONY_APPLIED_DIMINISHED_TRIAD, 2), "viio/ii"));
    assert(label_is(MAJ, applied(SLT, HARMONY_APPLIED_HALF_DIMINISHED_SEVENTH, 4), "vii%7/IV"));
    assert(label_is(MIN, applied(SLT, HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH, 6), "viio7/VI"));
    assert(label_is(MIN, applied(SD, HARMONY_APPLIED_DOMINANT_SEVENTH, 4), "V7/iv"));
    assert(label_is(MIN, applied(SD, HARMONY_APPLIED_DOMINANT_SEVENTH, 7), "V7/VII"));
    assert(label_is(MIN, applied(SD, HARMONY_APPLIED_MAJOR_TRIAD, 3), "V/III"));
}

static void test_label_buffers_and_invalid_input(void) {
    HarmonyAppliedChord chord = applied(SD, HARMONY_APPLIED_MAJOR_TRIAD, 6);
    char out[8] = "xxxxxxx";
    assert(harmony_applied_label(MAJ, &chord, out, 5) && strcmp(out, "V/vi") == 0);
    strcpy(out, "xxxxxxx");
    assert(!harmony_applied_label(MAJ, &chord, out, 4) && strcmp(out, "xxxxxxx") == 0);
    assert(!harmony_applied_label(MAJ, &chord, out, 0));
    assert(!harmony_applied_label(MAJ, &chord, NULL, 8));
    assert(!harmony_applied_label(MAJ, NULL, out, 8));
    chord.target = 1;
    assert(!harmony_applied_label(MAJ, &chord, out, 8));
}

static void test_the_scale_semitones(void) {
    const int major[] = {0, 2, 4, 5, 7, 9, 11};
    const int minor[] = {0, 2, 3, 5, 7, 8, 10};
    for (int degree = 1; degree <= 7; degree++) {
        assert(harmony_applied_scale_semitone(MAJ, degree) == major[degree - 1]);
        assert(harmony_applied_scale_semitone(MIN, degree) == minor[degree - 1]);
    }
    assert(harmony_applied_scale_semitone(MAJ, 0) == -1);
    assert(harmony_applied_scale_semitone(MAJ, 8) == -1);
    assert(harmony_applied_scale_semitone(HARMONY_MODE_COUNT, 1) == -1);
}

static void expect_analysis(HarmonyMode mode, int root, HarmonyAppliedQuality quality, const char *label) {
    HarmonyAppliedChord chord;
    char out[32];
    assert(harmony_applied_analyze(mode, root, quality, &chord));
    assert(harmony_applied_label(mode, &chord, out, sizeof(out)));
    assert(strcmp(out, label) == 0);
}

static void test_analyzing_a_chromatic_chord_in_major(void) {
    expect_analysis(MAJ, 2, HARMONY_APPLIED_MAJOR_TRIAD, "V/V");
    expect_analysis(MAJ, 2, HARMONY_APPLIED_DOMINANT_SEVENTH, "V7/V");
    expect_analysis(MAJ, 6, HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH, "viio7/V");
    expect_analysis(MAJ, 6, HARMONY_APPLIED_DIMINISHED_TRIAD, "viio/V");
    expect_analysis(MAJ, 6, HARMONY_APPLIED_HALF_DIMINISHED_SEVENTH, "vii%7/V");
    expect_analysis(MAJ, 8, HARMONY_APPLIED_DIMINISHED_TRIAD, "viio/vi");
    expect_analysis(MAJ, 9, HARMONY_APPLIED_DOMINANT_SEVENTH, "V7/ii");
    expect_analysis(MAJ, 0, HARMONY_APPLIED_DOMINANT_SEVENTH, "V7/IV");
    expect_analysis(MAJ, 4, HARMONY_APPLIED_DOMINANT_SEVENTH, "V7/vi");
    expect_analysis(MAJ, 11, HARMONY_APPLIED_DOMINANT_SEVENTH, "V7/iii");
    expect_analysis(MAJ, 1, HARMONY_APPLIED_DIMINISHED_TRIAD, "viio/ii");
    expect_analysis(MAJ, 13, HARMONY_APPLIED_DIMINISHED_TRIAD, "viio/ii");
    expect_analysis(MAJ, -10, HARMONY_APPLIED_MAJOR_TRIAD, "V/V");
}

static void test_analyzing_a_chromatic_chord_in_minor(void) {
    expect_analysis(MIN, 10, HARMONY_APPLIED_DOMINANT_SEVENTH, "V7/III");
    expect_analysis(MIN, 2, HARMONY_APPLIED_DOMINANT_SEVENTH, "V7/V");
    expect_analysis(MIN, 0, HARMONY_APPLIED_DOMINANT_SEVENTH, "V7/iv");
    expect_analysis(MIN, 3, HARMONY_APPLIED_DOMINANT_SEVENTH, "V7/VI");
    expect_analysis(MIN, 5, HARMONY_APPLIED_DOMINANT_SEVENTH, "V7/VII");
    expect_analysis(MIN, 6, HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH, "viio7/V");
}

static void test_a_root_that_tonicizes_nothing(void) {
    HarmonyAppliedChord chord = {SD, HARMONY_APPLIED_MAJOR_TRIAD, 5};
    HarmonyAppliedChord before = chord;
    assert(!harmony_applied_analyze(MAJ, 7, HARMONY_APPLIED_DOMINANT_SEVENTH, &chord)); /* V of I is V */
    assert(!harmony_applied_analyze(MAJ, 11, HARMONY_APPLIED_DIMINISHED_TRIAD, &chord)); /* vii of I */
    assert(!harmony_applied_analyze(MAJ, 10, HARMONY_APPLIED_DIMINISHED_TRIAD, &chord)); /* vii diminished */
    assert(!harmony_applied_analyze(MAJ, 3, HARMONY_APPLIED_DOMINANT_SEVENTH, &chord));  /* no such degree */
    assert(!harmony_applied_analyze(MIN, 1, HARMONY_APPLIED_DIMINISHED_TRIAD, &chord));  /* ii diminished */
    assert(memcmp(&chord, &before, sizeof(chord)) == 0);
    assert(!harmony_applied_analyze(MAJ, 2, HARMONY_APPLIED_QUALITY_COUNT, &chord));
    assert(!harmony_applied_analyze(HARMONY_MODE_COUNT, 2, HARMONY_APPLIED_MAJOR_TRIAD, &chord));
    assert(!harmony_applied_analyze(MAJ, 2, HARMONY_APPLIED_MAJOR_TRIAD, NULL));
}

static void test_the_spelling_of_an_applied_chord(void) {
    HarmonyAppliedChord v7_of_v = applied(SD, HARMONY_APPLIED_DOMINANT_SEVENTH, 5);
    HarmonyAppliedChord lt7_of_v = applied(SLT, HARMONY_APPLIED_FULLY_DIMINISHED_SEVENTH, 5);
    HarmonyAppliedChord v_of_iv = applied(SD, HARMONY_APPLIED_DOMINANT_SEVENTH, 4);
    HarmonyAppliedChord v_of_vi = applied(SD, HARMONY_APPLIED_MAJOR_TRIAD, 6);
    HarmonyAppliedChord lt_of_ii = applied(SLT, HARMONY_APPLIED_DIMINISHED_TRIAD, 2);
    assert(harmony_applied_root_degree(MAJ, &v7_of_v) == 2 && harmony_applied_root_semitone(MAJ, &v7_of_v) == 2);
    assert(harmony_applied_root_degree(MAJ, &lt7_of_v) == 4 && harmony_applied_root_semitone(MAJ, &lt7_of_v) == 6);
    assert(harmony_applied_root_degree(MAJ, &v_of_iv) == 1 && harmony_applied_root_semitone(MAJ, &v_of_iv) == 0);
    assert(harmony_applied_root_degree(MAJ, &v_of_vi) == 3 && harmony_applied_root_semitone(MAJ, &v_of_vi) == 4);
    assert(harmony_applied_root_degree(MAJ, &lt_of_ii) == 1 && harmony_applied_root_semitone(MAJ, &lt_of_ii) == 1);
    HarmonyAppliedChord v_of_iii_minor = applied(SD, HARMONY_APPLIED_DOMINANT_SEVENTH, 3);
    assert(harmony_applied_root_degree(MIN, &v_of_iii_minor) == 7 &&
           harmony_applied_root_semitone(MIN, &v_of_iii_minor) == 10);
    HarmonyAppliedChord invalid = applied(SD, HARMONY_APPLIED_DOMINANT_SEVENTH, 1);
    assert(harmony_applied_root_degree(MAJ, &invalid) == -1 && harmony_applied_root_semitone(MAJ, &invalid) == -1);
    assert(harmony_applied_root_degree(MAJ, NULL) == -1 && harmony_applied_root_semitone(MAJ, NULL) == -1);
}

static void test_analysis_and_spelling_agree(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        for (int quality = 0; quality < HARMONY_APPLIED_QUALITY_COUNT; quality++) {
            AppliedChordKind kind;
            assert(harmony_applied_quality_kind((HarmonyAppliedQuality)quality, &kind));
            for (int target = 1; target <= 7; target++) {
                HarmonyAppliedChord chord = {kind, (HarmonyAppliedQuality)quality, target};
                if (!harmony_applied_valid((HarmonyMode)mode, &chord)) {
                    continue;
                }
                HarmonyAppliedChord found;
                int root = harmony_applied_root_semitone((HarmonyMode)mode, &chord);
                assert(harmony_applied_analyze((HarmonyMode)mode, root, chord.quality, &found));
                assert(found.kind == chord.kind && found.quality == chord.quality && found.target == chord.target);
            }
        }
    }
}

static void test_secondary_dominants_as_altered_diatonic_chords(void) {
    HarmonyAppliedChord found;
    const int major[][2] = {{1, 4}, {2, 5}, {3, 6}, {6, 2}, {7, 3}};
    for (size_t i = 0; i < COUNT_OF(major); i++) {
        assert(harmony_applied_from_diatonic_root(MAJ, major[i][0], &found));
        assert(found.kind == SD && found.quality == HARMONY_APPLIED_DOMINANT_SEVENTH && found.target == major[i][1]);
    }
    const int minor[][2] = {{1, 4}, {2, 5}, {3, 6}, {4, 7}, {7, 3}};
    for (size_t i = 0; i < COUNT_OF(minor); i++) {
        assert(harmony_applied_from_diatonic_root(MIN, minor[i][0], &found));
        assert(found.target == minor[i][1]);
    }
    assert(!harmony_applied_from_diatonic_root(MAJ, 5, &found)); /* V would tonicize the tonic */
    assert(!harmony_applied_from_diatonic_root(MAJ, 4, &found)); /* IV would tonicize vii diminished */
    assert(!harmony_applied_from_diatonic_root(MIN, 5, &found));
    assert(!harmony_applied_from_diatonic_root(MIN, 6, &found)); /* VI would tonicize ii diminished */
    assert(!harmony_applied_from_diatonic_root(MAJ, 0, &found));
    assert(!harmony_applied_from_diatonic_root(MAJ, 8, &found));
    assert(!harmony_applied_from_diatonic_root(HARMONY_MODE_COUNT, 2, &found));
    assert(!harmony_applied_from_diatonic_root(MAJ, 2, NULL));
}

static void test_an_altered_diatonic_chord_has_the_root_of_its_applied_chord(void) {
    for (int mode = 0; mode < HARMONY_MODE_COUNT; mode++) {
        for (int degree = 1; degree <= 7; degree++) {
            HarmonyAppliedChord chord;
            if (harmony_applied_from_diatonic_root((HarmonyMode)mode, degree, &chord)) {
                assert(harmony_applied_root_semitone((HarmonyMode)mode, &chord) ==
                       harmony_applied_scale_semitone((HarmonyMode)mode, degree));
            }
        }
    }
}

int main(void) {
    test_qualities();
    test_a_quality_belongs_to_a_kind();
    test_the_targets_that_can_be_tonicized();
    test_a_valid_applied_chord();
    test_labels();
    test_label_buffers_and_invalid_input();
    test_the_scale_semitones();
    test_analyzing_a_chromatic_chord_in_major();
    test_analyzing_a_chromatic_chord_in_minor();
    test_a_root_that_tonicizes_nothing();
    test_the_spelling_of_an_applied_chord();
    test_analysis_and_spelling_agree();
    test_secondary_dominants_as_altered_diatonic_chords();
    test_an_altered_diatonic_chord_has_the_root_of_its_applied_chord();
    printf("test_harmony_applied: all tests passed\n");
    return 0;
}
