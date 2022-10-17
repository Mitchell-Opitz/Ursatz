#ifndef URSATZ_STANDARD_HARMONY_TESTS_HARMONY_DECEPTIVE_TEST_UTIL_H
#define URSATZ_STANDARD_HARMONY_TESTS_HARMONY_DECEPTIVE_TEST_UTIL_H

#include <string.h>

#include "harmony_deceptive_rules.h"
#include "harmony_test_util.h"

/* Worked deceptive motions in four voices, from the soprano to the bass, as diatonic steps (do
   is 0, re is 1, ti below do is -1, an octave is 7). Each is written to break no hard rule. */

static inline HarmonyDeceptiveMotion make_deceptive(HarmonyMode mode, HarmonyChord a, HarmonyChord b, const int s0[4],
                                                    const int s1[4]) {
    HarmonyDeceptiveMotion m;
    memset(&m, 0, sizeof(m));
    m.mode = mode;
    m.chords[0] = a;
    m.chords[1] = b;
    memcpy(m.steps[0], s0, sizeof(m.steps[0]));
    memcpy(m.steps[1], s1, sizeof(m.steps[1]));
    return m;
}

/* V vi over sol la: ti goes up to do and the other upper voices go down (sol to mi, re to do). */
static inline HarmonyDeceptiveMotion example_deceptive_vi(HarmonyMode mode) {
    static const int s0[4] = {6, 4, 1, -3};
    static const int s1[4] = {7, 2, 0, -2};
    return make_deceptive(mode, CH(5), CH(6), s0, s1);
}

/* V7 vi: the same, with fa in the alto going down to mi. */
static inline HarmonyDeceptiveMotion example_deceptive_v7_vi(HarmonyMode mode) {
    static const int s0[4] = {6, 3, 1, -3};
    static const int s1[4] = {7, 2, 0, -2};
    return make_deceptive(mode, CH7(5), CH(6), s0, s1);
}

/* V IV6 over sol la: the less common destination, which breaks only the soft rule. */
static inline HarmonyDeceptiveMotion example_deceptive_iv6(HarmonyMode mode) {
    static const int s0[4] = {6, 4, 1, -3};
    static const int s1[4] = {7, 3, 0, -2};
    return make_deceptive(mode, CH(5), CHI(4, 1), s0, s1);
}

#endif
