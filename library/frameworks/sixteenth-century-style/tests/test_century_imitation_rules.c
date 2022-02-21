#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "century_imitation_rules.h"
#include "century_rule_check.h"
#include "century_test_util.h"

#define IMITATION(name) SIXTEENTH_CENTURY_IMITATION_##name

#define R8() REST4(), REST4()

static bool holds(SixteenthCenturyImitationRule rule, const SixteenthCenturyPoint *point) {
    return sixteenth_century_imitation_rule_holds(rule, point);
}

/* A two-voice score over the slot arrays `lead` and `answer`, and a point of
   `notes` notes with the answer's first note the first of its voice. */
#define POINT_OF(name, lead, answer, notes)                                                           \
    StandardCounterpointSlotLine name##_voices[] = {{(lead), COUNT_OF(lead)}, {(answer), COUNT_OF(answer)}}; \
    SCORE_OF(name##_score, name##_voices);                                                            \
    SixteenthCenturyEntry name##_entries[] = {{0, 0}, {1, 0}};                                        \
    SixteenthCenturyPoint name = {&name##_score, name##_entries, 2, (notes)}

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(sixteenth_century_imitation_rule, sixteenth_century_imitation_constraint,
                                    SIXTEENTH_CENTURY_IMITATION_RULE_COUNT);
    for (int i = 0; i < SIXTEENTH_CENTURY_IMITATION_RULE_COUNT; i++) {
        ConstraintType type =
            constraint_type(sixteenth_century_imitation_constraint((SixteenthCenturyImitationRule)i));
        assert(type == (i <= IMITATION(CONTOUR_PRESERVED) ? CONSTRAINT_TYPE_HARD : CONSTRAINT_TYPE_SOFT));
    }
}

static void test_names(void) {
    assert(strcmp(sixteenth_century_imitation_rule_name(IMITATION(RHYTHM_PRESERVED)), "rhythm-preserved") == 0);
    assert(strcmp(sixteenth_century_imitation_rule_name(IMITATION(CONTOUR_PRESERVED)), "contour-preserved") == 0);
    assert(strcmp(sixteenth_century_imitation_rule_name(IMITATION(NOT_FREE)), "not-free") == 0);
    assert(strcmp(sixteenth_century_imitation_rule_name(IMITATION(ENTRY_INTERVAL)), "entry-interval") == 0);
    assert(strcmp(sixteenth_century_imitation_rule_name(IMITATION(METRIC_MATCH)), "metric-match") == 0);
    assert(strcmp(sixteenth_century_imitation_rule_name(IMITATION(OVERLAP)), "overlap") == 0);
    assert(sixteenth_century_imitation_rule_name(IMITATION(RULE_COUNT)) == NULL);
}

/* The leader's line G F E D C F in half notes; the point is its first four. */
#define LEADER(name)                                                                                     \
    const PitchSpelling *name##_g4 = P(G, 0, 4);                                                         \
    const PitchSpelling *name##_f4 = P(F, 0, 4);                                                         \
    const PitchSpelling *name##_e4 = P(E, 0, 4);                                                         \
    const PitchSpelling *name##_d4 = P(D, 0, 4);                                                         \
    const PitchSpelling *name##_c4 = P(C, 0, 4);                                                         \
    StandardCounterpointSlot name[] = {HALF(name##_g4), HALF(name##_f4), HALF(name##_e4),               \
                                       HALF(name##_d4), HALF(name##_c4), HALF(name##_f4)}

static void test_real_answer(void) {
    LEADER(lead);
    const PitchSpelling *g3 = P(G, 0, 3);
    const PitchSpelling *f3 = P(F, 0, 3);
    const PitchSpelling *e3 = P(E, 0, 3);
    const PitchSpelling *d3 = P(D, 0, 3);
    /* The answer follows two tactus later, an octave lower. */
    StandardCounterpointSlot answer[] = {R8(), HALF(g3), HALF(f3), HALF(e3), HALF(d3)};
    POINT_OF(point, lead, answer, 4);

    assert(sixteenth_century_point_valid(&point));
    for (int i = 0; i < SIXTEENTH_CENTURY_IMITATION_RULE_COUNT; i++) {
        assert(holds((SixteenthCenturyImitationRule)i, &point));
    }
    ImitationAnswer kind;
    assert(sixteenth_century_point_answer(&point, 1, &kind));
    assert(kind == IMITATION_ANSWER_REAL);
}

static void test_rhythm_preserved(void) {
    LEADER(lead);
    const PitchSpelling *g3 = P(G, 0, 3);
    const PitchSpelling *f3 = P(F, 0, 3);
    const PitchSpelling *e3 = P(E, 0, 3);
    /* The second note of the answer is a whole note. */
    StandardCounterpointSlot answer[] = {R8(), HALF(g3), WHOLE(f3), HALF(e3)};
    POINT_OF(point, lead, answer, 3);
    assert(!holds(IMITATION(RHYTHM_PRESERVED), &point));
}

static void test_contour_preserved(void) {
    LEADER(lead);
    const PitchSpelling *g3 = P(G, 0, 3);
    const PitchSpelling *a3 = P(A, 0, 3);
    const PitchSpelling *b3 = P(B, 0, 3);
    const PitchSpelling *c4 = P(C, 0, 4);
    /* The answer rises where the leader falls. */
    StandardCounterpointSlot answer[] = {R8(), HALF(g3), HALF(a3), HALF(b3), HALF(c4)};
    POINT_OF(point, lead, answer, 3);
    assert(!holds(IMITATION(CONTOUR_PRESERVED), &point));
}

static void test_not_free(void) {
    LEADER(lead);
    const PitchSpelling *g3 = P(G, 0, 3);
    const PitchSpelling *d3 = P(D, 0, 3);
    const PitchSpelling *a2 = P(A, 0, 2);
    const PitchSpelling *e2 = P(E, 0, 2);
    /* The same direction, but fourths where the leader has steps. */
    StandardCounterpointSlot answer[] = {R8(), HALF(g3), HALF(d3), HALF(a2), HALF(e2)};
    POINT_OF(point, lead, answer, 4);
    assert(holds(IMITATION(CONTOUR_PRESERVED), &point));
    assert(!holds(IMITATION(NOT_FREE), &point));
    ImitationAnswer kind;
    assert(sixteenth_century_point_answer(&point, 1, &kind));
    assert(kind == IMITATION_ANSWER_FREE);
}

static void test_tonal_answer(void) {
    const PitchSpelling *d4 = P(D, 0, 4);
    const PitchSpelling *a4 = P(A, 0, 4);
    const PitchSpelling *g4 = P(G, 0, 4);
    StandardCounterpointSlot lead[] = {HALF(d4), HALF(a4), HALF(g4), HALF(g4), HALF(g4), HALF(g4)};
    const PitchSpelling *a3 = P(A, 0, 3);
    const PitchSpelling *d4b = P(D, 0, 4);
    /* A fifth up answered by a fourth up: the answer is adjusted to the key. */
    StandardCounterpointSlot answer[] = {R8(), HALF(a3), HALF(d4b), HALF(d4b), HALF(d4b)};
    POINT_OF(point, lead, answer, 2);
    ImitationAnswer kind;
    assert(sixteenth_century_point_answer(&point, 1, &kind));
    assert(kind == IMITATION_ANSWER_TONAL);
    assert(holds(IMITATION(NOT_FREE), &point));

    /* An answer that turns the other way is free. */
    const PitchSpelling *f3 = P(F, 0, 3);
    StandardCounterpointSlot turned[] = {R8(), HALF(a3), HALF(f3), HALF(f3), HALF(f3)};
    POINT_OF(free_point, lead, turned, 2);
    assert(sixteenth_century_point_answer(&free_point, 1, &kind));
    assert(kind == IMITATION_ANSWER_FREE);
}

static void test_entry_interval(void) {
    LEADER(lead);
    const PitchSpelling *b3 = P(B, 0, 3);
    const PitchSpelling *a3 = P(A, 0, 3);
    const PitchSpelling *g3 = P(G, 0, 3);
    const PitchSpelling *f3 = P(F, 0, 3);
    /* The answer begins a minor sixth under the leader. */
    StandardCounterpointSlot answer[] = {R8(), HALF(b3), HALF(a3), HALF(g3), HALF(f3)};
    POINT_OF(point, lead, answer, 4);
    assert(!holds(IMITATION(ENTRY_INTERVAL), &point));
}

static void test_metric_match(void) {
    LEADER(lead);
    const PitchSpelling *g3 = P(G, 0, 3);
    const PitchSpelling *f3 = P(F, 0, 3);
    const PitchSpelling *e3 = P(E, 0, 3);
    const PitchSpelling *d3 = P(D, 0, 3);
    /* The answer begins on the sixth slot, between tactus starts, where the
       leader begins on a strong tactus. */
    StandardCounterpointSlot answer[] = {REST4(), REST(), REST(), HALF(g3), HALF(f3), HALF(e3), HALF(d3), REST(),
                                         REST()};
    POINT_OF(point, lead, answer, 4);
    assert(!holds(IMITATION(METRIC_MATCH), &point));
}

static void test_overlap(void) {
    LEADER(lead);
    const PitchSpelling *g3 = P(G, 0, 3);
    const PitchSpelling *f3 = P(F, 0, 3);
    const PitchSpelling *e3 = P(E, 0, 3);
    const PitchSpelling *d3 = P(D, 0, 3);
    StandardCounterpointSlot answer[] = {R8(), HALF(g3), HALF(f3), HALF(e3), HALF(d3)};
    /* A two-note point is over by the time the answer enters (slot 8). */
    POINT_OF(short_point, lead, answer, 2);
    assert(!holds(IMITATION(OVERLAP), &short_point));
    POINT_OF(long_point, lead, answer, 4);
    assert(holds(IMITATION(OVERLAP), &long_point));
}

static void test_three_voices(void) {
    /* A point of two notes, a fourth down, entered in three voices a long
       note apart. */
    const PitchSpelling *f5 = P(F, 0, 5);
    const PitchSpelling *c5 = P(C, 0, 5);
    const PitchSpelling *f4 = P(F, 0, 4);
    const PitchSpelling *c4 = P(C, 0, 4);
    const PitchSpelling *f3 = P(F, 0, 3);
    const PitchSpelling *c3 = P(C, 0, 3);
    StandardCounterpointSlot top[] = {WHOLE(f5), WHOLE(c5), R8(), R8()};
    StandardCounterpointSlot mid[] = {R8(), WHOLE(f4), WHOLE(c4), R8()};
    StandardCounterpointSlot bass[] = {R8(), R8(), WHOLE(f3), WHOLE(c3)};
    StandardCounterpointSlotLine voices[] = {{top, COUNT_OF(top)}, {mid, COUNT_OF(mid)}, {bass, COUNT_OF(bass)}};
    SCORE_OF(score, voices);
    SixteenthCenturyEntry entries[] = {{0, 0}, {1, 0}, {2, 0}};
    SixteenthCenturyPoint point = {&score, entries, 3, 2};
    assert(sixteenth_century_point_valid(&point));
    for (int i = 0; i < SIXTEENTH_CENTURY_IMITATION_RULE_COUNT; i++) {
        assert(holds((SixteenthCenturyImitationRule)i, &point));
    }
    ImitationAnswer kind;
    assert(sixteenth_century_point_answer(&point, 2, &kind));
    assert(kind == IMITATION_ANSWER_REAL);
}

static void test_point_valid(void) {
    LEADER(lead);
    const PitchSpelling *g3 = P(G, 0, 3);
    const PitchSpelling *f3 = P(F, 0, 3);
    StandardCounterpointSlot answer[] = {R8(), HALF(g3), HALF(f3), HALF(g3), HALF(f3)};
    POINT_OF(point, lead, answer, 2);
    assert(sixteenth_century_point_valid(&point));
    assert(!sixteenth_century_point_valid(NULL));

    SixteenthCenturyPoint one_entry = point;
    one_entry.entry_count = 1;
    assert(!sixteenth_century_point_valid(&one_entry));

    SixteenthCenturyPoint one_note = point;
    one_note.note_count = 1;
    assert(!sixteenth_century_point_valid(&one_note));

    SixteenthCenturyPoint too_long = point;
    too_long.note_count = 5;
    assert(!sixteenth_century_point_valid(&too_long));

    SixteenthCenturyEntry bad_voice[] = {{0, 0}, {2, 0}};
    SixteenthCenturyPoint bad_voice_point = point;
    bad_voice_point.entries = bad_voice;
    assert(!sixteenth_century_point_valid(&bad_voice_point));

    /* The entries must begin in order. */
    SixteenthCenturyEntry reversed[] = {{1, 0}, {0, 0}};
    SixteenthCenturyPoint reversed_point = point;
    reversed_point.entries = reversed;
    assert(!sixteenth_century_point_valid(&reversed_point));

    assert(!holds(IMITATION(RHYTHM_PRESERVED), &reversed_point));
    ImitationAnswer kind = IMITATION_ANSWER_FREE;
    assert(!sixteenth_century_point_answer(&point, 0, &kind));
    assert(!sixteenth_century_point_answer(&point, 2, &kind));
    assert(!sixteenth_century_point_answer(&point, 1, NULL));
    assert(kind == IMITATION_ANSWER_FREE);
}

static void test_invalid_and_check(void) {
    LEADER(lead);
    const PitchSpelling *g3 = P(G, 0, 3);
    const PitchSpelling *a3 = P(A, 0, 3);
    const PitchSpelling *b3 = P(B, 0, 3);
    const PitchSpelling *c4 = P(C, 0, 4);
    StandardCounterpointSlot answer[] = {R8(), HALF(g3), HALF(a3), HALF(b3), HALF(c4)};
    POINT_OF(point, lead, answer, 3);
    assert(!holds(IMITATION(RULE_COUNT), &point));
    assert(!holds((SixteenthCenturyImitationRule)-1, &point));
    assert(!holds(IMITATION(OVERLAP), NULL));

    unsigned hard = 0, soft = 0;
    assert(sixteenth_century_imitation_check(&point, &hard, &soft));
    assert(hard == (1u << IMITATION(CONTOUR_PRESERVED)));
    assert(soft & (1u << IMITATION(NOT_FREE)));

    hard = 7;
    soft = 7;
    assert(!sixteenth_century_imitation_check(NULL, &hard, &soft));
    assert(hard == 7 && soft == 7);
    assert(!sixteenth_century_imitation_check(&point, NULL, &soft));
    assert(!sixteenth_century_imitation_check(&point, &hard, NULL));
}

int main(void) {
    test_rules_reference_framework();
    test_names();
    test_real_answer();
    test_rhythm_preserved();
    test_contour_preserved();
    test_not_free();
    test_tonal_answer();
    test_entry_interval();
    test_metric_match();
    test_overlap();
    test_three_voices();
    test_point_valid();
    test_invalid_and_check();
    printf("test_century_imitation_rules: ok\n");
    return 0;
}
