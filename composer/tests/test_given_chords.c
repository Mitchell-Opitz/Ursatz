#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <plan/plan_constraint.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/given.h>
#include <plan/plan_lanes.h>
#include <ursatz_composer/piece.h>
#include <cJSON.h>
#include "piece/request_chords.h"
#include <plan/plan_state.h>
#include "support/test_knowledge.h"

/*
 * plan_give_chords (given.h): one Given harmony span for each chord, exactly its onset and length.
 *
 *   exact     chords on beats and on a half beat give spans of exactly that start and length, Given, with the numeral
 *   modes     a chord named with a mode (R-762, IV:dorian) is read by request_read_chords as the chord of that mode, a Given span
 *             whose payload borrows from it; the key's own mode, a diminished triad and a bad word are refused
 *   refused   an overlap of the chords, an overlap with harmony that is there, a chord past the section's end, an
 *             unreadable numeral and a negative onset are refused, and a refused call adds nothing
 */

static Rational time_of(long numerator, unsigned long denominator) {
    Rational time;
    assert(rational_make(numerator, denominator, &time) == RATIONAL_OK);
    return time;
}

static NodeId section_a(const Plan *plan) {
    NodeId root = plan_root(plan);
    for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
        NodeId node = plan_node_child_at(plan, root, i);
        const SectionDescription *section = plan_node_section(plan, node);
        if (section && strcmp(section->letter, "A") == 0) {
            return node;
        }
    }
    return PLAN_NODE_NONE;
}

static Plan *new_plan_in(const char *word, NodeId *section) {
    PieceRequest request;
    Plan *plan = NULL;
    memset(&request, 0, sizeof(request));
    request.form = "rounded-binary";
    request.key = test_key(0, word);
    request.knowledge = test_knowledge();
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    request.seed = 1;
    assert(piece_build_plan(&request, &plan) == PIECE_OK);
    *section = section_a(plan);
    assert(*section != PLAN_NODE_NONE);
    return plan;
}

static Plan *new_plan(NodeId *section) {
    return new_plan_in("major", section);
}

static size_t harmony_spans(const Plan *plan, NodeId section, LaneSpanId *ids, size_t capacity) {
    return plan_lane_find(plan, PLAN_LANE_HARMONY, 0, plan_node_span(plan, section), ids, capacity);
}

static void expect_span(const Plan *plan, LaneSpanId id, Rational start, Rational length, const char *numeral) {
    PlanSpan span = plan_lane_span_span(plan, id);
    assert(rational_cmp(span.start, start) == 0);
    assert(rational_cmp(span.length, length) == 0);
    assert(plan_lane_span_state(plan, id) == PLAN_STATE_GIVEN);
    assert(strcmp(plan_lane_span_payload(plan, id)->harmony.roman_numeral, numeral) == 0);
}

static void test_exact(void) {
    NodeId section;
    Plan *plan = new_plan(&section);
    Rational start = plan_node_span(plan, section).start;
    /* Bar 1 of 4/4: I for two beats, IV for a beat and a half, V on the "and" of beat 3 for the rest. */
    GivenChord chords[3] = {{"I", time_of(1, 1), time_of(1, 2)},
                            {"IV", time_of(3, 2), time_of(3, 8)},
                            {"V", time_of(15, 8), time_of(1, 8)}};
    LaneSpanId ids[8];
    assert(plan_give_chords(plan, section, chords, 3) == GIVEN_OK);
    assert(harmony_spans(plan, section, ids, 8) == 3);
    Rational at;
    assert(rational_add(start, chords[0].onset, &at) == RATIONAL_OK);
    expect_span(plan, ids[0], at, chords[0].length, "I");
    assert(rational_add(start, chords[1].onset, &at) == RATIONAL_OK);
    expect_span(plan, ids[1], at, chords[1].length, "IV");
    assert(rational_add(start, chords[2].onset, &at) == RATIONAL_OK);
    expect_span(plan, ids[2], at, chords[2].length, "V");
    plan_destroy(plan);
}

static void test_refused(void) {
    NodeId section;
    Plan *plan = new_plan(&section);
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

/* The chords of one bar of text in a key of `word`, read as a request does: the status, with the message in `error`. */
static UrsatzStatus read_text(const char *word, const char *text, GivenChord *chords, char numerals[][PLAN_ROMAN_NUMERAL_MAX], size_t *count,
                              UrsatzError *error) {
    char json[160];
    snprintf(json, sizeof(json), "[{\"bar\": 0, \"text\": \"%s\"}]", text);
    cJSON *list = cJSON_Parse(json);
    assert(list != NULL);
    PlanMeter meter;
    memset(&meter, 0, sizeof(meter));
    meter.beats_per_bar = 4;
    meter.beat_unit = 4;
    SpelledKey key = test_key(0, word);
    memset(error, 0, sizeof(*error));
    *count = 0;
    UrsatzStatus status = request_read_chords(test_composer_chords(), list, "/chords", "the theme", 0, &meter, &key, 4, chords, numerals, 8,
                                              count, error);
    cJSON_Delete(list);
    return status;
}

/* The one span `text` gives in a plan of the key: its numeral and the key word of the material it borrows from ("" for none). */
static void expect_mode(const char *word, const char *text, const char *numeral, const char *from) {
    GivenChord chords[8];
    char numerals[8][PLAN_ROMAN_NUMERAL_MAX];
    size_t count;
    UrsatzError error;
    assert(read_text(word, text, chords, numerals, &count, &error) == URSATZ_OK && count == 1);
    NodeId section;
    Plan *plan = new_plan_in(word, &section);
    LaneSpanId ids[8];
    assert(plan_give_chords_in(plan, test_composer_chords(), section, chords, count) == GIVEN_OK);
    assert(harmony_spans(plan, section, ids, 8) == 1);
    const PlanHarmony *harmony = &plan_lane_span_payload(plan, ids[0])->harmony;
    assert(strcmp(harmony->roman_numeral, numeral) == 0);
    assert(from[0] == '\0' ? harmony->borrowed_from == NULL
                           : harmony->borrowed_from != NULL && strcmp(harmony->borrowed_from->key_word, from) == 0);
    plan_destroy(plan);
}

static void expect_refusal(const char *word, const char *text, const char *message) {
    GivenChord chords[8];
    char numerals[8][PLAN_ROMAN_NUMERAL_MAX];
    size_t count;
    UrsatzError error;
    if (read_text(word, text, chords, numerals, &count, &error) != URSATZ_ERR_REQUEST || strstr(error.message, message) == NULL) {
        fprintf(stderr, "%s '%s': %s\n", word, text, error.message);
    }
    assert(strstr(error.message, message) != NULL);
}

static void test_modes(void) {
    /* IV in C minor from dorian is F major (the key's own iv is minor); II in C major from lydian is D major (its own ii is minor). */
    expect_mode("minor", "IV:dorian", "IV", "dorian");
    expect_mode("major", "II:lydian", "II", "lydian");
    /* A mode that holds the key's own chord gives it, borrowed from nothing. */
    expect_mode("major", "IV:dorian", "IV", "");
    /* Refused: the key's own mode (by its own word or the other), and a diminished triad. */
    expect_refusal("minor", "I:minor", "minor is the key's own mode; write the numeral without it");
    expect_refusal("major", "IV:lydian", "the triad on IV in lydian is not a major or minor chord");
    expect_refusal("major", "VII:major", "major is the key's own mode");
}

int main(void) {
    test_exact();
    test_modes();
    test_refused();
    printf("test_given_chords: ok\n");
    return 0;
}
