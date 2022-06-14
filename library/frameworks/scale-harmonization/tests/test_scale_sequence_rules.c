#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "scale_rule_check.h"
#include "scale_sequence_rules.h"
#include "scale_test_util.h"

#define SEQ(name) SCALE_SEQUENCE_##name

static bool holds(ScaleSequenceRule rule, ScalePatternId id, const ScaleHarmonization *harmonization) {
    ScaleSequence sequence = {scale_pattern(id), harmonization};
    return scale_sequence_rule_holds(rule, &sequence);
}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(scale_sequence_rule, scale_sequence_constraint, SCALE_SEQUENCE_RULE_COUNT);
    for (int i = 0; i < SCALE_SEQUENCE_RULE_COUNT; i++) {
        ConstraintType type = constraint_type(scale_sequence_constraint((ScaleSequenceRule)i));
        assert(type == (i <= SEQ(SUSPENDED) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(scale_sequence_rule_name(SEQ(DIRECTION)), "direction") == 0);
    assert(strcmp(scale_sequence_rule_name(SEQ(BASS_BY_STEP)), "bass-by-step") == 0);
    assert(strcmp(scale_sequence_rule_name(SEQ(INTERVALS)), "intervals") == 0);
    assert(strcmp(scale_sequence_rule_name(SEQ(REPEATS)), "repeats") == 0);
    assert(strcmp(scale_sequence_rule_name(SEQ(SUSPENDED)), "suspended") == 0);
    assert(strcmp(scale_sequence_rule_name(SEQ(WHOLE_UNITS)), "whole-units") == 0);
    assert(scale_sequence_rule_name(SEQ(RULE_COUNT)) == NULL);
}

/* The good exemplars, each over the bass scale in C. */
static void parallel_ascending(ScaleFixture *f) {
    const PitchSpelling *bass[8] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3), P(F, 0, 3),
                                    P(G, 0, 3), P(A, 0, 3), P(B, 0, 3), P(C, 0, 4)};
    const PitchSpelling *upper[8] = {P(A, 0, 3), P(B, 0, 3), P(C, 0, 4), P(D, 0, 4),
                                     P(E, 0, 4), P(F, 0, 4), P(G, 0, 4), P(A, 0, 4)};
    pair_fixture(f, SCALE_DIRECTION_ASCENDING, P(C, 0, 4), bass, upper, 8);
}

static void parallel_descending(ScaleFixture *f) {
    const PitchSpelling *bass[8] = {P(C, 0, 4), P(B, 0, 3), P(A, 0, 3), P(G, 0, 3),
                                    P(F, 0, 3), P(E, 0, 3), P(D, 0, 3), P(C, 0, 3)};
    const PitchSpelling *upper[8] = {P(A, 0, 4), P(G, 0, 4), P(F, 0, 4), P(E, 0, 4),
                                     P(D, 0, 4), P(C, 0, 4), P(B, 0, 3), P(A, 0, 3)};
    pair_fixture(f, SCALE_DIRECTION_DESCENDING, P(C, 0, 4), bass, upper, 8);
}

static void five_six_ascending(ScaleFixture *f) {
    const PitchSpelling *bass[6] = {P(C, 0, 3), P(C, 0, 3), P(D, 0, 3), P(D, 0, 3), P(E, 0, 3), P(E, 0, 3)};
    const PitchSpelling *upper[6] = {P(G, 0, 3), P(A, 0, 3), P(A, 0, 3), P(B, 0, 3), P(B, 0, 3), P(C, 0, 4)};
    pair_fixture(f, SCALE_DIRECTION_ASCENDING, P(C, 0, 4), bass, upper, 6);
}

static void five_six_descending(ScaleFixture *f) {
    const PitchSpelling *bass[6] = {P(C, 0, 4), P(C, 0, 4), P(B, 0, 3), P(B, 0, 3), P(A, 0, 3), P(A, 0, 3)};
    const PitchSpelling *upper[6] = {P(G, 0, 4), P(A, 0, 4), P(F, 0, 4), P(G, 0, 4), P(E, 0, 4), P(F, 0, 4)};
    pair_fixture(f, SCALE_DIRECTION_DESCENDING, P(C, 0, 4), bass, upper, 6);
}

static void five_six_alternating(ScaleFixture *f) {
    const PitchSpelling *bass[6] = {P(C, 0, 4), P(B, 0, 3), P(A, 0, 3), P(G, 0, 3), P(F, 0, 3), P(E, 0, 3)};
    const PitchSpelling *upper[6] = {P(G, 0, 4), P(G, 0, 4), P(E, 0, 4), P(E, 0, 4), P(C, 0, 4), P(C, 0, 4)};
    pair_fixture(f, SCALE_DIRECTION_DESCENDING, P(C, 0, 4), bass, upper, 6);
}

static void seven_six_descending(ScaleFixture *f) {
    const PitchSpelling *bass[6] = {P(C, 0, 4), P(C, 0, 4), P(B, 0, 3), P(B, 0, 3), P(A, 0, 3), P(A, 0, 3)};
    const PitchSpelling *upper[6] = {P(B, 0, 4), P(A, 0, 4), P(A, 0, 4), P(G, 0, 4), P(G, 0, 4), P(F, 0, 4)};
    pair_fixture(f, SCALE_DIRECTION_DESCENDING, P(C, 0, 4), bass, upper, 6);
}

static void test_good_exemplars(void) {
    ScaleFixture pa, pd, fa, fd, fl, sd;
    parallel_ascending(&pa);
    parallel_descending(&pd);
    five_six_ascending(&fa);
    five_six_descending(&fd);
    five_six_alternating(&fl);
    seven_six_descending(&sd);
    for (int i = 0; i < SCALE_SEQUENCE_RULE_COUNT; i++) {
        ScaleSequenceRule rule = (ScaleSequenceRule)i;
        assert(holds(rule, SCALE_PATTERN_PARALLEL_CHORDS, &pa.harmonization));
        assert(holds(rule, SCALE_PATTERN_PARALLEL_CHORDS, &pd.harmonization));
        assert(holds(rule, SCALE_PATTERN_FIVE_SIX_ASCENDING, &fa.harmonization));
        assert(holds(rule, SCALE_PATTERN_FIVE_SIX_DESCENDING, &fd.harmonization));
        assert(holds(rule, SCALE_PATTERN_FIVE_SIX_DESCENDING_ALTERNATING, &fl.harmonization));
        assert(holds(rule, SCALE_PATTERN_SEVEN_SIX_DESCENDING, &sd.harmonization));
    }
}

static void test_minor_key(void) {
    /* Parallel sixth chords over the ascending A minor scale. */
    const PitchSpelling *bass[8] = {P(A, 0, 2), P(B, 0, 2), P(C, 0, 3), P(D, 0, 3),
                                    P(E, 0, 3), P(F, 0, 3), P(G, 0, 3), P(A, 0, 3)};
    const PitchSpelling *upper[8] = {P(F, 0, 3), P(G, 0, 3), P(A, 0, 3), P(B, 0, 3),
                                     P(C, 0, 4), P(D, 0, 4), P(E, 0, 4), P(F, 0, 4)};
    ScaleFixture f;
    pair_fixture(&f, SCALE_DIRECTION_ASCENDING, P(A, 0, 3), bass, upper, 8);
    for (int i = 0; i < SCALE_SEQUENCE_RULE_COUNT; i++) {
        assert(holds((ScaleSequenceRule)i, SCALE_PATTERN_PARALLEL_CHORDS, &f.harmonization));
    }
}

static void test_direction(void) {
    ScaleFixture f;
    five_six_ascending(&f);
    /* The ascending 5-6 harmonized as if descending. */
    assert(!holds(SEQ(DIRECTION), SCALE_PATTERN_FIVE_SIX_DESCENDING, &f.harmonization));
    assert(holds(SEQ(DIRECTION), SCALE_PATTERN_FIVE_SIX_ASCENDING, &f.harmonization));
    assert(!holds(SEQ(DIRECTION), SCALE_PATTERN_SEVEN_SIX_DESCENDING, &f.harmonization));
}

static void test_bass_by_step(void) {
    ScaleFixture f;
    parallel_ascending(&f);
    /* The bass skips from D to F. */
    f.stages[2].pitches[0] = P(F, 0, 3);
    assert(!holds(SEQ(BASS_BY_STEP), SCALE_PATTERN_PARALLEL_CHORDS, &f.harmonization));

    /* The bass runs the wrong way for a descending harmonization. */
    parallel_ascending(&f);
    f.harmonization.direction = SCALE_DIRECTION_DESCENDING;
    assert(!holds(SEQ(BASS_BY_STEP), SCALE_PATTERN_PARALLEL_CHORDS, &f.harmonization));

    /* In a 5-6 the bass stays within a unit: here it moves at the second stage. */
    five_six_ascending(&f);
    f.stages[1].pitches[0] = P(D, 0, 3);
    assert(!holds(SEQ(BASS_BY_STEP), SCALE_PATTERN_FIVE_SIX_ASCENDING, &f.harmonization));

    /* And it must move at the start of the next. */
    five_six_ascending(&f);
    f.stages[2].pitches[0] = P(C, 0, 3);
    assert(!holds(SEQ(BASS_BY_STEP), SCALE_PATTERN_FIVE_SIX_ASCENDING, &f.harmonization));
}

static void test_intervals(void) {
    ScaleFixture f;
    parallel_ascending(&f);
    /* A fourth above the bass is not a first inversion chord. */
    f.stages[3].pitches[1] = P(B, 0, 3);
    assert(!holds(SEQ(INTERVALS), SCALE_PATTERN_PARALLEL_CHORDS, &f.harmonization));

    /* An upper voice below the bass. */
    parallel_ascending(&f);
    f.stages[1].pitches[1] = P(B, 0, 2);
    assert(!holds(SEQ(INTERVALS), SCALE_PATTERN_PARALLEL_CHORDS, &f.harmonization));

    /* A 5-6 whose second stage is another fifth. */
    five_six_ascending(&f);
    f.stages[1].pitches[1] = P(G, 0, 3);
    assert(!holds(SEQ(INTERVALS), SCALE_PATTERN_FIVE_SIX_ASCENDING, &f.harmonization));
}

static void test_repeats(void) {
    ScaleFixture f;
    parallel_ascending(&f);
    /* A third in the middle of a run of sixths: each interval is allowed, but
       the parallel chords are not parallel. */
    f.stages[3].pitches[1] = P(A, 0, 3);
    assert(holds(SEQ(INTERVALS), SCALE_PATTERN_PARALLEL_CHORDS, &f.harmonization));
    assert(!holds(SEQ(REPEATS), SCALE_PATTERN_PARALLEL_CHORDS, &f.harmonization));

    /* An octave and a third throughout is as parallel as sixths. */
    const PitchSpelling *bass[4] = {P(C, 0, 3), P(D, 0, 3), P(E, 0, 3), P(F, 0, 3)};
    const PitchSpelling *upper[4] = {P(E, 0, 4), P(F, 0, 4), P(G, 0, 4), P(A, 0, 4)};
    pair_fixture(&f, SCALE_DIRECTION_ASCENDING, P(C, 0, 4), bass, upper, 4);
    assert(holds(SEQ(REPEATS), SCALE_PATTERN_PARALLEL_CHORDS, &f.harmonization));
}

static void test_suspended(void) {
    ScaleFixture f;
    seven_six_descending(&f);
    /* The upper voice is not held into the third stage: it moves to F. */
    f.stages[2].pitches[1] = P(G, 0, 4);
    assert(!holds(SEQ(SUSPENDED), SCALE_PATTERN_SEVEN_SIX_DESCENDING, &f.harmonization));

    /* The rule does not apply to a pattern with no suspensions. */
    parallel_descending(&f);
    assert(holds(SEQ(SUSPENDED), SCALE_PATTERN_PARALLEL_CHORDS, &f.harmonization));
}

static void test_whole_units(void) {
    ScaleFixture f;
    five_six_ascending(&f);
    f.harmonization.stage_count = 5;
    assert(!holds(SEQ(WHOLE_UNITS), SCALE_PATTERN_FIVE_SIX_ASCENDING, &f.harmonization));
    f.harmonization.stage_count = 4;
    assert(holds(SEQ(WHOLE_UNITS), SCALE_PATTERN_FIVE_SIX_ASCENDING, &f.harmonization));
}

static void test_invalid_and_check(void) {
    ScaleFixture f;
    parallel_ascending(&f);
    ScaleSequence good = {scale_pattern(SCALE_PATTERN_PARALLEL_CHORDS), &f.harmonization};
    assert(scale_sequence_valid(&good));
    assert(!scale_sequence_valid(NULL));
    ScaleSequence no_pattern = {NULL, &f.harmonization};
    assert(!scale_sequence_valid(&no_pattern));
    ScaleSequence no_harmonization = {scale_pattern(SCALE_PATTERN_PARALLEL_CHORDS), NULL};
    assert(!scale_sequence_valid(&no_harmonization));

    assert(!scale_sequence_rule_holds(SEQ(RULE_COUNT), &good));
    assert(!scale_sequence_rule_holds((ScaleSequenceRule)-1, &good));
    assert(!scale_sequence_rule_holds(SEQ(INTERVALS), NULL));

    unsigned hard = 7, soft = 7;
    assert(!scale_sequence_check(NULL, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!scale_sequence_check(&good, NULL, &soft));
    assert(!scale_sequence_check(&good, &hard, NULL));

    assert(scale_sequence_check(&good, &hard, &soft));
    assert(hard == 0 && soft == 0);

    /* Wrong way for the bass: a hard failure. An odd length: advice. */
    five_six_ascending(&f);
    f.harmonization.stage_count = 5;
    ScaleSequence five_six = {scale_pattern(SCALE_PATTERN_FIVE_SIX_ASCENDING), &f.harmonization};
    assert(scale_sequence_check(&five_six, &hard, &soft));
    assert(hard == 0);
    assert(soft == (1u << SEQ(WHOLE_UNITS)));
    f.harmonization.direction = SCALE_DIRECTION_DESCENDING;
    assert(scale_sequence_check(&five_six, &hard, &soft));
    assert(hard & (1u << SEQ(DIRECTION)));
    assert(hard & (1u << SEQ(BASS_BY_STEP)));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_good_exemplars();
    test_minor_key();
    test_direction();
    test_bass_by_step();
    test_intervals();
    test_repeats();
    test_suspended();
    test_whole_units();
    test_invalid_and_check();
    printf("test_scale_sequence_rules: ok\n");
    return 0;
}
