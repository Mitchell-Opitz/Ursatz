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

/* The leading-tone chord in the middle, in place of the V with the same bass note. In minor
   it is built on ti (the leading_tone flag). */
static inline HarmonyChord vii_chord(HarmonyMode mode, int inversion, bool seventh) {
    return (HarmonyChord){7, inversion, mode == HARMONY_MODE_MINOR, seventh};
}

/* do ti do: I vii7 I. The seventh is le, a fully diminished chord; le falls to sol. */
static inline HarmonyTonicProlongation example_vii7(HarmonyMode mode) {
    static const int s0[4] = {4, 2, 0, -7};
    static const int s1[4] = {3, 1, -2, -8};
    static const int s2[4] = {2, 0, -3, -7};
    return make_prolongation(mode, I_ROOT, vii_chord(mode, 0, true), I_ROOT, s0, s1, s2);
}

/* do re mi: I vii6 I6, the triad in first inversion, re doubled. */
static inline HarmonyTonicProlongation example_vii6_triad(HarmonyMode mode) {
    static const int s0[4] = {4, 2, 0, -7};
    static const int s1[4] = {3, 1, -1, -6};
    static const int s2[4] = {2, 0, 0, -5};
    return make_prolongation(mode, I_ROOT, vii_chord(mode, 1, false), I_FIRST, s0, s1, s2);
}

/* do re mi: I vii6/5 I6. re (bass) and le make a diminished fifth, then do re resolves. */
static inline HarmonyTonicProlongation example_vii65(HarmonyMode mode) {
    static const int s0[4] = {4, 2, 0, -7};
    static const int s1[4] = {5, 3, -1, -6};
    static const int s2[4] = {4, 2, 0, -5};
    return make_prolongation(mode, I_ROOT, vii_chord(mode, 1, true), I_FIRST, s0, s1, s2);
}

/* do fa mi: I vii4/3 I6. */
static inline HarmonyTonicProlongation example_vii43(HarmonyMode mode) {
    static const int s0[4] = {4, 2, 0, -7};
    static const int s1[4] = {5, 1, -1, -4};
    static const int s2[4] = {4, 0, 0, -5};
    return make_prolongation(mode, I_ROOT, vii_chord(mode, 2, true), I_FIRST, s0, s1, s2);
}

/* The same notes as example_vii65, but the half-diminished chord (la, not le): re in the
   bass and la make a perfect fifth, and parallel fifths follow. */
static inline HarmonyTonicProlongation example_half_diminished_65(void) {
    HarmonyTonicProlongation p = example_vii65(HARMONY_MODE_MAJOR);
    p.chords[1] = (HarmonyChord){7, 1, false, true, false, true};
    return p;
}

/* do ti do with the half-diminished chord, re above la: no parallel fifths. */
static inline HarmonyTonicProlongation example_half_diminished_7(void) {
    static const int s0[4] = {7, 4, 2, -7};
    static const int s1[4] = {8, 5, 3, -8};
    static const int s2[4] = {7, 4, 2, -7};
    return make_prolongation(HARMONY_MODE_MAJOR, I_ROOT, (HarmonyChord){7, 0, false, true, false, true}, I_ROOT, s0, s1,
                             s2);
}

#endif
