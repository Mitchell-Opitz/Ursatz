#ifndef URSATZ_STANDARD_HARMONY_TESTS_HARMONY_PLAGAL_TEST_UTIL_H
#define URSATZ_STANDARD_HARMONY_TESTS_HARMONY_PLAGAL_TEST_UTIL_H

#include <string.h>

#include "harmony_plagal_rules.h"
#include "harmony_test_util.h"

/* Worked plagal motions in four voices, from the soprano to the bass, as diatonic steps (do is
   0, re is 1, an octave is 7). The two after a cadence break no rule; the phrase beginning
   ones break only the soft rule that (IV) is the less common choice there. */

static inline HarmonyPlagalMotion make_plagal(HarmonyMode mode, HarmonyPlagalPlacement placement, HarmonyChord a,
                                              HarmonyChord b, const int s0[4], const int s1[4]) {
    HarmonyPlagalMotion m;
    memset(&m, 0, sizeof(m));
    m.mode = mode;
    m.placement = placement;
    m.chords[0] = a;
    m.chords[1] = b;
    memcpy(m.steps[0], s0, sizeof(m.steps[0]));
    memcpy(m.steps[1], s1, sizeof(m.steps[1]));
    return m;
}

/* IV I over fa do, after a PAC: both complete, the bass doubled, every upper voice by step or
   common tone, do in the soprano (la sol, fa mi in the alto and tenor). */
static inline HarmonyPlagalMotion example_plagal_after_cadence(HarmonyMode mode) {
    static const int s0[4] = {7, 5, 3, -4};
    static const int s1[4] = {7, 4, 2, -7};
    return make_plagal(mode, HARMONY_PLAGAL_AFTER_CADENCE, CH(4), CH(1), s0, s1);
}

/* The same with the melody in the alto and la sol in the soprano: do is not in the soprano. */
static inline HarmonyPlagalMotion example_plagal_soprano_la(HarmonyMode mode) {
    static const int s0[4] = {5, 7, 3, -4};
    static const int s1[4] = {4, 7, 2, -7};
    return make_plagal(mode, HARMONY_PLAGAL_AFTER_CADENCE, CH(4), CH(1), s0, s1);
}

/* The root position version at a phrase beginning: the same voices. */
static inline HarmonyPlagalMotion example_plagal_beginning_root(HarmonyMode mode) {
    HarmonyPlagalMotion m = example_plagal_after_cadence(mode);
    m.placement = HARMONY_PLAGAL_PHRASE_BEGINNING;
    return m;
}

/* IV I6 over fa mi at a phrase beginning: the tenor leaps from fa to do, so it does not
   follow the bass down to mi. */
static inline HarmonyPlagalMotion example_plagal_beginning_inverted(HarmonyMode mode) {
    static const int s0[4] = {7, 5, 3, -4};
    static const int s1[4] = {7, 4, 0, -5};
    return make_plagal(mode, HARMONY_PLAGAL_PHRASE_BEGINNING, CH(4), CHI(1, 1), s0, s1);
}

/* IV I6 with the tenor on mi instead: it follows the bass from fa to mi, parallel octaves. */
static inline HarmonyPlagalMotion example_plagal_parallel_octaves(HarmonyMode mode) {
    static const int s0[4] = {7, 5, 3, -4};
    static const int s1[4] = {7, 4, 2, -5};
    return make_plagal(mode, HARMONY_PLAGAL_PHRASE_BEGINNING, CH(4), CHI(1, 1), s0, s1);
}

#endif
