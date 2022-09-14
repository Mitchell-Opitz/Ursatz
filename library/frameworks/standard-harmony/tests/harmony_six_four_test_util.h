#ifndef URSATZ_STANDARD_HARMONY_TESTS_HARMONY_SIX_FOUR_TEST_UTIL_H
#define URSATZ_STANDARD_HARMONY_TESTS_HARMONY_SIX_FOUR_TEST_UTIL_H

#include <string.h>

#include "harmony_six_four.h"
#include "harmony_test_util.h"

/* Worked 6/4 chords in four voices, from the soprano to the bass, as diatonic steps (do is 0,
   re is 1, ti below do is -1, an octave is 7). Each is written to break no rule. */

static inline HarmonySixFour make_six_four(HarmonyMode mode, HarmonyChord a, HarmonyChord b, HarmonyChord c,
                                          const int s0[4], const int s1[4], const int s2[4]) {
    HarmonySixFour s;
    memset(&s, 0, sizeof(s));
    s.mode = mode;
    s.chords[0] = a;
    s.chords[1] = b;
    s.chords[2] = c;
    memcpy(s.steps[0], s0, sizeof(s.steps[0]));
    memcpy(s.steps[1], s1, sizeof(s.steps[1]));
    memcpy(s.steps[2], s2, sizeof(s.steps[2]));
    s.beat[0] = BEAT_STRENGTH_STRONG;
    s.beat[1] = BEAT_STRENGTH_WEAK;
    s.beat[2] = BEAT_STRENGTH_STRONG;
    return s;
}

/* I V6/4 I6 over do re mi: the V6/4 on the passing re, between two tonic chords. */
static inline HarmonySixFour example_passing_tonic(HarmonyMode mode) {
    static const int s0[4] = {7, 2, -3, -7};
    static const int s1[4] = {6, 1, -3, -6};
    static const int s2[4] = {7, 0, -3, -5};
    return make_six_four(mode, CH(1), CHI(5, 2), CHI(1, 1), s0, s1, s2);
}

/* The same backward: I6 V6/4 I over mi re do. */
static inline HarmonySixFour example_passing_backward(HarmonyMode mode) {
    static const int s0[4] = {7, 0, -3, -5};
    static const int s1[4] = {6, 1, -3, -6};
    static const int s2[4] = {7, 2, -3, -7};
    return make_six_four(mode, CHI(1, 1), CHI(5, 2), CH(1), s0, s1, s2);
}

/* IV vi6/4 ii over fa mi re: the passing mi, between two predominants. */
static inline HarmonySixFour example_passing_predominant(HarmonyMode mode) {
    static const int s0[4] = {3, 5, 0, -4};
    static const int s1[4] = {2, 5, 0, -5};
    static const int s2[4] = {1, 5, 1, -6};
    return make_six_four(mode, CH(4), CHI(6, 2), CH(2), s0, s1, s2);
}

/* I IV6/4 I over do do do: the upper neighbors in the soprano and alto, the tenor static. */
static inline HarmonySixFour example_neighbor_tonic(HarmonyMode mode) {
    static const int s0[4] = {4, 2, 0, -7};
    static const int s1[4] = {5, 3, 0, -7};
    static const int s2[4] = {4, 2, 0, -7};
    return make_six_four(mode, CH(1), CHI(4, 2), CH(1), s0, s1, s2);
}

/* The same with the soprano static: the neighbors are in the alto and the tenor. */
static inline HarmonySixFour example_neighbor_static_soprano(HarmonyMode mode) {
    static const int s0[4] = {7, 2, -3, -7};
    static const int s1[4] = {7, 3, -2, -7};
    static const int s2[4] = {7, 2, -3, -7};
    return make_six_four(mode, CH(1), CHI(4, 2), CH(1), s0, s1, s2);
}

/* V I6/4 V over sol sol sol. */
static inline HarmonySixFour example_neighbor_dominant(HarmonyMode mode) {
    static const int s0[4] = {1, -1, -3, -10};
    static const int s1[4] = {2, 0, -3, -10};
    static const int s2[4] = {1, -1, -3, -10};
    return make_six_four(mode, CH(5), CHI(1, 2), CH(5), s0, s1, s2);
}

/* I I6/4 I: the bass leaps from do to its fifth, sol, while the upper voices sustain the
   chord. */
static inline HarmonySixFour example_arpeggiating(HarmonyMode mode) {
    static const int s0[4] = {4, 2, 0, -7};
    static const int s1[4] = {4, 2, 0, -3};
    static const int s2[4] = {4, 2, 0, -7};
    return make_six_four(mode, CH(1), CHI(1, 2), CH(1), s0, s1, s2);
}

/* IV, the cadential 6/4, V over fa sol sol. */
static inline HarmonySixFour example_cadential(HarmonyMode mode) {
    static const int s0[4] = {3, 2, 0, -11};
    static const int s1[4] = {2, 0, -3, -10};
    static const int s2[4] = {1, -1, -3, -10};
    return make_six_four(mode, CH(4), CADENTIAL_CHORD, CH(5), s0, s1, s2);
}

#endif
