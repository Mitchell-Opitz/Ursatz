#ifndef URSATZ_STANDARD_HARMONY_TESTS_HARMONY_IV6_PROLONGATION_TEST_UTIL_H
#define URSATZ_STANDARD_HARMONY_TESTS_HARMONY_IV6_PROLONGATION_TEST_UTIL_H

#include <string.h>

#include "harmony_iv6_prolongation_rules.h"
#include "harmony_test_util.h"

/* Worked tonic prolongations with IV6 as a predominant in four voices, from the soprano to the
   bass, as diatonic steps (do is 0, re is 1, ti below do is -1, an octave is 7). */

static inline HarmonyIv6Prolongation make_iv6_prolongation(HarmonyMode mode, HarmonyChord dominant,
                                                           const int s0[4], const int s1[4], const int s2[4],
                                                           const int s3[4]) {
    HarmonyIv6Prolongation p;
    memset(&p, 0, sizeof(p));
    p.mode = mode;
    p.chords[0] = CH(1);
    p.chords[1] = CHI(4, 1);
    p.chords[2] = dominant;
    p.chords[3] = CH(1);
    memcpy(p.steps[0], s0, sizeof(p.steps[0]));
    memcpy(p.steps[1], s1, sizeof(p.steps[1]));
    memcpy(p.steps[2], s2, sizeof(p.steps[2]));
    memcpy(p.steps[3], s3, sizeof(p.steps[3]));
    return p;
}

/* I IV6 V6 I over do la ti do, the soprano do do re do. Breaks no rule in a major key. */
static inline HarmonyIv6Prolongation example_iv6_v6(HarmonyMode mode) {
    static const int s0[4] = {7, 4, 2, -7};
    static const int s1[4] = {7, 5, 3, -9};
    static const int s2[4] = {8, 4, 1, -8};
    static const int s3[4] = {7, 4, 2, -7};
    return make_iv6_prolongation(mode, CHI(5, 1), s0, s1, s2, s3);
}

/* I IV6 V I over do la sol do: the same line with the root position V, which breaks only the
   soft rule that V6 is preferred. */
static inline HarmonyIv6Prolongation example_iv6_v(HarmonyMode mode) {
    static const int s0[4] = {7, 4, 2, -7};
    static const int s1[4] = {7, 5, 3, -9};
    static const int s2[4] = {8, 6, 1, -10};
    static const int s3[4] = {7, 7, 2, -7};
    return make_iv6_prolongation(mode, CH(5), s0, s1, s2, s3);
}

#endif
