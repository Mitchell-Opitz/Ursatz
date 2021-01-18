#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "interval_consonance_rules.h"

typedef StandardTonalityIntervalQuality Quality;

#define P STANDARD_TONALITY_QUALITY_PERFECT
#define MA STANDARD_TONALITY_QUALITY_MAJOR
#define MI STANDARD_TONALITY_QUALITY_MINOR
#define MELODIC STANDARD_TONALITY_INTERVAL_MELODIC
#define HARMONIC STANDARD_TONALITY_INTERVAL_HARMONIC

static bool consonant(StandardTonalityIntervalType type, int size, Quality quality) {
    return standard_tonality_interval_is_consonant(type, size, quality);
}

static bool dissonant(StandardTonalityIntervalType type, int size, Quality quality) {
    return standard_tonality_interval_is_dissonant(type, size, quality);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_INTERVAL_CONSONANCE_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_interval_consonance_rule((StandardTonalityIntervalConsonanceRule)i);
        const Constraint *constraint =
            standard_tonality_interval_consonance_constraint((StandardTonalityIntervalConsonanceRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_interval_consonance_rule(STANDARD_TONALITY_INTERVAL_CONSONANCE_RULE_COUNT) == NULL);
}

/* Example 12: melodic. Consonant: perfect intervals, ma2 mi2, ma3 mi3,
   ma6 mi6. Dissonant: augmented, diminished, ma7 mi7. */
static void test_melodic(void) {
    int perfect_sizes[] = {1, 4, 5, 8};
    for (size_t i = 0; i < 4; i++) {
        assert(consonant(MELODIC, perfect_sizes[i], P));
    }
    int consonant_sizes[] = {2, 3, 6};
    for (size_t i = 0; i < 3; i++) {
        assert(consonant(MELODIC, consonant_sizes[i], MA));
        assert(consonant(MELODIC, consonant_sizes[i], MI));
    }
    assert(dissonant(MELODIC, 7, MA));
    assert(dissonant(MELODIC, 7, MI));
    for (int size = 1; size <= 8; size++) {
        assert(dissonant(MELODIC, size, STANDARD_TONALITY_QUALITY_AUGMENTED));
        assert(dissonant(MELODIC, size, STANDARD_TONALITY_QUALITY_DIMINISHED));
    }
}

/* Example 13: harmonic. Consonant: ma3 mi3, ma6 mi6, P1 P8 P5. Dissonant:
   ma2 mi2, augmented, diminished, ma7 mi7, P4. */
static void test_harmonic(void) {
    assert(consonant(HARMONIC, 3, MA) && consonant(HARMONIC, 3, MI));
    assert(consonant(HARMONIC, 6, MA) && consonant(HARMONIC, 6, MI));
    assert(consonant(HARMONIC, 1, P));
    assert(consonant(HARMONIC, 5, P));
    assert(consonant(HARMONIC, 8, P));

    assert(dissonant(HARMONIC, 2, MA) && dissonant(HARMONIC, 2, MI));
    assert(dissonant(HARMONIC, 7, MA) && dissonant(HARMONIC, 7, MI));
    assert(dissonant(HARMONIC, 4, P)); /* the perfect fourth */
    for (int size = 1; size <= 8; size++) {
        assert(dissonant(HARMONIC, size, STANDARD_TONALITY_QUALITY_AUGMENTED));
        assert(dissonant(HARMONIC, size, STANDARD_TONALITY_QUALITY_DIMINISHED));
    }
}

/* The perfect fourth is the one place the two tables differ for a
   perfect interval; the seconds differ too. */
static void test_melodic_and_harmonic_differ(void) {
    assert(consonant(MELODIC, 4, P) && dissonant(HARMONIC, 4, P));
    assert(consonant(MELODIC, 2, MA) && dissonant(HARMONIC, 2, MA));
    assert(consonant(MELODIC, 2, MI) && dissonant(HARMONIC, 2, MI));
    /* They agree on thirds and sixths, and on both kinds of seventh. */
    assert(consonant(MELODIC, 3, MA) == consonant(HARMONIC, 3, MA));
    assert(consonant(MELODIC, 6, MI) == consonant(HARMONIC, 6, MI));
    assert(dissonant(MELODIC, 7, MI) && dissonant(HARMONIC, 7, MI));
}

/* Doubly and triply altered intervals count as augmented or diminished. */
static void test_further_altered_are_dissonant(void) {
    Quality altered[] = {STANDARD_TONALITY_QUALITY_DOUBLY_AUGMENTED, STANDARD_TONALITY_QUALITY_TRIPLY_AUGMENTED,
                         STANDARD_TONALITY_QUALITY_DOUBLY_DIMINISHED, STANDARD_TONALITY_QUALITY_TRIPLY_DIMINISHED};
    for (size_t i = 0; i < 4; i++) {
        assert(dissonant(MELODIC, 5, altered[i]));
        assert(dissonant(HARMONIC, 3, altered[i]));
    }
}

/* Compound intervals are classified as their simple counterparts. */
static void test_compound_follows_simple(void) {
    for (int type = 0; type < 2; type++) {
        StandardTonalityIntervalType t = (StandardTonalityIntervalType)type;
        for (int size = 1; size <= 8; size++) {
            for (int q = 0; q < STANDARD_TONALITY_QUALITY_COUNT; q++) {
                int compound = size + 7;
                assert(consonant(t, size, (Quality)q) == consonant(t, compound, (Quality)q));
                assert(dissonant(t, size, (Quality)q) == dissonant(t, compound, (Quality)q));
            }
        }
    }
    assert(consonant(HARMONIC, 10, MA)); /* a tenth is a third */
    assert(consonant(HARMONIC, 12, P));
    assert(dissonant(HARMONIC, 11, P));
}

/* Property: every interval whose quality fits its size is exactly one of
   consonant and dissonant; one that does not fit is neither. */
static void test_exactly_one(void) {
    for (int type = 0; type < 2; type++) {
        StandardTonalityIntervalType t = (StandardTonalityIntervalType)type;
        for (int size = 1; size <= 15; size++) {
            for (int q = 0; q < STANDARD_TONALITY_QUALITY_COUNT; q++) {
                bool fits = standard_tonality_interval_quality_fits_size(size, (Quality)q);
                bool c = consonant(t, size, (Quality)q);
                bool d = dissonant(t, size, (Quality)q);
                if (fits) {
                    assert(c != d);
                } else {
                    assert(!c && !d);
                }
            }
        }
    }
}

static void test_invalid_type(void) {
    assert(!consonant((StandardTonalityIntervalType)99, 3, MA));
    assert(!dissonant((StandardTonalityIntervalType)99, 3, MA));
    assert(!consonant(MELODIC, 0, P));
    assert(!consonant(MELODIC, 3, P)); /* no perfect third */
}

int main(void) {
    test_rules_reference_framework();
    test_melodic();
    test_harmonic();
    test_melodic_and_harmonic_differ();
    test_further_altered_are_dissonant();
    test_compound_follows_simple();
    test_exactly_one();
    test_invalid_type();
    printf("test_interval_consonance_rules (standard-tonality): ok\n");
    return 0;
}
