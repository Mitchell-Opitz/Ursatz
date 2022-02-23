#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "century_rules.h"
#include "century_test_util.h"

#define R8() REST4(), REST4()

static void test_group_names(void) {
    assert(strcmp(sixteenth_century_group_name(SIXTEENTH_CENTURY_GROUP_MELODIC), "melodic") == 0);
    assert(strcmp(sixteenth_century_group_name(SIXTEENTH_CENTURY_GROUP_HARMONIC), "harmonic") == 0);
    assert(strcmp(sixteenth_century_group_name(SIXTEENTH_CENTURY_GROUP_DISSONANCE), "dissonance") == 0);
    assert(strcmp(sixteenth_century_group_name(SIXTEENTH_CENTURY_GROUP_RHYTHM), "rhythm") == 0);
    assert(strcmp(sixteenth_century_group_name(SIXTEENTH_CENTURY_GROUP_IMITATION), "imitation") == 0);
    assert(sixteenth_century_group_name(SIXTEENTH_CENTURY_GROUP_COUNT) == NULL);
    assert(sixteenth_century_group_name((SixteenthCenturyGroup)-1) == NULL);
}

/* Two voices: a leader G F E D C F, and an answer an octave lower two tactus
   later, G F E D. Every interval between them is a consonance, so the score
   breaks no hard rule. */
#define IMITATIVE_SCORE(name, answer_first, answer_second)                                               \
    const PitchSpelling *name##_g4 = P(G, 0, 4);                                                         \
    const PitchSpelling *name##_f4 = P(F, 0, 4);                                                         \
    const PitchSpelling *name##_e4 = P(E, 0, 4);                                                         \
    const PitchSpelling *name##_d4 = P(D, 0, 4);                                                         \
    const PitchSpelling *name##_c4 = P(C, 0, 4);                                                         \
    const PitchSpelling *name##_g3 = P(G, 0, 3);                                                         \
    const PitchSpelling *name##_f3 = P(F, 0, 3);                                                         \
    const PitchSpelling *name##_e3 = P(E, 0, 3);                                                         \
    const PitchSpelling *name##_d3 = P(D, 0, 3);                                                         \
    StandardCounterpointSlot name##_lead[] = {HALF(name##_g4), HALF(name##_f4), HALF(name##_e4),         \
                                              HALF(name##_d4), HALF(name##_c4), HALF(name##_f4)};        \
    StandardCounterpointSlot name##_answer[] = {R8(), HALF(answer_first), HALF(answer_second),           \
                                                HALF(name##_e3), HALF(name##_d3)};                       \
    StandardCounterpointSlotLine name##_voices[] = {{name##_lead, COUNT_OF(name##_lead)},                \
                                                    {name##_answer, COUNT_OF(name##_answer)}};           \
    SCORE_OF(name, name##_voices);                                                                       \
    SixteenthCenturyEntry name##_entries[] = {{0, 0}, {1, 0}};                                           \
    SixteenthCenturyPoint name##_point = {&name, name##_entries, 2, 4}

static void test_good_score(void) {
    IMITATIVE_SCORE(score, score_g3, score_f3);
    SixteenthCenturyInput input = {&score, &score_point, 1};
    SixteenthCenturyResult result;
    assert(sixteenth_century_check(&input, &result));
    assert(result.hard_violations == 0);
    assert(sixteenth_century_valid(&input));
    /* The score breaks no hard rule and its point is a real answer. */
    assert(!(result.soft_violations & (1u << SIXTEENTH_CENTURY_GROUP_IMITATION)));

    /* The same score without naming the point. */
    SixteenthCenturyInput no_points = {&score, NULL, 0};
    assert(sixteenth_century_valid(&no_points));
}

static void test_each_group_reports(void) {
    /* The answer rises where the leader falls: the imitation group, and
       only the imitation group, breaks a hard rule. */
    const PitchSpelling *e3 = P(E, 0, 3);
    const PitchSpelling *a3 = P(A, 0, 3);
    IMITATIVE_SCORE(bad_contour, bad_contour_g3, a3);
    (void)e3;
    (void)bad_contour_f3;
    SixteenthCenturyInput input = {&bad_contour, &bad_contour_point, 1};
    SixteenthCenturyResult result;
    assert(sixteenth_century_check(&input, &result));
    assert(result.hard_violations & (1u << SIXTEENTH_CENTURY_GROUP_IMITATION));
    assert(!sixteenth_century_valid(&input));

    /* A tritone leap breaks the melodic group. */
    const PitchSpelling *f4 = P(F, 0, 4);
    const PitchSpelling *b4 = P(B, 0, 4);
    StandardCounterpointSlot voice[] = {HALF(f4), HALF(b4)};
    StandardCounterpointSlotLine voices[] = {{voice, COUNT_OF(voice)}};
    SCORE_OF(tritone, voices);
    SixteenthCenturyInput tritone_input = {&tritone, NULL, 0};
    assert(sixteenth_century_check(&tritone_input, &result));
    assert(result.hard_violations == (1u << SIXTEENTH_CENTURY_GROUP_MELODIC));

    /* Parallel fifths break the harmonic group. */
    const PitchSpelling *g4 = P(G, 0, 4);
    const PitchSpelling *a4 = P(A, 0, 4);
    const PitchSpelling *c4 = P(C, 0, 4);
    const PitchSpelling *d4 = P(D, 0, 4);
    StandardCounterpointSlot top[] = {HALF(g4), HALF(a4)};
    StandardCounterpointSlot bottom[] = {HALF(c4), HALF(d4)};
    StandardCounterpointSlotLine pair[] = {{top, COUNT_OF(top)}, {bottom, COUNT_OF(bottom)}};
    SCORE_OF(fifths, pair);
    SixteenthCenturyInput fifths_input = {&fifths, NULL, 0};
    assert(sixteenth_century_check(&fifths_input, &result));
    assert(result.hard_violations == (1u << SIXTEENTH_CENTURY_GROUP_HARMONIC));

    /* A new dissonance on a tactus start breaks the dissonance group. */
    const PitchSpelling *c3 = P(C, 0, 3);
    StandardCounterpointSlot dis_top[] = {HALF(d4), HALF(c4)};
    StandardCounterpointSlot dis_bottom[] = {HALF(c3), HALF(c3)};
    StandardCounterpointSlotLine dis_pair[] = {{dis_top, COUNT_OF(dis_top)}, {dis_bottom, COUNT_OF(dis_bottom)}};
    SCORE_OF(dissonant, dis_pair);
    SixteenthCenturyInput dis_input = {&dissonant, NULL, 0};
    assert(sixteenth_century_check(&dis_input, &result));
    assert(result.hard_violations == (1u << SIXTEENTH_CENTURY_GROUP_DISSONANCE));

    /* Rhythm rules are all soft. */
    StandardCounterpointSlot quick[] = {QUARTER(c4), QUARTER(d4), HALF(c4)};
    StandardCounterpointSlotLine quick_voices[] = {{quick, COUNT_OF(quick)}};
    SCORE_OF(quick_score, quick_voices);
    SixteenthCenturyInput quick_input = {&quick_score, NULL, 0};
    assert(sixteenth_century_check(&quick_input, &result));
    assert(result.hard_violations == 0);
    assert(result.soft_violations & (1u << SIXTEENTH_CENTURY_GROUP_RHYTHM));
    assert(sixteenth_century_valid(&quick_input));
}

static void test_invalid_input(void) {
    IMITATIVE_SCORE(score, score_g3, score_f3);
    SixteenthCenturyResult result = {7, 7};
    SixteenthCenturyInput input = {&score, &score_point, 1};

    assert(!sixteenth_century_check(NULL, &result));
    assert(!sixteenth_century_check(&input, NULL));
    assert(!sixteenth_century_valid(NULL));

    SixteenthCenturyInput no_score = {NULL, NULL, 0};
    assert(!sixteenth_century_check(&no_score, &result));

    SixteenthCenturyInput missing_points = {&score, NULL, 1};
    assert(!sixteenth_century_check(&missing_points, &result));

    /* A point of another score. */
    SixteenthCenturyScore other = score;
    SixteenthCenturyInput foreign = {&other, &score_point, 1};
    assert(!sixteenth_century_check(&foreign, &result));

    /* An invalid point. */
    SixteenthCenturyPoint bad_point = score_point;
    bad_point.note_count = 1;
    SixteenthCenturyInput bad = {&score, &bad_point, 1};
    assert(!sixteenth_century_check(&bad, &result));

    assert(result.hard_violations == 7 && result.soft_violations == 7);
}

int main(void) {
    test_group_names();
    test_good_score();
    test_each_group_reports();
    test_invalid_input();
    printf("test_century_rules: ok\n");
    return 0;
}
