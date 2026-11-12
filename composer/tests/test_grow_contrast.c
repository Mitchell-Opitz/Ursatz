#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>

#include "api/api_grow.h"
#include "support/test_support.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The contrast distance of a theme (B1 of L4, R-475, R-476): s77 (C major) and u51 (C minor), two bars of 4/4 each, in an
 * 8-bar period (antecedent and consequent, a PAC after a half cadence) and an 8-bar sentence. A distant contrast is drawn
 * through GrowContext.contrast and read in the plan: the node `distant for m<k>` and the melody of the idea it makes.
 */

#define SEEDS 8
#define START_SEEDS 16

/* "fast" runs seed 1 of every case (test_grow_contrast_smoke); no argument runs them all (slow). */
static bool fast_run = false;

static unsigned long seeds(void) {
    return fast_run ? 1 : SEEDS;
}

static unsigned long start_seeds(void) {
    return fast_run ? 1 : START_SEEDS;
}

typedef struct {
    const char *name;
    Motive motive;
} Fixture;

static Fixture FIXTURES[2];

static Motive load_motive(const char *path) {
    char *text = test_read_file(path, NULL);
    Motive motive;
    MotiveError error;
    memset(&error, 0, sizeof(error));
    MotiveResult result = motive_from_json(text, NULL, &motive, &error);
    if (result != MOTIVE_OK) {
        fprintf(stderr, "%s: cannot read the motif at %s: %s\n", path, error.location, error.message);
    }
    assert(result == MOTIVE_OK);
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

/* The 8-bar period or sentence of the motive and seed under the contrast settings (NULL for none): the plan after grow_run. */
static PiecePlan *grow_with(const Motive *motive, unsigned long seed, bool period, const GrowContrast *contrast) {
    GrowRequest request;
    GrowConfig config;
    GrowOutcome outcome;
    PiecePlan *plan = NULL;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.key = motive_key(motive);
    request.meter = motive_meter(motive);
    request.bars = 8;
    request.pairing.beginning = period ? PLAN_THEME_BEGINNING_ANTECEDENT : PLAN_THEME_BEGINNING_PRESENTATION;
    request.pairing.ending = period ? PLAN_THEME_ENDING_CONSEQUENT : PLAN_THEME_ENDING_CONTINUATION;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    request.first_cadence = CADENCE_TYPE_HALF;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.basic = motive;
    if (contrast) {
        request.contrast = *contrast;
        config.context.contrast = *contrast;
    }
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    GrowResult result = grow_run(plan, &config, &outcome);
    if (result != GROW_OK) {
        plan_destroy(plan);
        return NULL;
    }
    grow_outcome_destroy(&outcome);
    return plan;
}

static char *plan_text(const PiecePlan *plan) {
    PlanJsonError error;
    char *text = plan_to_json(plan, &error);
    assert(text);
    return text;
}

/* No distance setting changes nothing: {1, 0} and {0, 0} with new material off are the plan of a zero context. */
static void test_defaults_change_nothing(void) {
    GrowContrast similar = {{1, 0}, 0};
    GrowContrast zero = {{0, 0}, 0};
    for (size_t f = 0; f < 2; f++) {
        for (int period = 0; period < 2; period++) {
            for (unsigned long seed = 1; seed <= seeds(); seed++) {
                PiecePlan *plain = grow_with(&FIXTURES[f].motive, seed, period, NULL);
                PiecePlan *with_similar = grow_with(&FIXTURES[f].motive, seed, period, &similar);
                PiecePlan *with_zero = grow_with(&FIXTURES[f].motive, seed, period, &zero);
                assert((plain == NULL) == (with_similar == NULL) && (plain == NULL) == (with_zero == NULL));
                if (plain) {
                    char *a = plan_text(plain);
                    char *b = plan_text(with_similar);
                    char *c = plan_text(with_zero);
                    assert(strcmp(a, b) == 0 && strcmp(a, c) == 0);
                    free(a);
                    free(b);
                    free(c);
                    plan_destroy(plain);
                    plan_destroy(with_similar);
                    plan_destroy(with_zero);
                }
            }
        }
    }
}

typedef struct {
    Rational onset;
    Rational duration;
} Beat;

/* The notes of the theme's melody that start in the bars [from, to) of the 4/4 theme (0-based), their onsets counted from bar from. */
static size_t melody_rhythm(const PiecePlan *plan, long from, long to, Beat *out, size_t size) {
    NodeId theme = find_node(plan, "theme");
    LaneSpanId spans[64];
    size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, plan_node_span(plan, theme), spans, 64);
    Rational origin = plan_node_span(plan, theme).start;
    Rational first;
    assert(rational_add(origin, rational_ratio(from, 1), &first) == RATIONAL_OK);
    size_t count = 0;
    for (size_t s = 0; s < found; s++) {
        PlanSpan span = plan_lane_span_span(plan, spans[s]);
        const PlanNotes *notes = &plan_lane_span_payload(plan, spans[s])->notes;
        for (size_t i = 0; i < notes->note_count; i++) {
            Rational onset;
            Rational relative;
            Rational whole_bars;
            long long bar = 0;
            assert(rational_add(span.start, notes->notes[i].offset, &onset) == RATIONAL_OK);
            assert(rational_sub(onset, origin, &whole_bars) == RATIONAL_OK);
            assert(rational_floor(whole_bars, &bar) == RATIONAL_OK);
            if (bar < from || bar >= to || count >= size) {
                continue;
            }
            assert(rational_sub(onset, first, &relative) == RATIONAL_OK);
            out[count].onset = relative;
            out[count].duration = notes->notes[i].duration;
            count++;
        }
    }
    /* The spans are not in time order. */
    for (size_t i = 1; i < count; i++) {
        for (size_t j = i; j > 0 && rational_cmp(out[j - 1].onset, out[j].onset) > 0; j--) {
            Beat swap = out[j];
            out[j] = out[j - 1];
            out[j - 1] = swap;
        }
    }
    return count;
}

/* Whether two rhythms are the same over the first `common` bars. */
static bool same_over(const Beat *a, size_t a_count, const Beat *b, size_t b_count, long common) {
    Rational limit = rational_ratio(common, 1);
    size_t i = 0;
    size_t j = 0;
    while (i < a_count && j < b_count && rational_cmp(a[i].onset, limit) < 0 && rational_cmp(b[j].onset, limit) < 0) {
        if (rational_cmp(a[i].onset, b[j].onset) != 0 || rational_cmp(a[i].duration, b[j].duration) != 0) {
            return false;
        }
        i++;
        j++;
    }
    return !(i < a_count && rational_cmp(a[i].onset, limit) < 0) && !(j < b_count && rational_cmp(b[j].onset, limit) < 0);
}

/* A distant contrast changes the rhythm of the idea it makes: against the basic idea's (bars 0 to 2) over their common length. */
static void test_distant_changes_rhythm(void) {
    static const char *const NAMES[3] = {"contrasting idea", "consequent contrasting idea", "sentence fragment 1"};
    GrowContrast distant = {{0, 1}, 0};
    for (size_t f = 0; f < 2; f++) {
        size_t made[3] = {0, 0, 0};
        size_t composed[3] = {0, 0, 0};
        for (int period = 0; period < 2; period++) {
            for (unsigned long seed = 1; seed <= seeds(); seed++) {
                PiecePlan *plan = grow_with(&FIXTURES[f].motive, seed, period, &distant);
                if (!plan) {
                    continue;
                }
                Beat basic[64];
                size_t basic_count = melody_rhythm(plan, 0, 2, basic, 64);
                /* The idea's bars and its place: a period's contrast is bars 2 and 3, the consequent's bars 6 and 7, a sentence's fragment 1 bar 4. */
                static const long FROM[3] = {2, 6, 4};
                static const long TO[3] = {4, 8, 5};
                static const char *const NODE[3] = {"distant for m1", "distant for m3", "distant for m2"};
                for (size_t k = 0; k < 3; k++) {
                    if ((k < 2) != (period != 0)) {
                        continue;
                    }
                    composed[k]++;
                    if (find_node(plan, NODE[k]) == PLAN_NODE_NONE) {
                        continue;
                    }
                    made[k]++;
                    Beat idea[64];
                    size_t idea_count = melody_rhythm(plan, FROM[k], TO[k], idea, 64);
                    long common = TO[k] - FROM[k];
                    assert(!same_over(basic, basic_count, idea, idea_count, common));
                }
                plan_destroy(plan);
            }
        }
        for (size_t k = 0; k < 3; k++) {
            printf("%s: distant %s made in %zu of %zu composed (%lu seeds)\n", FIXTURES[f].name, NAMES[k], made[k],
                   composed[k], seeds());
            /* Measured: every seed composes and makes one (a 4-bar 3/4 theme makes none, motive_random has no one-bar 3/4 motive). */
            assert(composed[k] == seeds() && made[k] == seeds());
        }
    }
}

/* Distance {1, 1} and {0, 1} compose on both motifs, in both pairings, from every start seed. */
static void test_distant_composes(void) {
    static const GrowContrast SETTINGS[2] = {{{1, 1}, 0}, {{0, 1}, 0}};
    for (size_t s = 0; s < 2; s++) {
        for (size_t f = 0; f < 2; f++) {
            for (int period = 0; period < 2; period++) {
                unsigned composed = 0;
                for (unsigned long seed = 1; seed <= start_seeds(); seed++) {
                    GrowBriefText text;
                    GrowSetup setup;
                    memset(&text, 0, sizeof(text));
                    text.key = f == 0 ? "C major" : "C minor";
                    text.meter = "4/4";
                    text.cadence = "PAC";
                    text.into = period ? "period" : "sentence";
                    memset(&setup, 0, sizeof(setup));
                    setup.text = &text;
                    setup.into = period ? GROW_INTO_PERIOD : GROW_INTO_SENTENCE;
                    setup.pairing.beginning = period ? PLAN_THEME_BEGINNING_ANTECEDENT : PLAN_THEME_BEGINNING_PRESENTATION;
                    setup.pairing.ending = period ? PLAN_THEME_ENDING_CONSEQUENT : PLAN_THEME_ENDING_CONTINUATION;
                    setup.key = motive_key(&FIXTURES[f].motive);
                    setup.meter = motive_meter(&FIXTURES[f].motive);
                    setup.bars = 8;
                    setup.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
                    setup.first_cadence = CADENCE_TYPE_HALF;
                    setup.motive = &FIXTURES[f].motive;
                    setup.suggest = 1;
                    setup.seed = seed;
                    setup.tempo_from = "none";
                    setup.contrast = &SETTINGS[s];
                    GrowRun run;
                    ComposerError error;
                    memset(&error, 0, sizeof(error));
                    composed += api_grow_run(&setup, &run, &error) == COMPOSER_OK && run.made == 1 ? 1 : 0;
                    api_grow_run_destroy(&run);
                }
                printf("distance {%u, %u} %s %s: %u of %d composed\n", SETTINGS[s].distance[0], SETTINGS[s].distance[1],
                       FIXTURES[f].name, period ? "period" : "sentence", composed, (unsigned)start_seeds());
                assert(composed == start_seeds());
            }
        }
    }
}

int main(int argc, char **argv) {
    fast_run = argc > 1 && strcmp(argv[1], "fast") == 0;
    FIXTURES[0].name = "s77";
    FIXTURES[0].motive = load_motive(COMPOSER_FIXTURE_DIR "/continuation/s77.json");
    FIXTURES[1].name = "u51";
    FIXTURES[1].motive = load_motive(COMPOSER_FIXTURE_DIR "/continuation/u51.json");
    test_defaults_change_nothing();
    test_distant_changes_rhythm();
    test_distant_composes();
    return 0;
}
