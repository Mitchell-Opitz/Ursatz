#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/fill_record.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive.h>
#include <plan/plan_state.h>

#include "api/api_grow.h"
#include "support/test_support.h"
#include "structure/plan_util.h"
#include "support/test_composer.h"
#include "ursatz_test.h"
#include "support/test_knowledge.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Harmony from developed motives (B2 of P36): the harmony stage keeps the chords development gave each idea's motive and
 * fills only the cells it leaves open. Over seeds 1 to 10 the Test Motif as an 8-bar sentence in C major 6/8: the basic idea
 * is I (its last chord may be V when the repetition answers in the dominant), the repetition opens on I (exact), V (dominant
 * form) or the second degree (a sequence) by its ref, each fragment opens on the chord of its motive, and the cadence is a PAC.
 * The 2-bar 3/4 A minor motif as an 8-bar period: the antecedent's and the consequent's basic ideas open on the tonic. With
 * the chords of motif-realization.md given, the theme's chords are the given ones. A sequence presentation over a motif with harmony
 * holds the motif's chords a step up (R-348, R-714): s77-chords (I V@4 I@7, C major, 4/4) as 8-bar sentences through api_grow_run
 * with development theme, seeds 1 to 10 (measured: every seed draws the sequence; before the fix none did); the chordless s77 repetition is ii whole.
 */

#define SEEDS 10
#define GIVEN 9
#define HARMONY_TEST_CHORDS 64

static Motive load_motive(const char *path) {
    char *text = test_must_read_file(path, NULL);
    Motive motive;
    MotiveError error;
    memset(&error, 0, sizeof(error));
    assert(text);
    assert(motive_from_json(text, test_motive_options(), &motive, &error) == MOTIVE_OK);
    free(text);
    return motive;
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
static const char *chord_at(const Plan *plan, Rational at) {
    LaneSpanId ids[4];
    PlanSpan here = {at, {1, 4096}};
    assert(plan_lane_find(plan, PLAN_LANE_HARMONY, 0, here, ids, 4) >= 1);
    return plan_lane_span_payload(plan, ids[0])->harmony.roman_numeral;
}

static const PlanMotiveRef *ref_of_node(const Plan *plan, NodeId idea, LaneSpanId *span_out) {
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

static const PlanMotiveTones *tones_of(const Plan *plan, NodeId node) {
    const PlanConstraint *constraint = plan_node_find_constraint(plan, node, PLAN_CONSTRAINT_MOTIVE_TONES);
    return constraint ? &constraint->params.motive_tones : NULL;
}

static Plan *compose(const Motive *motive, unsigned long seed, bool sentence, GivenChord *chords, size_t chord_count) {
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.bars = 8;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    if (sentence) {
        request.key = test_key(0, "major");
        request.meter.beats_per_bar = 6;
        request.meter.beat_unit = 8;
        request.pairing.beginning = SECTION_BEGINNING_PRESENTATION;
        request.pairing.ending = SECTION_ENDING_CONTINUATION;
    } else {
        request.key = test_key(9, "minor");
        request.meter.beats_per_bar = 3;
        request.meter.beat_unit = 4;
        request.pairing.beginning = SECTION_BEGINNING_ANTECEDENT;
        request.pairing.ending = SECTION_ENDING_CONSEQUENT;
        request.first_cadence = CADENCE_TYPE_HALF;
    }
    request.chords = chords;
    request.chord_count = chord_count;
    Plan *plan = NULL;
    GrowOutcome outcome;
    GrowConfig config;
    memset(&config, 0, sizeof(config));
    config.knowledge = test_knowledge();
    config.seed = seed;
    config.basic = motive;
    assert(grow_build_plan_checked(&request, &plan, NULL) == GROW_OK);
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
static void check_sentence(const Plan *plan, size_t seen[3]) {
    NodeId basic = test_find_node(plan, "basic idea");
    NodeId repetition = test_find_node(plan, "repetition");
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
        NodeId idea = test_find_node(plan, FRAGMENTS[f]);
        if (idea == PLAN_NODE_NONE) {
            continue;
        }
        const PlanMotiveRef *fragment = ref_of_node(plan, idea, NULL);
        const PlanMotiveTones *tones = fragment ? tones_of(plan, fragment->motive) : NULL;
        assert(tones && tones->chord_count >= 1);
        assert(degree_of(chord_at(plan, plan_node_span(plan, idea).start)) == degree_of(tones->chords[0].numeral));
    }

    /* The cadence is a PAC: the dominant, then the tonic, on the last chord. */
    PlanSpan theme = plan_node_span(plan, test_find_node(plan, "phrase"));
    LaneSpanId all[HARMONY_TEST_CHORDS];
    size_t total = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, theme, all, HARMONY_TEST_CHORDS);
    assert(total >= 2 && total <= HARMONY_TEST_CHORDS);
    assert(degree_of(plan_lane_span_payload(plan, all[total - 1])->harmony.roman_numeral) == 1);
    assert(degree_of(plan_lane_span_payload(plan, all[total - 2])->harmony.roman_numeral) == 5);
}

/* The theme of the motive and seed through api_grow_run, development theme: the plan of the first theme made, or NULL. */
static Plan *theme_of(const Motive *motive, unsigned long seed, GrowRun *run) {
    GrowBriefText text;
    GrowSetup setup;
    UrsatzError error;
    memset(&text, 0, sizeof(text));
    text.key = "C major";
    text.meter = "4/4";
    text.cadence = "PAC";
    text.into = "sentence";
    memset(&setup, 0, sizeof(setup));
    setup.knowledge = test_knowledge();
    setup.text = &text;
    setup.into = GROW_INTO_SENTENCE;
    setup.pairing.beginning = SECTION_BEGINNING_PRESENTATION;
    setup.pairing.ending = SECTION_ENDING_CONTINUATION;
    setup.key = motive_key(motive);
    setup.meter = motive_meter(motive);
    setup.bars = 8;
    setup.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    setup.first_cadence = CADENCE_TYPE_HALF;
    setup.development = GROW_DEVELOP_THEME;
    setup.motive = motive;
    setup.suggest = 1;
    setup.seed = seed;
    setup.tempo_from = "none";
    memset(&error, 0, sizeof(error));
    if (api_grow_run(&setup, run, &error) != URSATZ_OK || run->made != 1) {
        api_grow_run_destroy(run);
        return NULL;
    }
    return run->themes[0].plan;
}

/* The degrees of the chords that start inside the idea. */
static size_t chord_degrees(const Plan *plan, NodeId idea, int degrees[8]) {
    PlanSpan span = plan_node_span(plan, idea);
    LaneSpanId ids[16];
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, 16);
    size_t count = 0;
    for (size_t i = 0; i < found && count < 8; i++) {
        Rational at = plan_lane_span_span(plan, ids[i]).start;
        if (rational_cmp(at, span.start) >= 0) {
            degrees[count++] = degree_of(plan_lane_span_payload(plan, ids[i])->harmony.roman_numeral);
        }
    }
    return count;
}

/* The repetition of a sequence holds the basic idea's chords each a step up; without chords it is ii whole. */
static void test_sequence_holds_motif_chords(void) {
    Motive chords = load_motive(COMPOSER_FIXTURE_DIR "/continuation/s77-chords.json");
    Motive plain = load_motive(COMPOSER_FIXTURE_DIR "/continuation/s77.json");
    size_t sequences = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        GrowRun run;
        Plan *plan = theme_of(&chords, seed, &run);
        if (!plan) {
            continue;
        }
        NodeId basic = test_find_node(plan, "basic idea");
        NodeId repetition = test_find_node(plan, "repetition");
        const PlanMotiveRef *ref = ref_of_node(plan, repetition, NULL);
        if (ref && ref->has_step && ref->step.kind == MOTIVE_RELATION_TRANSPOSE && ref->step.value[0] != 0 && ref->step.value[1] == 1) {
            int before[8];
            int after[8];
            size_t count = chord_degrees(plan, basic, before);
            assert(count >= 1 && chord_degrees(plan, repetition, after) == count);
            for (size_t i = 0; i < count; i++) {
                assert(after[i] == before[i] % 7 + 1);
            }
            sequences++;
        }
        api_grow_run_destroy(&run);

        /* The same notes with no chords: the sequence is written whole, I then ii. */
        plan = theme_of(&plain, seed, &run);
        if (plan) {
            ref = ref_of_node(plan, test_find_node(plan, "repetition"), NULL);
            if (ref && ref->has_step && ref->step.kind == MOTIVE_RELATION_TRANSPOSE && ref->step.value[0] != 0 && ref->step.value[1] == 1) {
                int after[8];
                assert(chord_degrees(plan, test_find_node(plan, "repetition"), after) == 1 && after[0] == 2);
            }
        }
        api_grow_run_destroy(&run);
    }
    assert(sequences >= 1);
}

int main(void) {
    test_sequence_holds_motif_chords();
    size_t seen[3] = {0, 0, 0}; /* exact, dominant form, sequence */
    size_t composed = 0;
    Motive test_motif = load_motive(COMPOSER_FIXTURE_DIR "/motifs/test-motif-6-8.json");
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        Plan *plan = compose(&test_motif, seed, true, NULL, 0);
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
        Plan *plan = compose(&period, seed, false, NULL, 0);
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
        Plan *plan = compose(&test_motif, seed, true, chords, GIVEN);
        if (!plan) {
            continue;
        }
        Rational origin = plan_node_span(plan, test_find_node(plan, "phrase")).start;
        for (size_t i = 0; i < GIVEN; i++) {
            Rational at;
            assert(rational_add(origin, chords[i].onset, &at) == RATIONAL_OK);
            assert(strcmp(chord_at(plan, at), NUMERALS[i]) == 0);
        }
        given_runs++;
        plan_destroy(plan);
    }
    assert(given_runs >= 8);
    /* A motif with no harmony of its own: its developed motives carry chords proposed from the notes, which are not held,
       so the harmony stage never writes a step by harmony-motive (R-348). */
    Motive chordless;
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.knowledge = test_knowledge();
    options.has_meter = true;
    options.meter.beats_per_bar = 4;
    options.meter.beat_unit = 4;
    options.has_key = true;
    options.key = test_key(2, "minor");
    assert(motive_from_degrees_text("1:e 3:e 5:e 3:e 1:e 3:e 5:e 1':e", &options, &chordless, NULL) == MOTIVE_OK);
    assert(chordless.chord_count == 0);
    size_t chordless_runs = 0;
    for (unsigned long seed = 1; seed <= SEEDS; seed++) {
        GrowRequest request;
        memset(&request, 0, sizeof(request));
        request.motive = &chordless;
        request.key = options.key;
        request.meter = options.meter;
        request.bars = 8;
        request.pairing.beginning = SECTION_BEGINNING_PRESENTATION;
        request.pairing.ending = SECTION_ENDING_CONTINUATION;
        request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
        Plan *plan = NULL;
        GrowOutcome outcome;
        GrowConfig config;
        memset(&config, 0, sizeof(config));
        config.knowledge = test_knowledge();
        config.seed = seed;
        config.basic = &chordless;
        assert(grow_build_plan_checked(&request, &plan, NULL) == GROW_OK);
        if (grow_run(plan, &config, &outcome) == GROW_OK) {
            const FillRecord *record = outcome.records[GROW_STAGE_HARMONY];
            assert(record && record->step_count > 0);
            for (size_t i = 0; i < record->step_count; i++) {
                for (size_t c = 0; c < record->steps[i].considered_count; c++) {
                    assert(strcmp(record->steps[i].considered[c].operator_name, "harmony-motive") != 0);
                }
            }
            chordless_runs++;
        }
        grow_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    assert(chordless_runs >= 8);
    printf("test_grow_motive_harmony: ok (%zu sentences: %zu exact, %zu dominant, %zu sequence; %zu periods; %zu given; %zu chordless)\n",
           composed, seen[0], seen[1], seen[2], periods, given_runs, chordless_runs);
    return 0;
}
