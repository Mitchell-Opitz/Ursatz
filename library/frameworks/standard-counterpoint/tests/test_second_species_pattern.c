#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "second_species_pattern.h"
#include "second_species_test_util.h"

/* A two-bar exercise: the cantus firmus notes c0 and c1, and the counterpoint
   d1 and w in the first bar, d2 tied through the second. Classifies bar 0. */
static bool classify(const PitchSpelling *c0, const PitchSpelling *c1, const PitchSpelling *d1,
                     const PitchSpelling *w, const PitchSpelling *d2, StandardCounterpointWeakBeatPattern *pattern) {
    const PitchSpelling *cantus[] = {c0, c1};
    const StandardCounterpointSlot slots[] = {ATK(d1), ATK(w), ATK(d2), TIE(d2)};
    EXERCISE(exercise, cantus, slots, CANTUS_LOWER);
    return standard_counterpoint_second_species_weak_beat_pattern(&exercise, 0, pattern);
}

static StandardCounterpointWeakBeatPattern pattern_of(const PitchSpelling *c0, const PitchSpelling *c1,
                                                      const PitchSpelling *d1, const PitchSpelling *w,
                                                      const PitchSpelling *d2) {
    StandardCounterpointWeakBeatPattern pattern;
    assert(classify(c0, c1, d1, w, d2, &pattern));
    return pattern;
}

static bool has_no_pattern(const PitchSpelling *c0, const PitchSpelling *c1, const PitchSpelling *d1,
                           const PitchSpelling *w, const PitchSpelling *d2) {
    StandardCounterpointWeakBeatPattern pattern = STANDARD_COUNTERPOINT_WEAK_BEAT_PATTERN_COUNT;
    bool matched = classify(c0, c1, d1, w, d2, &pattern);
    assert(matched || pattern == STANDARD_COUNTERPOINT_WEAK_BEAT_PATTERN_COUNT);
    return !matched;
}

static void test_names(void) {
    assert(strcmp(standard_counterpoint_weak_beat_pattern_name(STANDARD_COUNTERPOINT_WEAK_BEAT_CONSONANT_PASSING_TONE),
                  "consonant-passing-tone") == 0);
    assert(strcmp(standard_counterpoint_weak_beat_pattern_name(
                      STANDARD_COUNTERPOINT_WEAK_BEAT_DELAY_OF_MELODIC_PROGRESSION),
                  "delay-of-melodic-progression") == 0);
    assert(strcmp(standard_counterpoint_weak_beat_pattern_name(STANDARD_COUNTERPOINT_WEAK_BEAT_DISSONANT_PASSING_TONE),
                  "dissonant-passing-tone") == 0);
    assert(standard_counterpoint_weak_beat_pattern_name(STANDARD_COUNTERPOINT_WEAK_BEAT_PATTERN_COUNT) == NULL);
    for (int i = 0; i < STANDARD_COUNTERPOINT_WEAK_BEAT_PATTERN_COUNT; i++) {
        bool dissonant = standard_counterpoint_weak_beat_pattern_is_dissonant((StandardCounterpointWeakBeatPattern)i);
        assert(dissonant == (i == STANDARD_COUNTERPOINT_WEAK_BEAT_DISSONANT_PASSING_TONE));
    }
}

static void test_consonant_passing_tone(void) {
    assert(pattern_of(P(C, 0, 3), P(E, 0, 3), P(G, 0, 3), P(A, 0, 3), P(B, 0, 3)) ==
           STANDARD_COUNTERPOINT_WEAK_BEAT_CONSONANT_PASSING_TONE);
    /* Downward, too. */
    assert(pattern_of(P(C, 0, 3), P(D, 0, 3), P(A, 0, 3), P(G, 0, 3), P(F, 0, 3)) ==
           STANDARD_COUNTERPOINT_WEAK_BEAT_CONSONANT_PASSING_TONE);
}

static void test_dissonant_passing_tone(void) {
    assert(pattern_of(P(C, 0, 3), P(E, 0, 3), P(E, 0, 3), P(F, 0, 3), P(G, 0, 3)) ==
           STANDARD_COUNTERPOINT_WEAK_BEAT_DISSONANT_PASSING_TONE);
    /* A dissonance entered by leap is no pattern. */
    assert(has_no_pattern(P(C, 0, 3), P(F, 0, 3), P(E, 0, 3), P(B, 0, 3), P(A, 0, 3)));
    /* The passing motion must end on a consonant downbeat. */
    assert(has_no_pattern(P(C, 0, 3), P(C, 0, 3), P(G, 0, 3), P(A, 0, 3), P(B, 0, 3)));
}

static void test_substitution(void) {
    assert(pattern_of(P(C, 0, 3), P(E, 0, 3), P(E, 0, 3), P(A, 0, 3), P(G, 0, 3)) ==
           STANDARD_COUNTERPOINT_WEAK_BEAT_SUBSTITUTION);
}

static void test_skipped_passing_tone(void) {
    /* A third, then a step. */
    assert(pattern_of(P(C, 0, 3), P(F, 0, 3), P(E, 0, 3), P(G, 0, 3), P(A, 0, 3)) ==
           STANDARD_COUNTERPOINT_WEAK_BEAT_SKIPPED_PASSING_TONE);
    /* A step, then a third. */
    assert(pattern_of(P(C, 0, 3), P(E, 0, 3), P(G, 0, 3), P(A, 0, 3), P(C, 0, 4)) ==
           STANDARD_COUNTERPOINT_WEAK_BEAT_SKIPPED_PASSING_TONE);
}

static void test_interval_subdivision(void) {
    /* A fifth in two thirds. */
    assert(pattern_of(P(C, 0, 3), P(G, 0, 3), P(E, 0, 3), P(G, 0, 3), P(B, 0, 3)) ==
           STANDARD_COUNTERPOINT_WEAK_BEAT_INTERVAL_SUBDIVISION);
    /* A sixth in a third and a fourth, and in a fourth and a third. */
    assert(pattern_of(P(C, 0, 3), P(E, 0, 3), P(E, 0, 3), P(G, 0, 3), P(C, 0, 4)) ==
           STANDARD_COUNTERPOINT_WEAK_BEAT_INTERVAL_SUBDIVISION);
    assert(pattern_of(P(C, 0, 3), P(G, 0, 3), P(G, 0, 3), P(C, 0, 4), P(E, 0, 4)) ==
           STANDARD_COUNTERPOINT_WEAK_BEAT_INTERVAL_SUBDIVISION);
}

static void test_change_of_register(void) {
    /* An octave up and a step back: it makes a seventh with the downbeat. */
    assert(pattern_of(P(C, 0, 3), P(D, 0, 3), P(G, 0, 3), P(G, 0, 4), P(F, 0, 4)) ==
           STANDARD_COUNTERPOINT_WEAK_BEAT_CHANGE_OF_REGISTER);
    /* A sixth. */
    assert(pattern_of(P(C, 0, 3), P(G, 0, 3), P(E, 0, 3), P(C, 0, 4), P(B, 0, 3)) ==
           STANDARD_COUNTERPOINT_WEAK_BEAT_CHANGE_OF_REGISTER);
}

static void test_delay_of_melodic_progression(void) {
    assert(pattern_of(P(C, 0, 3), P(D, 0, 3), P(E, 0, 3), P(G, 0, 3), P(F, 0, 3)) ==
           STANDARD_COUNTERPOINT_WEAK_BEAT_DELAY_OF_MELODIC_PROGRESSION);
}

static void test_consonant_neighbor_tone(void) {
    /* A fifth becomes a sixth and back. */
    assert(pattern_of(P(C, 0, 3), P(E, 0, 3), P(G, 0, 3), P(A, 0, 3), P(G, 0, 3)) ==
           STANDARD_COUNTERPOINT_WEAK_BEAT_CONSONANT_NEIGHBOR_TONE);
}

static void test_no_pattern(void) {
    /* A repeated note. */
    assert(has_no_pattern(P(C, 0, 3), P(E, 0, 3), P(E, 0, 3), P(E, 0, 3), P(E, 0, 3)));
    /* A step and then a leap of a fifth in the same direction. */
    assert(has_no_pattern(P(C, 0, 3), P(E, 0, 3), P(G, 0, 3), P(A, 0, 3), P(E, 0, 4)));
}

static void test_bars_without_a_pattern(void) {
    const PitchSpelling *cantus[] = {P(C, 0, 3), P(E, 0, 3)};
    StandardCounterpointWeakBeatPattern pattern = STANDARD_COUNTERPOINT_WEAK_BEAT_PATTERN_COUNT;

    /* The last bar has no next downbeat. */
    const StandardCounterpointSlot whole[] = {ATK(P(G, 0, 3)), ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), TIE(P(B, 0, 3))};
    EXERCISE(exercise, cantus, whole, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_weak_beat_pattern(&exercise, 1, &pattern));

    /* A whole note has no weak-beat note. */
    const StandardCounterpointSlot tied[] = {ATK(P(G, 0, 3)), TIE(P(G, 0, 3)), ATK(P(B, 0, 3)), TIE(P(B, 0, 3))};
    EXERCISE(tied_exercise, cantus, tied, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_weak_beat_pattern(&tied_exercise, 0, &pattern));

    /* A half rest has no downbeat. */
    const StandardCounterpointSlot rest[] = {REST(), ATK(P(A, 0, 3)), ATK(P(B, 0, 3)), TIE(P(B, 0, 3))};
    EXERCISE(rest_exercise, cantus, rest, CANTUS_LOWER);
    assert(!standard_counterpoint_second_species_weak_beat_pattern(&rest_exercise, 0, &pattern));

    assert(!standard_counterpoint_second_species_weak_beat_pattern(&exercise, 5, &pattern));
    assert(!standard_counterpoint_second_species_weak_beat_pattern(NULL, 0, &pattern));
    assert(!standard_counterpoint_second_species_weak_beat_pattern(&exercise, 0, NULL));
    assert(pattern == STANDARD_COUNTERPOINT_WEAK_BEAT_PATTERN_COUNT);
}

int main(void) {
    test_names();
    test_consonant_passing_tone();
    test_dissonant_passing_tone();
    test_substitution();
    test_skipped_passing_tone();
    test_interval_subdivision();
    test_change_of_register();
    test_delay_of_melodic_progression();
    test_consonant_neighbor_tone();
    test_no_pattern();
    test_bars_without_a_pattern();
    printf("test_second_species_pattern (standard-counterpoint): ok\n");
    return 0;
}
