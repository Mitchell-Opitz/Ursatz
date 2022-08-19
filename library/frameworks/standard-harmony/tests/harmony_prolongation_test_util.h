#ifndef URSATZ_STANDARD_HARMONY_TESTS_HARMONY_PROLONGATION_TEST_UTIL_H
#define URSATZ_STANDARD_HARMONY_TESTS_HARMONY_PROLONGATION_TEST_UTIL_H

#include <string.h>

#include "harmony_prolongation_rules.h"
#include "harmony_test_util.h"

/* Worked tonic prolongations in four voices, from the soprano to the bass, as diatonic
   steps (do is 0, re is 1, ti below do is -1, an octave is 7). Each is written to
   break no rule: strong, weak, strong beats; V has the leading tone. */

#define I_ROOT CH(1)
#define I_FIRST CHI(1, 1)
#define V6_TRIAD CHI(5, 1)
#define V65 CH7I(5, 1)
#define V43 CH7I(5, 2)
#define V42 CH7I(5, 3)

static inline HarmonyTonicProlongation make_prolongation(HarmonyMode mode, HarmonyChord a, HarmonyChord b,
                                                         HarmonyChord c, const int s0[4], const int s1[4],
                                                         const int s2[4]) {
    HarmonyTonicProlongation p;
    memset(&p, 0, sizeof(p));
    p.mode = mode;
    p.chords[0] = a;
    p.chords[1] = b;
    p.chords[2] = c;
    memcpy(p.steps[0], s0, sizeof(p.steps[0]));
    memcpy(p.steps[1], s1, sizeof(p.steps[1]));
    memcpy(p.steps[2], s2, sizeof(p.steps[2]));
    p.beat[0] = BEAT_STRENGTH_STRONG;
    p.beat[1] = BEAT_STRENGTH_WEAK;
    p.beat[2] = BEAT_STRENGTH_STRONG;
    p.dominant_has_leading_tone = true;
    return p;
}

/* do ti do: I V6/5 I. The bass falls to ti and comes back; fa to mi, re to do. */
static inline HarmonyTonicProlongation example_v65(HarmonyMode mode) {
    static const int s0[4] = {4, 2, 0, -7};
    static const int s1[4] = {3, 1, -3, -8};
    static const int s2[4] = {2, 0, -3, -7};
    return make_prolongation(mode, I_ROOT, V65, I_ROOT, s0, s1, s2);
}

/* do ti do with the V6 triad: re goes up to mi. */
static inline HarmonyTonicProlongation example_v6_triad(HarmonyMode mode) {
    static const int s0[4] = {2, 0, -3, -7};
    static const int s1[4] = {1, 1, -3, -8};
    static const int s2[4] = {2, 0, -3, -7};
    return make_prolongation(mode, I_ROOT, V6_TRIAD, I_ROOT, s0, s1, s2);
}

/* do re mi: I V4/3 I6. The bass re goes up to mi. */
static inline HarmonyTonicProlongation example_v43_to_first(HarmonyMode mode) {
    static const int s0[4] = {4, 2, 0, -7};
    static const int s1[4] = {4, 3, -1, -6};
    static const int s2[4] = {4, 2, 0, -5};
    return make_prolongation(mode, I_ROOT, V43, I_FIRST, s0, s1, s2);
}

/* do re do: I V4/3 I. The bass re goes down to do. */
static inline HarmonyTonicProlongation example_v43_to_root(HarmonyMode mode) {
    static const int s0[4] = {4, 2, 0, -7};
    static const int s1[4] = {4, 3, -1, -6};
    static const int s2[4] = {4, 2, 0, -7};
    return make_prolongation(mode, I_ROOT, V43, I_ROOT, s0, s1, s2);
}

/* do fa mi: I V4/2 I6. The bass fa, the chordal seventh, goes down to mi. */
static inline HarmonyTonicProlongation example_v42(HarmonyMode mode) {
    static const int s0[4] = {4, 2, 0, -7};
    static const int s1[4] = {4, 1, -1, -4};
    static const int s2[4] = {4, 0, 0, -5};
    return make_prolongation(mode, I_ROOT, V42, I_FIRST, s0, s1, s2);
}

/* do re mi again, the soprano taking mi fa sol in parallel tenths with the bass: fa goes
   up to sol, the exception to the resolution of the chordal seventh. */
static inline HarmonyTonicProlongation example_tenths(HarmonyMode mode) {
    static const int s0[4] = {2, -3, 0, -7};
    static const int s1[4] = {3, -3, -1, -6};
    static const int s2[4] = {4, -3, 0, -5};
    return make_prolongation(mode, I_ROOT, V43, I_FIRST, s0, s1, s2);
}

#endif
