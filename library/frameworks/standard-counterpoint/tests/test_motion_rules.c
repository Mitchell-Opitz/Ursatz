#include <assert.h>
#include <stdio.h>

#include "counterpoint_test_util.h"
#include "motion_rules.h"
#include "rule_check.h"

static MotionType motion(const PitchSpelling *lower_from, const PitchSpelling *lower_to,
                         const PitchSpelling *upper_from, const PitchSpelling *upper_to) {
    MotionType out;
    assert(standard_counterpoint_classify_motion(lower_from, lower_to, upper_from, upper_to, &out));
    return out;
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_counterpoint_motion_rule, standard_counterpoint_motion_constraint,
                                    STANDARD_COUNTERPOINT_MOTION_RULE_COUNT);
    for (int i = 0; i < STANDARD_COUNTERPOINT_MOTION_RULE_COUNT; i++) {
        assert(constraint_type(standard_counterpoint_motion_constraint((StandardCounterpointMotionRule)i)) ==
               CONSTRAINT_TYPE_HARD);
    }
}

static void test_contrary(void) {
    assert(motion(P(C, 0, 4), P(B, 0, 3), P(E, 0, 4), P(G, 0, 4)) == MOTION_TYPE_CONTRARY);
    assert(motion(P(C, 0, 4), P(G, 0, 4), P(E, 0, 5), P(D, 0, 5)) == MOTION_TYPE_CONTRARY);
}

static void test_similar(void) {
    /* Both up, by different distances. */
    assert(motion(P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(G, 0, 4)) == MOTION_TYPE_SIMILAR);
    /* Both down, by different distances. */
    assert(motion(P(G, 0, 4), P(C, 0, 4), P(E, 0, 5), P(D, 0, 5)) == MOTION_TYPE_SIMILAR);
}

static void test_parallel(void) {
    /* Thirds stay thirds, major to minor included. */
    assert(motion(P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(F, 0, 4)) == MOTION_TYPE_PARALLEL);
    assert(motion(P(D, 0, 4), P(C, 0, 4), P(F, 0, 4), P(E, 0, 4)) == MOTION_TYPE_PARALLEL);
    /* Perfect fifths. */
    assert(motion(P(C, 0, 4), P(D, 0, 4), P(G, 0, 4), P(A, 0, 4)) == MOTION_TYPE_PARALLEL);
}

static void test_oblique(void) {
    assert(motion(P(C, 0, 4), P(C, 0, 4), P(E, 0, 4), P(F, 0, 4)) == MOTION_TYPE_OBLIQUE);
    assert(motion(P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(E, 0, 4)) == MOTION_TYPE_OBLIQUE);
}

static void test_static(void) {
    assert(motion(P(C, 0, 4), P(C, 0, 4), P(E, 0, 4), P(E, 0, 4)) == MOTION_TYPE_STATIC);
}

static void test_null_motion(void) {
    MotionType out = MOTION_TYPE_STATIC;
    assert(!standard_counterpoint_classify_motion(NULL, P(C, 0, 4), P(E, 0, 4), P(F, 0, 4), &out));
    assert(!standard_counterpoint_classify_motion(P(C, 0, 4), NULL, P(E, 0, 4), P(F, 0, 4), &out));
    assert(!standard_counterpoint_classify_motion(P(C, 0, 4), P(D, 0, 4), NULL, P(F, 0, 4), &out));
    assert(!standard_counterpoint_classify_motion(P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), NULL, &out));
    assert(!standard_counterpoint_classify_motion(P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(F, 0, 4), NULL));
}

static void test_pair_motion(void) {
    const PitchSpelling *lower_notes[] = {P(C, 0, 4), P(D, 0, 4), P(E, 0, 4)};
    const PitchSpelling *upper_notes[] = {P(G, 0, 4), P(F, 0, 4), P(G, 0, 4)};
    StandardCounterpointLine lower = LINE_OF(lower_notes);
    StandardCounterpointLine upper = LINE_OF(upper_notes);
    StandardCounterpointPair pair = {&upper, &lower};
    MotionType out;

    assert(standard_counterpoint_pair_motion(&pair, 0, &out) && out == MOTION_TYPE_CONTRARY);
    assert(standard_counterpoint_pair_motion(&pair, 1, &out) && out == MOTION_TYPE_PARALLEL);
    assert(!standard_counterpoint_pair_motion(&pair, 2, &out));
    assert(!standard_counterpoint_pair_motion(NULL, 0, &out));

    StandardCounterpointLine short_upper = {upper_notes, 2};
    StandardCounterpointPair mismatched = {&short_upper, &lower};
    assert(!standard_counterpoint_pair_motion(&mismatched, 0, &out));
}

int main(void) {
    test_rules_reference_framework();
    test_contrary();
    test_similar();
    test_parallel();
    test_oblique();
    test_static();
    test_null_motion();
    test_pair_motion();
    printf("test_motion_rules (standard-counterpoint): ok\n");
    return 0;
}
