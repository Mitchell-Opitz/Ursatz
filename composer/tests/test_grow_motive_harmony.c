#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/given.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_state.h>

#include "support/test_support.h"
#include "util/plan_util.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Harmony from developed motives (B2 of P36): the harmony stage keeps the chords development gave each idea's motive and
 * fills only the cells it leaves open. Over seeds 1 to 10 the Test Motif as an 8-bar sentence in C major 6/8: the basic idea
 * is I (its last chord may be V when the repetition answers in the dominant), the repetition opens on I (exact), V (dominant
 * form) or the second degree (a sequence) by its ref, each fragment opens on the chord of its motive, and the cadence is a PAC.
 * The 2-bar 3/4 A minor motif as an 8-bar period: the antecedent's and the consequent's basic ideas open on the tonic. With
 * the chords of motif-realization.md given, the theme's chords are the given ones.
 */

#define SEEDS 10
#define GIVEN 9
#define HARMONY_TEST_CHORDS 64

static Motive load_motive(const char *path) {
    char *text = test_read_file(path, NULL);
    Motive motive;
    MotiveError error;
    memset(&error, 0, sizeof(error));
    assert(text);
    assert(motive_from_json(text, NULL, &motive, &error) == MOTIVE_OK);
    free(text);
    return motive;
}

static NodeId find_node(const PiecePlan *plan, const char *name) {
    for (NodeId id = 1; id <= plan_node_count(plan); id++) {
        const char *found = plan_node_name(plan, id);
        if (found && strcmp(found, name) == 0) {
            return id;
        }
    }
    return PLAN_NODE_NONE;
}

/* The scale degree (1 to 7) a chord numeral names, whatever its case or figures. */
static int degree_of(const char *numeral) {
    char lower[8];
    size_t n = 0;
    while (numeral[n] && strchr("IVivx", numeral[n]) && n < 7) {
        lower[n] = (char)tolower((unsigned char)numeral[n]);
        n++;
    }
    lower[n] = '\0';
    static const char *const NAMES[] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    for (int d = 0; d < 7; d++) {
        if (strcmp(lower, NAMES[d]) == 0) {
            return d + 1;
        }
    }
    assert(!"a chord numeral");
    return 0;
}

/* The numeral of the harmony span sounding at the time. */
static const char *chord_at(const PiecePlan *plan, Rational at) {
    LaneSpanId ids[4];
    PlanSpan here = {at, {1, 4096}};
    assert(plan_lane_find(plan, PLAN_LANE_HARMONY, 0, here, ids, 4) >= 1);
    return plan_lane_span_payload(plan, ids[0])->harmony.roman_numeral;
}

static const PlanMotiveRef *ref_of_node(const PiecePlan *plan, NodeId idea, LaneSpanId *span_out) {
    LaneSpanId span;
    assert(plan_melody_span_of(plan, idea, &span));
    if (span_out) {
        *span_out = span;
    }
    for (size_t i = 0; i < plan_lane_span_constraint_count(plan, span); i++) {
        const PlanConstraint *constraint = plan_lane_span_constraint_at(plan, span, i);
        if (constraint->kind == PLAN_CONSTRAINT_MOTIVE_REF) {
            return &constraint->params.motive;
        }
    }
    return NULL;
}

static const PlanMotiveTones *tones_of(const PiecePlan *plan, NodeId node) {
    const PlanConstraint *constraint = plan_node_find_constraint(plan, node, PLAN_CONSTRAINT_MOTIVE_TONES);
    return constraint ? &constraint->params.motive_tones : NULL;
}

static PiecePlan *compose(const Motive *motive, unsigned long seed, bool sentence, GivenChord *chords, size_t chord_count) {
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.bars = 8;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    if (sentence) {
        request.key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
        request.meter.beats_per_bar = 6;
        request.meter.beat_unit = 8;
        request.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
        request.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
    } else {
        request.key = plan_key_of_pitch_class(9, KEY_STRUCTURE_MODE_MINOR);
        request.meter.beats_per_bar = 3;
        request.meter.beat_unit = 4;
        request.pairing.beginning = PLAN_THEME_BEGINNING_ANTECEDENT;
        request.pairing.ending = PLAN_THEME_ENDING_CONSEQUENT;
        request.first_cadence = CADENCE_TYPE_HALF;
    }
    request.chords = chords;
    request.chord_count = chord_count;
    PiecePlan *plan = NULL;
    GrowOutcome outcome;
    GrowConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.basic = motive;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    GrowResult result = grow_run(plan, &config, &outcome);
    if (result != GROW_OK) {
        grow_outcome_destroy(&outcome);
        plan_destroy(plan);
        return NULL;
    }
    grow_outcome_destroy(&outcome);
    return plan;
}

/* The bars of 3/4 from the theme's start. */
static Rational bars_of_three_four(long bars) {
    return rational_ratio(3 * bars, 4);
}

/* The sentence's ideas: the chords of the basic idea and the repetition, the fragments and the cadence. */
static void check_sentence(const PiecePlan *plan, size_t seen[3]) {
    NodeId basic = find_node(plan, "basic idea");
    NodeId repetition = find_node(plan, "repetition");
    assert(basic != PLAN_NODE_NONE && repetition != PLAN_NODE_NONE);
    const PlanMotiveRef *ref = ref_of_node(plan, repetition, NULL);
    assert(ref);
    bool dominant = ref->has_step && ref->step.kind == MOTIVE_RELATION_DOMINANT_FORM;

    /* The basic idea is the tonic, but for a last chord on V when the repetition answers in the dominant. */
    PlanSpan basic_span = plan_node_span(plan, basic);
    LaneSpanId chords[16];
    size_t count = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, basic_span, chords, 16);
    assert(count >= 1 && count <= 16);
    for (size_t i = 0; i < count; i++) {
        int degree = degree_of(plan_lane_span_payload(plan, chords[i])->harmony.roman_numeral);
        assert(degree == 1 || (dominant && i + 1 == count && degree == 5));
    }

    /* The repetition's first chord is its ref's: exact I, dominant form V, a sequence the second degree. */
    int first = degree_of(chord_at(plan, plan_node_span(plan, repetition).start));
    if (ref->has_step && ref->step.kind == MOTIVE_RELATION_DOMINANT_FORM) {
        assert(first == 5);
        seen[1]++;
    } else if (ref->has_step && ref->step.kind == MOTIVE_RELATION_TRANSPOSE && ref->step.value[0] != 0) {
        assert(first == (int)(((1 - 1 + ref->step.value[1]) % 7 + 7) % 7) + 1);
        seen[2]++;
    } else {
        assert(first == 1);
        seen[0]++;
    }

    /* Each fragment opens on the chord its motive has at its start. */
    static const char *const FRAGMENTS[2] = {"fragment 1", "fragment 2"};
    for (size_t f = 0; f < 2; f++) {
        NodeId idea = find_node(plan, FRAGMENTS[f]);
        if (idea == PLAN_NODE_NONE) {
            continue;
        }
        const PlanMotiveRef *fragment = ref_of_node(plan, idea, NULL);
        const PlanMotiveTones *tones = fragment ? tones_of(plan, fragment->motive) : NULL;
        assert(tones && tones->chord_count >= 1);
        assert(degree_of(chord_at(plan, plan_node_span(plan, idea).start)) == degree_of(tones->chords[0].numeral));
    }

    /* The cadence is a PAC: the dominant, then the tonic, on the last chord. */
    PlanSpan theme = plan_node_span(plan, find_node(plan, "phrase"));
    LaneSpanId all[HARMONY_TEST_CHORDS];
    size_t total = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, theme, all, HARMONY_TEST_CHORDS);
    assert(total >= 2 && total <= HARMONY_TEST_CHORDS);
    assert(degree_of(plan_lane_span_payload(plan, all[total - 1])->harmony.roman_numeral) == 1);
    assert(degree_of(plan_lane_span_payload(plan, all[total - 2])->harmony.roman_numeral) == 5);
}

int main(void) {
    size_t seen[3] = {0, 0, 0}; /* exact, dominant form, sequence */
    size_t composed = 0;
    Motive test_motif = load_motive(COMPOSER_FIXTURE_DIR "/motifs/test-motif-6-8.json");
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        PiecePlan *plan = compose(&test_motif, seed, true, NULL, 0);
        if (!plan) {
            continue;
        }
        check_sentence(plan, seen);
        composed++;
        plan_destroy(plan);
    }
    assert(composed >= 8 && seen[0] > 0);

    /* The 2-bar 3/4 period: both basic ideas open on the motif's base harmony. */
    Motive period = load_motive(COMPOSER_FIXTURE_DIR "/rates/two-bar-3-4.json");
    size_t periods = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        PiecePlan *plan = compose(&period, seed, false, NULL, 0);
        if (!plan) {
            continue;
        }
        assert(degree_of(chord_at(plan, rational_ratio(0, 1))) == 1);
        assert(degree_of(chord_at(plan, bars_of_three_four(4))) == 1);
        periods++;
        plan_destroy(plan);
    }
    assert(periods >= 8);

    /* Given chords stay as given: the theme's chords are the ones the plan was asked for. */
    static const char *const NUMERALS[GIVEN] = {"I", "vi", "IV", "V", "I", "vi", "IV", "V", "I"};
    static const long long STARTS[GIVEN] = {0, 6, 12, 18, 24, 30, 36, 39, 42};
    static const long long LENGTHS[GIVEN] = {6, 6, 6, 6, 6, 6, 3, 3, 6};
    size_t given_runs = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        GivenChord chords[GIVEN];
        for (size_t i = 0; i < GIVEN; i++) {
            chords[i].numeral = NUMERALS[i];
            chords[i].onset = rational_ratio(STARTS[i], 8);
            chords[i].length = rational_ratio(LENGTHS[i], 8);
        }
        PiecePlan *plan = compose(&test_motif, seed, true, chords, GIVEN);
        if (!plan) {
            continue;
        }
        Rational origin = plan_node_span(plan, find_node(plan, "phrase")).start;
        for (size_t i = 0; i < GIVEN; i++) {
            Rational at;
            assert(rational_add(origin, chords[i].onset, &at) == RATIONAL_OK);
            assert(strcmp(chord_at(plan, at), NUMERALS[i]) == 0);
        }
        given_runs++;
        plan_destroy(plan);
    }
    assert(given_runs >= 8);
    printf("test_grow_motive_harmony: ok (%zu sentences: %zu exact, %zu dominant, %zu sequence; %zu periods; %zu given)\n",
           composed, seen[0], seen[1], seen[2], periods, given_runs);
    return 0;
}
