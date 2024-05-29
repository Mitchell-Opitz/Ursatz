#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <texture_rules.h>

#include <ursatz_composer/grow.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/plan_state.h>

#include "accompaniment_check.h"
#include "texture_heard.h"

/*
 * Themes grown with an accompaniment, seeds 1 to 10, in C major 4/4 and D minor 3/4, for each pattern. Run with no
 * argument, each must complete and every hard rule must hold in the plan it leaves (accompaniment_check.h, worked out
 * from the lanes and not from the operator), and the same seed must give the same plan. Run with "heard", themes are
 * grown to be heard as melody and accompaniment and the share that standard-texture hears so in every measure is
 * printed; below 100% the run fails, and that failure is expected (docs/accompaniment.md).
 */

#define SEEDS 10

typedef struct {
    const char *name;
    int tonic;
    PlanKeyMode mode;
    int beats;
    int unit;
    const char *motive;
} Scenario;

static const Scenario SCENARIOS[] = {
    {"C major 4/4", 0, PLAN_MODE_MAJOR, 4, 4, "E4:q G4:e F4:e D4:q C4:q"},
    {"D minor 3/4", 2, PLAN_MODE_MINOR, 3, 4, "F4:q A4:e G4:e E4:q"},
};
#define SCENARIO_COUNT (sizeof(SCENARIOS) / sizeof(SCENARIOS[0]))

static const PlanAccompanimentPattern PATTERNS[] = {PLAN_ACCOMPANIMENT_BLOCK, PLAN_ACCOMPANIMENT_ARPEGGIATED,
                                                    PLAN_ACCOMPANIMENT_ALBERTI, PLAN_ACCOMPANIMENT_OOM_PAH,
                                                    PLAN_ACCOMPANIMENT_ANY};
static const char *const PATTERN_NAMES[] = {"block", "arpeggiated", "alberti", "oom-pah", "any"};
#define PATTERN_COUNT (sizeof(PATTERNS) / sizeof(PATTERNS[0]))

static Motive make_motive(const Scenario *scenario) {
    Motive motive;
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = scenario->beats;
    options.meter.beat_unit = scenario->unit;
    assert(motive_from_notes_text(scenario->motive, &options, &motive, NULL) == MOTIVE_OK);
    return motive;
}

static GrowRequest make_request(const Scenario *scenario, const Motive *motive, PlanAccompaniment accompaniment) {
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.key.tonic_pitch_class = scenario->tonic;
    request.key.mode = scenario->mode;
    request.meter.beats_per_bar = scenario->beats;
    request.meter.beat_unit = scenario->unit;
    request.bars = 8;
    request.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
    request.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
    request.cadence = PLAN_CADENCE_PAC;
    request.has_accompaniment = true;
    request.accompaniment = accompaniment;
    return request;
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

static GrowResult grow(const GrowRequest *request, unsigned long seed, PiecePlan **plan, GrowOutcome *outcome) {
    GrowConfig config;
    memset(&config, 0, sizeof(config));
    memset(outcome, 0, sizeof(*outcome));
    config.seed = seed;
    assert(grow_build_plan(request, plan) == GROW_OK);
    return grow_run(*plan, &config, outcome);
}

/* The voices a written theme has: 1 to 3, the notes in each, and whether any span is still open. */
static void voice_notes(const PiecePlan *plan, PlanSpan span, size_t notes[3], size_t spans[3]) {
    for (int voice = 1; voice <= 3; voice++) {
        LaneSpanId ids[64];
        size_t found = plan_lane_find(plan, PLAN_LANE_INNER_VOICE, voice, span, ids, 64);
        assert(found <= 64);
        notes[voice - 1] = 0;
        spans[voice - 1] = found;
        for (size_t i = 0; i < found; i++) {
            assert(plan_lane_span_state(plan, ids[i]) == PLAN_STATE_REALIZED);
            notes[voice - 1] += plan_lane_span_payload(plan, ids[i])->notes.note_count;
        }
    }
}

static void test_hard_rules(void) {
    size_t themes = 0;
    size_t checked = 0;
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        Motive motive = make_motive(&SCENARIOS[s]);
        for (size_t p = 0; p < PATTERN_COUNT; p++) {
            PlanAccompaniment accompaniment;
            memset(&accompaniment, 0, sizeof(accompaniment));
            accompaniment.pattern = PATTERNS[p];
            accompaniment.texture = -1;
            GrowRequest request = make_request(&SCENARIOS[s], &motive, accompaniment);
            for (unsigned long seed = 1; seed <= SEEDS; seed++) {
                PiecePlan *plan = NULL;
                GrowOutcome outcome;
                GrowResult result = grow(&request, seed, &plan, &outcome);
                if (result != GROW_OK) {
                    fprintf(stderr, "%s, %s, seed %lu: %s (stage %s)\n", SCENARIOS[s].name, PATTERN_NAMES[p], seed,
                            grow_result_name(result), grow_stage_name(outcome.stuck_stage));
                }
                assert(result == GROW_OK && !outcome.stuck);
                assert(outcome.records[GROW_STAGE_TEXTURE] != NULL);

                NodeId theme = find_node(plan, "theme");
                PlanSpan span = plan_node_span(plan, theme);
                PlanKey key = {SCENARIOS[s].tonic, SCENARIOS[s].mode};
                checked += check_accompaniment(plan, span, key, PLAN_ACCOMPANIMENT_DEFAULT_LOW,
                                               PLAN_ACCOMPANIMENT_DEFAULT_HIGH);
                size_t notes[3];
                size_t spans[3];
                voice_notes(plan, span, notes, spans);
                assert(notes[0] > 0);
                if (PATTERNS[p] == PLAN_ACCOMPANIMENT_ARPEGGIATED || PATTERNS[p] == PLAN_ACCOMPANIMENT_ALBERTI) {
                    /* One line: no other voice has a span. */
                    assert(spans[1] == 0 && spans[2] == 0);
                } else {
                    assert(spans[1] > 0 && spans[2] > 0);
                    if (PATTERNS[p] != PLAN_ACCOMPANIMENT_ANY) {
                        assert(notes[1] > 0 && notes[2] > 0);
                    }
                }

                /* The same seed gives the same plan, accompaniment and all. */
                if (seed <= 3) {
                    PiecePlan *again = NULL;
                    GrowOutcome second;
                    assert(grow(&request, seed, &again, &second) == GROW_OK);
                    char *a = plan_to_json(plan, NULL);
                    char *b = plan_to_json(again, NULL);
                    assert(a && b && strcmp(a, b) == 0);
                    free(a);
                    free(b);
                    grow_outcome_destroy(&second);
                    plan_destroy(again);
                }
                grow_outcome_destroy(&outcome);
                plan_destroy(plan);
                themes++;
            }
        }
    }
    printf("%zu themes grown with an accompaniment, %zu notes of it held to the hard rules\n", themes, checked);
}

static void test_bad_requests(void) {
    Motive motive = make_motive(&SCENARIOS[0]);
    PlanAccompaniment accompaniment;
    PiecePlan *plan = NULL;
    memset(&accompaniment, 0, sizeof(accompaniment));
    accompaniment.pattern = PLAN_ACCOMPANIMENT_BLOCK;
    accompaniment.texture = -1;

    /* A texture that is not homophonic, a pattern that does not exist, a range with no room. */
    accompaniment.texture = STANDARD_TEXTURE_POLYPHONY;
    GrowRequest request = make_request(&SCENARIOS[0], &motive, accompaniment);
    assert(grow_build_plan(&request, &plan) == GROW_ERR_BAD_REQUEST && !plan);
    request.accompaniment.texture = -1;
    request.accompaniment.pattern = (PlanAccompanimentPattern)9;
    assert(grow_build_plan(&request, &plan) == GROW_ERR_BAD_REQUEST && !plan);
    request.accompaniment.pattern = PLAN_ACCOMPANIMENT_BLOCK;
    request.accompaniment.low = 60;
    request.accompaniment.high = 55;
    assert(grow_build_plan(&request, &plan) == GROW_ERR_BAD_REQUEST && !plan);

    /* Alberti is not written in 6/8. */
    Scenario compound = {"G major 6/8", 7, PLAN_MODE_MAJOR, 6, 8, "B4:q A4:e G4:e F#4:e D4:e"};
    Motive compound_motive = make_motive(&compound);
    accompaniment.texture = -1;
    accompaniment.pattern = PLAN_ACCOMPANIMENT_ALBERTI;
    request = make_request(&compound, &compound_motive, accompaniment);
    assert(grow_build_plan(&request, &plan) == GROW_ERR_BAD_REQUEST && !plan);
    request.accompaniment.pattern = PLAN_ACCOMPANIMENT_ANY;
    assert(grow_build_plan(&request, &plan) == GROW_OK && plan);
    plan_destroy(plan);

    /* Without the request for one, a theme has no inner voice, and the texture stage has nothing to do. */
    request = make_request(&SCENARIOS[0], &motive, accompaniment);
    request.has_accompaniment = false;
    GrowOutcome outcome;
    plan = NULL;
    assert(grow(&request, 1, &plan, &outcome) == GROW_OK);
    size_t notes[3];
    size_t spans[3];
    voice_notes(plan, plan_node_span(plan, find_node(plan, "theme")), notes, spans);
    assert(spans[0] == 0 && spans[1] == 0 && spans[2] == 0);
    assert(outcome.records[GROW_STAGE_TEXTURE] != NULL && outcome.records[GROW_STAGE_TEXTURE]->step_count == 0);
    grow_outcome_destroy(&outcome);
    plan_destroy(plan);
}

/* Themes grown to be heard as melody and accompaniment, and whether standard-texture hears them so. */
static int test_heard(void) {
    size_t themes = 0;
    size_t heard_all = 0;
    size_t measures = 0;
    size_t heard_measures = 0;
    for (size_t s = 0; s < SCENARIO_COUNT; s++) {
        Motive motive = make_motive(&SCENARIOS[s]);
        PlanAccompaniment accompaniment;
        memset(&accompaniment, 0, sizeof(accompaniment));
        accompaniment.pattern = PLAN_ACCOMPANIMENT_ANY;
        accompaniment.texture = STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT;
        GrowRequest request = make_request(&SCENARIOS[s], &motive, accompaniment);
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            PiecePlan *plan = NULL;
            GrowOutcome outcome;
            assert(grow(&request, seed, &plan, &outcome) == GROW_OK);
            NodeId theme = find_node(plan, "theme");
            PlanMeter meter = {SCENARIOS[s].beats, SCENARIOS[s].unit, 0};
            TextureHeard heard =
                texture_heard(plan, plan_node_span(plan, theme), meter, STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT);
            themes++;
            measures += heard.measures;
            heard_measures += heard.heard;
            heard_all += heard.measures > 0 && heard.heard == heard.measures;
            printf("%s, seed %2lu: %zu of %zu measures heard as melody and accompaniment (textures:", SCENARIOS[s].name,
                   seed, heard.heard, heard.measures);
            for (size_t m = 0; m < heard.measures && m < 64; m++) {
                printf(" %s", heard.textures[m] >= 0 ? standard_texture_name((StandardTexture)heard.textures[m]) : "none");
            }
            printf(")\n");
            grow_outcome_destroy(&outcome);
            plan_destroy(plan);
        }
    }
    printf("heard as melody and accompaniment in every measure: %zu of %zu themes (%.1f%%); %zu of %zu measures (%.1f%%)\n",
           heard_all, themes, 100.0 * (double)heard_all / (double)themes, heard_measures, measures,
           100.0 * (double)heard_measures / (double)measures);
    return heard_all == themes ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "heard") == 0) {
        return test_heard();
    }
    test_bad_requests();
    test_hard_rules();
    puts("test_grow_accompaniment: ok");
    return 0;
}
