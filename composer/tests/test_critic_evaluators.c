#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/critic.h>

#include "support/test_knowledge.h"

/*
 * The ten evaluators of the critic (R-719, R-755) over lines built by hand, 4/4 with a beat of 1/4. A line is given by the durations of its
 * notes in sixteenths of a whole note and their pitches; a bar is 16 sixteenths and the first bar is bar 1.
 */

#define MAX_NOTES 64

static CriticLine build(const int *durations, const int *pitches, size_t count) {
    CriticLine line;
    int onset = 0;
    memset(&line, 0, sizeof(line));
    line.notes = calloc(count, sizeof(CriticNote));
    assert(line.notes != NULL);
    for (size_t i = 0; i < count; i++) {
        line.notes[i].onset = rational_ratio(onset, 16);
        line.notes[i].duration = rational_ratio(durations[i], 16);
        line.notes[i].pitch = pitches[i];
        line.notes[i].bar = onset / 16 + 1;
        line.notes[i].position = rational_ratio(onset % 16, 16);
        onset += durations[i];
    }
    line.count = count;
    line.beat = rational_ratio(1, 4);
    return line;
}

/* The pitches of a line of `count` notes that wander without repeating or leaping: only the rhythm is under test. */
static void wander(int *pitches, size_t count) {
    static const int STEPS[] = {60, 62, 64, 62, 64, 65};
    for (size_t i = 0; i < count; i++) {
        pitches[i] = STEPS[i % 6];
    }
}

static void evaluate(const CriticLine *line, const CriticLine *const *set, size_t set_count, CriticFinding out[CRITIC_EVALUATOR_COUNT]) {
    assert(critic_evaluate(line, set, set_count, test_knowledge(), out) == CRITIC_OK);
    for (int i = 0; i < CRITIC_EVALUATOR_COUNT; i++) {
        assert(out[i].evaluator == (CriticEvaluator)i);
        assert(rational_cmp(out[i].penalty, rational_ratio(0, 1)) >= 0 && rational_cmp(out[i].penalty, rational_ratio(1, 1)) <= 0);
    }
}

static void expect(const CriticFinding *finding, long long num, long long den) {
    if (finding->penalty.num != num || finding->penalty.den != den) {
        fprintf(stderr, "%s: penalty %lld/%lld, expected %lld/%lld\n", critic_evaluator_code(finding->evaluator), finding->penalty.num,
                finding->penalty.den, num, den);
        assert(false);
    }
}

static void test_rhythmic_monotony(void) {
    CriticFinding f[CRITIC_EVALUATOR_COUNT];
    int pitches[MAX_NOTES];
    /* eight bars, bars 1 to 4 on one rhythm: the third and fourth bars of the run count, 2 of the 6 bars between the first and last */
    const int eight[] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 8, 8, 12, 4, 4, 8, 4, 16};
    wander(pitches, 24);
    CriticLine line = build(eight, pitches, 24);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_RHYTHMIC_MONOTONY], 1, 3);
    assert(f[CRITIC_RHYTHMIC_MONOTONY].first == 1 && f[CRITIC_RHYTHMIC_MONOTONY].last == 4);
    critic_line_destroy(&line);
    /* every note but the last of one duration: monotonous whatever the bars */
    const int quarters[] = {4, 4, 4, 4, 4, 4, 8};
    wander(pitches, 7);
    line = build(quarters, pitches, 7);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_RHYTHMIC_MONOTONY], 1, 1);
    critic_line_destroy(&line);
    /* two bars have no run to count */
    const int two[] = {4, 4, 4, 4, 8, 8};
    wander(pitches, 6);
    line = build(two, pitches, 6);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_RHYTHMIC_MONOTONY], 0, 1);
    critic_line_destroy(&line);
}

/* A period of two ideas of two bars each: the basic idea in bars 1 and 2, the contrasting idea in bars 3 and 4. */
static CriticLine period(const int *durations, const int *pitches, size_t count, size_t idea_first) {
    CriticLine line = build(durations, pitches, count);
    CriticIdeaPair *pair = &line.pairs[0];
    snprintf(pair->idea, sizeof(pair->idea), "contrasting idea");
    snprintf(pair->against, sizeof(pair->against), "basic idea");
    pair->idea_first = idea_first;
    pair->idea_count = count - idea_first;
    pair->against_first = 0;
    pair->against_count = idea_first;
    pair->idea_start = rational_ratio(2, 1);
    pair->against_start = rational_ratio(0, 1);
    pair->idea_length = rational_ratio(2, 1);
    pair->against_length = rational_ratio(2, 1);
    line.pair_count = 1;
    return line;
}

static void test_contrast_distance(void) {
    CriticFinding f[CRITIC_EVALUATOR_COUNT];
    const int basic[] = {60, 62, 64, 65, 67, 65, 64, 62};
    int pitches[16];
    /* the contrasting idea repeats the basic idea's durations and directions a semitone up: it does not contrast */
    const int same_rhythm[] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
    memcpy(pitches, basic, sizeof(basic));
    for (int i = 0; i < 8; i++) {
        pitches[8 + i] = basic[i] + 1;
    }
    CriticLine line = period(same_rhythm, pitches, 16, 8);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_CONTRAST_DISTANCE], 1, 1);
    assert(strcmp(f[CRITIC_CONTRAST_DISTANCE].idea, "contrasting idea") == 0 && strcmp(f[CRITIC_CONTRAST_DISTANCE].against, "basic idea") == 0);
    assert(strcmp(f[CRITIC_CONTRAST_DISTANCE].aspect, "rhythm") == 0);
    critic_line_destroy(&line);
    /* other durations, in the register of the basic idea: it contrasts */
    const int other[] = {4, 4, 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 8};
    const int far_notes[] = {60, 62, 64, 65, 67, 65, 64, 62, 64, 62, 65, 67, 64, 62};
    memcpy(pitches, far_notes, sizeof(far_notes));
    line = period(other, pitches, 14, 8);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_CONTRAST_DISTANCE], 0, 1);
    critic_line_destroy(&line);
    /* the same 19 semitones higher is too far */
    for (int i = 8; i < 14; i++) {
        pitches[i] += 19;
    }
    line = period(other, pitches, 14, 8);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_CONTRAST_DISTANCE], 1, 1);
    assert(strcmp(f[CRITIC_CONTRAST_DISTANCE].aspect, "register") == 0 && f[CRITIC_CONTRAST_DISTANCE].value.num > 7);
    critic_line_destroy(&line);
    /* no pair, no penalty */
    line = period(other, pitches, 14, 8);
    line.pair_count = 0;
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_CONTRAST_DISTANCE], 0, 1);
    critic_line_destroy(&line);
}

static void test_leaps_and_repeats(void) {
    CriticFinding f[CRITIC_EVALUATOR_COUNT];
    const int quarters[] = {4, 4, 4, 4, 4};
    /* moves of 2, 3, 5 and 0 semitones: a skip counts 1, a leap 2, so (1 + 2) / 4 less the allowance 1/2 */
    const int moves[] = {60, 62, 65, 70, 70};
    CriticLine line = build(quarters, moves, 5);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_LEAP_LOAD], 1, 4);
    assert(f[CRITIC_LEAP_LOAD].value.num == 3 && f[CRITIC_LEAP_LOAD].value.den == 4 && f[CRITIC_LEAP_LOAD].limit.den == 2);
    expect(&f[CRITIC_REPEATED_NOTES], 1, 4);
    assert(f[CRITIC_REPEATED_NOTES].count == 1);
    critic_line_destroy(&line);
    const int steps[] = {60, 62, 64, 65, 67};
    line = build(quarters, steps, 5);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_LEAP_LOAD], 0, 1);
    expect(&f[CRITIC_REPEATED_NOTES], 0, 1);
    critic_line_destroy(&line);
    const int leaps[] = {60, 67, 60, 67, 60};
    line = build(quarters, leaps, 5);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_LEAP_LOAD], 1, 1);
    critic_line_destroy(&line);
    /* a line of one note has no move */
    line = build(quarters, steps, 1);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_LEAP_LOAD], 0, 1);
    expect(&f[CRITIC_REPEATED_NOTES], 0, 1);
    critic_line_destroy(&line);
}

static void test_open_ending(void) {
    CriticFinding f[CRITIC_EVALUATOR_COUNT];
    int pitches[MAX_NOTES];
    wander(pitches, 5);
    const int shorter[] = {8, 8, 8, 2};
    CriticLine line = build(shorter, pitches, 4);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_OPEN_ENDING], 1, 1);
    assert(f[CRITIC_OPEN_ENDING].bar == 2);
    critic_line_destroy(&line);
    const int longer[] = {4, 4, 4, 16};
    line = build(longer, pitches, 4);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_OPEN_ENDING], 0, 1);
    critic_line_destroy(&line);
    const int even[] = {4, 4, 4, 4};
    line = build(even, pitches, 4);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_OPEN_ENDING], 1, 1); /* no longer than the mean */
    critic_line_destroy(&line);
}

static void test_stray_syncopation(void) {
    CriticFinding f[CRITIC_EVALUATOR_COUNT];
    int pitches[MAX_NOTES];
    wander(pitches, 8);
    /* a quarter on the second eighth is held past the beat: syncopated; the same place in the next bar makes it a figure */
    const int twice[] = {2, 4, 2, 8, 2, 4, 2, 8};
    CriticLine line = build(twice, pitches, 8);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_STRAY_SYNCOPATION], 0, 1);
    critic_line_destroy(&line);
    const int once[] = {2, 4, 2, 8, 4, 4, 4, 4};
    line = build(once, pitches, 8);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_STRAY_SYNCOPATION], 1, 1);
    assert(f[CRITIC_STRAY_SYNCOPATION].bar == 1 && f[CRITIC_STRAY_SYNCOPATION].beat.num == 3 && f[CRITIC_STRAY_SYNCOPATION].beat.den == 2);
    critic_line_destroy(&line);
    const int none[] = {4, 4, 4, 4, 8, 8, 4, 4};
    line = build(none, pitches, 8);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_STRAY_SYNCOPATION], 0, 1);
    critic_line_destroy(&line);
}

static void test_one_rhythm(void) {
    CriticFinding f[CRITIC_EVALUATOR_COUNT];
    int pitches[MAX_NOTES];
    wander(pitches, 6);
    const int shared[] = {4, 4, 4, 4, 8, 8};
    const int apart[] = {8, 8, 4, 4, 4, 4};
    CriticLine a = build(shared, pitches, 6);
    CriticLine b = build(shared, pitches, 6);
    CriticLine c = build(apart, pitches, 6);
    const CriticLine *set[] = {&a, &b, &c};
    evaluate(&a, set, 3, f);
    expect(&f[CRITIC_ONE_RHYTHM], 1, 2);
    assert(f[CRITIC_ONE_RHYTHM].count == 1);
    evaluate(&c, set, 3, f);
    expect(&f[CRITIC_ONE_RHYTHM], 0, 1);
    evaluate(&a, set, 1, f);
    expect(&f[CRITIC_ONE_RHYTHM], 0, 1);
    evaluate(&a, NULL, 0, f);
    expect(&f[CRITIC_ONE_RHYTHM], 0, 1);
    const CriticLine *pair[] = {&a, &b};
    evaluate(&b, pair, 2, f);
    expect(&f[CRITIC_ONE_RHYTHM], 1, 1);
    critic_line_destroy(&a);
    critic_line_destroy(&b);
    critic_line_destroy(&c);
}

static void test_wide_range(void) {
    CriticFinding f[CRITIC_EVALUATOR_COUNT];
    const int halves[] = {8, 8};
    const int sixteen[] = {60, 76};
    CriticLine line = build(halves, sixteen, 2);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_WIDE_RANGE], 0, 1);
    assert(f[CRITIC_WIDE_RANGE].value.num == 16 && f[CRITIC_WIDE_RANGE].limit.num == 16);
    critic_line_destroy(&line);
    const int twenty[] = {60, 80};
    line = build(halves, twenty, 2);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_WIDE_RANGE], 1, 2);
    critic_line_destroy(&line);
    const int huge[] = {40, 90};
    line = build(halves, huge, 2);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_WIDE_RANGE], 1, 1);
    critic_line_destroy(&line);
}

/* A shift of register (R-755): the mean pitch of a bar against the bar before, over 7 semitones by up to 5 more. */
static void test_register_shift(void) {
    CriticFinding f[CRITIC_EVALUATOR_COUNT];
    const int bars[] = {16, 16};
    const int twelve[] = {60, 72};
    CriticLine line = build(bars, twelve, 2);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_REGISTER_SHIFT], 1, 1);
    assert(f[CRITIC_REGISTER_SHIFT].value.num == 12 && f[CRITIC_REGISTER_SHIFT].limit.num == 7 && f[CRITIC_REGISTER_SHIFT].first == 1 &&
           f[CRITIC_REGISTER_SHIFT].last == 2);
    critic_line_destroy(&line);
    const int eight[] = {60, 68};
    line = build(bars, eight, 2);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_REGISTER_SHIFT], 1, 5);
    assert(f[CRITIC_REGISTER_SHIFT].value.num == 8);
    critic_line_destroy(&line);
    const int within[] = {60, 67};
    line = build(bars, within, 2);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_REGISTER_SHIFT], 0, 1);
    critic_line_destroy(&line);
    /* The mean of a bar: two notes of the second bar, 66 and 74, make 70 against 60. */
    const int mixed_durations[] = {16, 8, 8};
    const int mixed[] = {60, 66, 74};
    line = build(mixed_durations, mixed, 3);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_REGISTER_SHIFT], 3, 5);
    critic_line_destroy(&line);
}

/* Half notes one after another (R-755): each half note after a half note counts, 2 make the penalty 1. */
static void test_half_notes(void) {
    CriticFinding f[CRITIC_EVALUATOR_COUNT];
    const int three_halves[] = {8, 8, 8, 8};
    int pitches[4] = {60, 62, 64, 62};
    CriticLine line = build(three_halves, pitches, 3);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_HALF_NOTES], 1, 1);
    assert(f[CRITIC_HALF_NOTES].count == 2 && f[CRITIC_HALF_NOTES].bar == 1);
    critic_line_destroy(&line);
    const int broken[] = {8, 4, 8, 4};
    line = build(broken, pitches, 4);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_HALF_NOTES], 0, 1);
    assert(f[CRITIC_HALF_NOTES].count == 0);
    critic_line_destroy(&line);
    const int one_pair[] = {4, 8, 8, 4};
    line = build(one_pair, pitches, 4);
    evaluate(&line, NULL, 0, f);
    expect(&f[CRITIC_HALF_NOTES], 1, 2);
    critic_line_destroy(&line);
}

static void test_score(void) {
    CriticFinding findings[CRITIC_EVALUATOR_COUNT];
    CriticSettings settings;
    for (int i = 0; i < CRITIC_EVALUATOR_COUNT; i++) {
        memset(&findings[i], 0, sizeof(findings[i]));
        findings[i].evaluator = (CriticEvaluator)i;
        findings[i].penalty = rational_ratio(0, 1);
    }
    findings[CRITIC_LEAP_LOAD].penalty = rational_ratio(1, 4);
    findings[CRITIC_WIDE_RANGE].penalty = rational_ratio(1, 2);
    critic_settings_default(&settings);
    Rational score = critic_score(findings, &settings);
    assert(score.num == -3 && score.den == 4);
    settings.weights[CRITIC_LEAP_LOAD] = 2;
    settings.reversed[CRITIC_WIDE_RANGE] = true;
    score = critic_score(findings, &settings);
    assert(score.num == 0 && score.den == 1);
    settings.weights[CRITIC_LEAP_LOAD] = 0;
    score = critic_score(findings, &settings);
    assert(score.num == 1 && score.den == 2);
}

static void test_settings_and_refusals(void) {
    CriticSettings settings;
    CriticFinding f[CRITIC_EVALUATOR_COUNT];
    critic_settings_default(&settings);
    for (int i = 0; i < CRITIC_EVALUATOR_COUNT; i++) {
        assert(settings.weights[i] == 1 && !settings.reversed[i]);
    }
    assert(!settings.rank && settings.candidates == 4 && settings.keep_rejected == 8);
    const int quarters[] = {4, 4, 4, 4};
    int pitches[MAX_NOTES];
    wander(pitches, 4);
    CriticLine line = build(quarters, pitches, 4);
    assert(critic_evaluate(&line, NULL, 0, NULL, f) == CRITIC_ERR_KNOWLEDGE);
    assert(critic_evaluate(NULL, NULL, 0, test_knowledge(), f) == CRITIC_ERR_NULL);
    assert(critic_evaluate(&line, NULL, 0, test_knowledge(), NULL) == CRITIC_ERR_NULL);
    critic_line_destroy(&line);
    CriticLine empty;
    memset(&empty, 0, sizeof(empty));
    assert(critic_evaluate(&empty, NULL, 0, test_knowledge(), f) == CRITIC_ERR_EMPTY);
}

int main(void) {
    test_rhythmic_monotony();
    test_contrast_distance();
    test_leaps_and_repeats();
    test_open_ending();
    test_stray_syncopation();
    test_one_rhythm();
    test_wide_range();
    test_register_shift();
    test_half_notes();
    test_score();
    test_settings_and_refusals();
    printf("test_critic_evaluators: ok\n");
    return 0;
}
