#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "harmony_prolongation.h"
#include "harmony_test_util.h"

#define DO_TI_DO HARMONY_PROLONGATION_DO_TI_DO
#define DO_RE_MI HARMONY_PROLONGATION_DO_RE_MI
#define DO_RE_DO HARMONY_PROLONGATION_DO_RE_DO
#define DO_FA_MI HARMONY_PROLONGATION_DO_FA_MI

#define I CH(1)
#define I6 CHI(1, 1)
#define V6 CHI(5, 1)  /* the triad in first inversion */
#define V65 CH7I(5, 1)
#define V43 CH7I(5, 2)
#define V42 CH7I(5, 3)

static bool line_of(HarmonyChord a, HarmonyChord b, HarmonyChord c, HarmonyProlongationLine *line) {
    HarmonyChord chords[3] = {a, b, c};
    return harmony_prolongation_of(chords, line);
}

static HarmonyProlongationLine classify(HarmonyChord a, HarmonyChord b, HarmonyChord c) {
    HarmonyProlongationLine line = HARMONY_PROLONGATION_LINE_COUNT;
    assert(line_of(a, b, c, &line));
    return line;
}

static bool classifies(HarmonyChord a, HarmonyChord b, HarmonyChord c) {
    HarmonyProlongationLine line = DO_FA_MI;
    return line_of(a, b, c, &line);
}

static void test_validity_and_names(void) {
    for (int i = 0; i < HARMONY_PROLONGATION_LINE_COUNT; i++) {
        assert(harmony_prolongation_line_valid((HarmonyProlongationLine)i));
    }
    assert(!harmony_prolongation_line_valid(HARMONY_PROLONGATION_LINE_COUNT));
    assert(!harmony_prolongation_line_valid((HarmonyProlongationLine)-1));
    assert(strcmp(harmony_prolongation_line_name(DO_TI_DO), "do-ti-do") == 0);
    assert(strcmp(harmony_prolongation_line_name(DO_RE_MI), "do-re-mi") == 0);
    assert(strcmp(harmony_prolongation_line_name(DO_RE_DO), "do-re-do") == 0);
    assert(strcmp(harmony_prolongation_line_name(DO_FA_MI), "do-fa-mi") == 0);
    assert(harmony_prolongation_line_name(HARMONY_PROLONGATION_LINE_COUNT) == NULL);
}

static void test_bass_lines(void) {
    int bass[3] = {0, 0, 0};
    assert(harmony_prolongation_line_bass(DO_TI_DO, bass) && bass[0] == 1 && bass[1] == 7 && bass[2] == 1);
    assert(harmony_prolongation_line_bass(DO_RE_MI, bass) && bass[0] == 1 && bass[1] == 2 && bass[2] == 3);
    assert(harmony_prolongation_line_bass(DO_RE_DO, bass) && bass[0] == 1 && bass[1] == 2 && bass[2] == 1);
    assert(harmony_prolongation_line_bass(DO_FA_MI, bass) && bass[0] == 1 && bass[1] == 4 && bass[2] == 3);
    bass[0] = 99;
    assert(!harmony_prolongation_line_bass(HARMONY_PROLONGATION_LINE_COUNT, bass));
    assert(bass[0] == 99);
    assert(!harmony_prolongation_line_bass(DO_TI_DO, NULL));
}

static void test_the_four_progressions(void) {
    assert(classify(I, V65, I) == DO_TI_DO);
    assert(classify(I, V6, I) == DO_TI_DO); /* V6, the triad, is also possible */
    assert(classify(I, V43, I6) == DO_RE_MI);
    assert(classify(I, V43, I) == DO_RE_DO);
    assert(classify(I, V42, I6) == DO_FA_MI);
}

static void test_other_progressions_are_not_classified(void) {
    /* V4/2 does not go to root position I, and V6/5 does not go to I6. */
    assert(!classifies(I, V42, I));
    assert(!classifies(I, V65, I6));
    assert(!classifies(I, V6, I6));
    /* A V in root position, or the triad in second inversion, is not an inverted V7. */
    assert(!classifies(I, CH(5), I));
    assert(!classifies(I, CH7(5), I));
    assert(!classifies(I, CHI(5, 2), I6));
    assert(!classifies(I, CHI(5, 2), I));
    /* The first chord is root position I, and the last I or I6. */
    assert(!classifies(I6, V43, I6));
    assert(!classifies(I6, V65, I));
    assert(!classifies(CH(4), V65, I));
    assert(!classifies(I, V65, CH(4)));
    assert(!classifies(I, V65, CHI(1, 2)));
    /* A seventh chord on the tonic, or the cadential 6/4, is not the tonic here. */
    assert(!classifies(CH7(1), V65, I));
    assert(!classifies(I, V65, CH7(1)));
    assert(!classifies(CADENTIAL_CHORD, V65, I));
    /* The middle chord is V. */
    assert(!classifies(I, CH7I(4, 1), I));
    assert(!classifies(I, CHI(2, 1), I));
}

static void test_invalid_inputs(void) {
    HarmonyProlongationLine line = DO_FA_MI;
    HarmonyChord bad[3] = {I, CH(9), I};
    HarmonyChord ok[3] = {I, V65, I};
    assert(!harmony_prolongation_of(bad, &line));
    assert(!harmony_prolongation_of(NULL, &line));
    assert(!harmony_prolongation_of(ok, NULL));
    assert(line == DO_FA_MI);
}

int main(void) {
    test_validity_and_names();
    test_bass_lines();
    test_the_four_progressions();
    test_other_progressions_are_not_classified();
    test_invalid_inputs();
    printf("test_harmony_prolongation: ok\n");
    return 0;
}
