#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/harmony/harmony_applied.h"
#include "theory/tests/harmony/harmony_test_util.h"

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
    for (int i = 0; i < HARMONY_APPLIED_QUALITY_COUNT; i++) {
        assert(harmony_applied_quality_valid((HarmonyAppliedQuality)i));
    }
    assert(!harmony_applied_quality_valid(HARMONY_APPLIED_QUALITY_COUNT));
    assert(!harmony_applied_quality_valid((HarmonyAppliedQuality)-1));
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

int main(void) {
    test_qualities();
    test_a_quality_belongs_to_a_kind();
    test_the_targets_that_can_be_tonicized();
    test_a_valid_applied_chord();
    test_labels();
    test_label_buffers_and_invalid_input();
    test_the_scale_semitones();
    test_the_spelling_of_an_applied_chord();
    printf("test_harmony_applied: all tests passed\n");
    return 0;
}
