#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_template.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_state.h>

/*
 * plan_give_chords (given.h): one Given harmony span for each chord, exactly its onset and length.
 *
 *   exact     chords on beats and on a half beat give spans of exactly that start and length, Given, with the numeral
 *   refused   an overlap of the chords, an overlap with harmony that is there, a chord past the section's end, an
 *             unreadable numeral and a negative onset are refused, and a refused call adds nothing
 */

static PlanTime time_of(long numerator, unsigned long denominator) {
    PlanTime time;
    assert(plan_time_make(numerator, denominator, &time) == PLAN_OK);
    return time;
}

static NodeId section_a(const PiecePlan *plan) {
    NodeId root = plan_root(plan);
    for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
        NodeId node = plan_node_child_at(plan, root, i);
        for (size_t c = 0; c < plan_node_constraint_count(plan, node); c++) {
            const PlanConstraint *constraint = plan_node_constraint_at(plan, node, c);
            if (constraint->kind == PLAN_CONSTRAINT_SECTION_LETTER && strcmp(constraint->params.letter, "A") == 0) {
                return node;
            }
        }
    }
    return PLAN_NODE_NONE;
}

static PiecePlan *new_plan(NodeId *section) {
    PieceRequest request;
    PiecePlan *plan = NULL;
    memset(&request, 0, sizeof(request));
    request.form = FORM_TEMPLATE_ROUNDED_BINARY;
    request.key.tonic_pitch_class = 0;
    request.key.mode = PLAN_MODE_MAJOR;
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    request.seed = 1;
    assert(piece_build_plan(&request, &plan) == PIECE_OK);
    *section = section_a(plan);
    assert(*section != PLAN_NODE_NONE);
    return plan;
}

static size_t harmony_spans(const PiecePlan *plan, NodeId section, LaneSpanId *ids, size_t capacity) {
    return plan_lane_find(plan, PLAN_LANE_HARMONY, 0, plan_node_span(plan, section), ids, capacity);
}

static void expect_span(const PiecePlan *plan, LaneSpanId id, PlanTime start, PlanTime length, const char *numeral) {
    PlanSpan span = plan_lane_span_span(plan, id);
    assert(plan_time_compare(span.start, start) == 0);
    assert(plan_time_compare(span.length, length) == 0);
    assert(plan_lane_span_state(plan, id) == PLAN_STATE_GIVEN);
    assert(strcmp(plan_lane_span_payload(plan, id)->harmony.roman_numeral, numeral) == 0);
}

static void test_exact(void) {
    NodeId section;
    PiecePlan *plan = new_plan(&section);
    PlanTime start = plan_node_span(plan, section).start;
    /* Bar 1 of 4/4: I for two beats, IV for a beat and a half, V on the "and" of beat 3 for the rest. */
    GivenChord chords[3] = {{"I", time_of(1, 1), time_of(1, 2)},
                            {"IV", time_of(3, 2), time_of(3, 8)},
                            {"V", time_of(15, 8), time_of(1, 8)}};
    LaneSpanId ids[8];
    assert(plan_give_chords(plan, section, chords, 3) == GIVEN_OK);
    assert(harmony_spans(plan, section, ids, 8) == 3);
    PlanTime at;
    assert(plan_time_add(start, chords[0].onset, &at) == PLAN_OK);
    expect_span(plan, ids[0], at, chords[0].length, "I");
    assert(plan_time_add(start, chords[1].onset, &at) == PLAN_OK);
    expect_span(plan, ids[1], at, chords[1].length, "IV");
    assert(plan_time_add(start, chords[2].onset, &at) == PLAN_OK);
    expect_span(plan, ids[2], at, chords[2].length, "V");
    plan_destroy(plan);
}

static void test_refused(void) {
    NodeId section;
    PiecePlan *plan = new_plan(&section);
    PlanSpan span = plan_node_span(plan, section);
    LaneSpanId ids[8];
    GivenChord overlap[2] = {{"I", time_of(0, 1), time_of(1, 2)}, {"V", time_of(3, 8), time_of(1, 2)}};
    GivenChord one[1] = {{"I", time_of(0, 1), time_of(1, 2)}};
    GivenChord past[2] = {{"I", time_of(0, 1), time_of(1, 2)}, {"V", span.length, time_of(1, 2)}};
    GivenChord straddle[1] = {{"V", time_of(span.length.num * 2 - 1, span.length.den * 2), time_of(1, 1)}};
    GivenChord bad[2] = {{"I", time_of(0, 1), time_of(1, 2)}, {"Q", time_of(1, 2), time_of(1, 2)}};
    GivenChord negative[1] = {{"I", time_of(-1, 4), time_of(1, 2)}};
    assert(plan_give_chords(plan, section, overlap, 2) == GIVEN_ERR_OVERLAP);
    assert(harmony_spans(plan, section, ids, 8) == 0);
    assert(plan_give_chords(plan, section, past, 2) == GIVEN_ERR_PAST_END);
    assert(plan_give_chords(plan, section, straddle, 1) == GIVEN_ERR_PAST_END);
    assert(plan_give_chords(plan, section, bad, 2) == GIVEN_ERR_BAD_NUMERAL);
    assert(plan_give_chords(plan, section, negative, 1) == GIVEN_ERR_BAD_NUMERAL);
    assert(plan_give_chords(plan, section, NULL, 0) == GIVEN_ERR_BAD_NUMERAL);
    assert(harmony_spans(plan, section, ids, 8) == 0);
    /* Harmony that is there is not overlapped. */
    assert(plan_give_chords(plan, section, one, 1) == GIVEN_OK);
    assert(plan_give_chords(plan, section, one, 1) == GIVEN_ERR_OVERLAP);
    assert(harmony_spans(plan, section, ids, 8) == 1);
    plan_destroy(plan);
}

int main(void) {
    test_exact();
    test_refused();
    printf("test_given_chords: ok\n");
    return 0;
}
