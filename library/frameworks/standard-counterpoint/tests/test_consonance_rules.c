#include <assert.h>
#include <stdio.h>

#include "consonance_rules.h"
#include "counterpoint_test_util.h"
#include "rule_check.h"

static StandardCounterpointIntervalCategory category(const PitchSpelling *a, const PitchSpelling *b) {
    StandardCounterpointIntervalCategory out;
    assert(standard_counterpoint_classify_pitches(a, b, &out));
    return out;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_consonance_rule, standard_counterpoint_consonance_constraint,
                                    STANDARD_COUNTERPOINT_CONSONANCE_RULE_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_CONSONANCE_RULE_COUNT; i++) {
        assert(constraint_type(standard_counterpoint_consonance_constraint(
                   (StandardCounterpointConsonanceRule)i)) == CONSTRAINT_TYPE_HARD);
    }
}

static void test_perfect_consonances(void) {
    assert(category(P(C, 0, 4), P(C, 0, 4)) == STANDARD_COUNTERPOINT_PERFECT_CONSONANCE);
    assert(category(P(C, 0, 4), P(G, 0, 4)) == STANDARD_COUNTERPOINT_PERFECT_CONSONANCE);
    assert(category(P(C, 0, 4), P(C, 0, 5)) == STANDARD_COUNTERPOINT_PERFECT_CONSONANCE);
    /* Compound: a twelfth and a fifteenth. */
    assert(category(P(C, 0, 4), P(G, 0, 5)) == STANDARD_COUNTERPOINT_PERFECT_CONSONANCE);
    assert(category(P(C, 0, 4), P(C, 0, 6)) == STANDARD_COUNTERPOINT_PERFECT_CONSONANCE);
    /* Direction and argument order do not matter. */
    assert(category(P(G, 0, 4), P(C, 0, 4)) == STANDARD_COUNTERPOINT_PERFECT_CONSONANCE);
}

static void test_imperfect_consonances(void) {
    assert(category(P(C, 0, 4), P(E, 0, 4)) == STANDARD_COUNTERPOINT_IMPERFECT_CONSONANCE);
    assert(category(P(C, 0, 4), P(E, -1, 4)) == STANDARD_COUNTERPOINT_IMPERFECT_CONSONANCE);
    assert(category(P(C, 0, 4), P(A, 0, 4)) == STANDARD_COUNTERPOINT_IMPERFECT_CONSONANCE);
    assert(category(P(C, 0, 4), P(A, -1, 4)) == STANDARD_COUNTERPOINT_IMPERFECT_CONSONANCE);
    /* A tenth is a third. */
    assert(category(P(C, 0, 4), P(E, 0, 5)) == STANDARD_COUNTERPOINT_IMPERFECT_CONSONANCE);
}

static void test_dissonances(void) {
    assert(category(P(C, 0, 4), P(D, 0, 4)) == STANDARD_COUNTERPOINT_DISSONANCE);
    assert(category(P(C, 0, 4), P(D, -1, 4)) == STANDARD_COUNTERPOINT_DISSONANCE);
    assert(category(P(C, 0, 4), P(B, 0, 4)) == STANDARD_COUNTERPOINT_DISSONANCE);
    assert(category(P(C, 0, 4), P(B, -1, 4)) == STANDARD_COUNTERPOINT_DISSONANCE);
    /* Augmented and diminished intervals. */
    assert(category(P(C, 0, 4), P(G, 1, 4)) == STANDARD_COUNTERPOINT_DISSONANCE);
    assert(category(P(C, 0, 4), P(G, -1, 4)) == STANDARD_COUNTERPOINT_DISSONANCE);
    assert(category(P(C, 0, 4), P(E, 1, 4)) == STANDARD_COUNTERPOINT_DISSONANCE);
    assert(category(P(C, 0, 4), P(C, 1, 4)) == STANDARD_COUNTERPOINT_DISSONANCE);
}

static void test_perfect_fourth_is_dissonant(void) {
    assert(category(P(C, 0, 4), P(F, 0, 4)) == STANDARD_COUNTERPOINT_DISSONANCE);
    assert(category(P(C, 0, 4), P(F, 0, 5)) == STANDARD_COUNTERPOINT_DISSONANCE);
    assert(!standard_counterpoint_pitches_consonant(P(C, 0, 4), P(F, 0, 4)));
}

static void test_pitches_consonant(void) {
    assert(standard_counterpoint_pitches_consonant(P(C, 0, 4), P(G, 0, 4)));
    assert(standard_counterpoint_pitches_consonant(P(C, 0, 4), P(E, 0, 4)));
    assert(!standard_counterpoint_pitches_consonant(P(C, 0, 4), P(D, 0, 4)));
    assert(!standard_counterpoint_pitches_consonant(NULL, P(D, 0, 4)));
}

static void test_null_classification(void) {
    StandardCounterpointIntervalCategory out = STANDARD_COUNTERPOINT_DISSONANCE;
    assert(!standard_counterpoint_classify_pitches(NULL, P(C, 0, 4), &out));
    assert(!standard_counterpoint_classify_pitches(P(C, 0, 4), NULL, &out));
    assert(!standard_counterpoint_classify_pitches(P(C, 0, 4), P(G, 0, 4), NULL));
    assert(!standard_counterpoint_classify_interval(NULL, &out));
}

static bool melodic(const PitchSpelling *a, const PitchSpelling *b) {
    StandardCounterpointInterval interval;
    assert(standard_counterpoint_interval_between(a, b, &interval));
    return standard_counterpoint_interval_melodic_consonant(&interval);
}

static void test_melodic_consonance(void) {
    assert(melodic(P(C, 0, 4), P(D, 0, 4)));
    assert(melodic(P(C, 0, 4), P(D, -1, 4)));
    assert(melodic(P(C, 0, 4), P(E, -1, 4)));
    assert(melodic(P(C, 0, 4), P(F, 0, 4)));
    assert(melodic(P(C, 0, 4), P(G, 0, 4)));
    assert(melodic(P(C, 0, 4), P(A, -1, 4)));
    assert(melodic(P(C, 0, 4), P(A, 0, 4)));
    assert(melodic(P(C, 0, 4), P(C, 0, 5)));

    assert(!melodic(P(C, 0, 4), P(B, -1, 4)));
    assert(!melodic(P(C, 0, 4), P(B, 0, 4)));
    assert(!melodic(P(F, 0, 4), P(B, 0, 4)));
    assert(!melodic(P(C, 0, 4), P(G, -1, 4)));
    assert(!melodic(P(C, 0, 4), P(D, 1, 4)));
    assert(!standard_counterpoint_interval_melodic_consonant(NULL));
}

int main(void) {
    test_rules_reference_framework();
    test_perfect_consonances();
    test_imperfect_consonances();
    test_dissonances();
    test_perfect_fourth_is_dissonant();
    test_pitches_consonant();
    test_null_classification();
    test_melodic_consonance();
    printf("test_consonance_rules (standard-counterpoint): ok\n");
    return 0;
}
