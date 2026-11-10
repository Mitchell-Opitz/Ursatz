#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_composer/grow.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/plan_state.h>

/*
 * A sentence of 8 bars that ends in a half cadence, in three minor keys and meters, seeds 1 to 4. A minor half cadence has
 * two chords before its dominant (i and iv) and no dominant before the last chord, so a continuation whose chords were
 * capped to fewer than the cells need could not be dealt and the harmony stage rejected every attempt (P44 B6, R-418).
 * Each run must complete. A grow request needs a motive, so it is given here; the pieces that invent one are in
 * test_piece_structure and test_piece_keys.
 */

typedef struct {
    const char *name;
    int tonic;
    int beats;
} Scenario;

static const Scenario SCENARIOS[] = {
    {"A minor 3/4", 9, 3},
    {"A minor 4/4", 9, 4},
    {"D minor 4/4", 2, 4},
};

#define SEEDS 4

static void run(const Scenario *scenario, unsigned long seed) {
    Motive motive;
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter.beats_per_bar = scenario->beats;
    options.meter.beat_unit = 4;
    assert(motive_from_notes_text("D4:q F4:e A4:e D5:q", &options, &motive, NULL) == MOTIVE_OK);

    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = &motive;
    request.key = plan_key_of_pitch_class(scenario->tonic, KEY_STRUCTURE_MODE_MINOR);
    request.meter = options.meter;
    request.bars = 8;
    request.pairing.beginning = PLAN_THEME_BEGINNING_PRESENTATION;
    request.pairing.ending = PLAN_THEME_ENDING_CONTINUATION;
    request.cadence = CADENCE_TYPE_HALF;

    PiecePlan *plan = NULL;
    GrowConfig config;
    GrowOutcome outcome;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    assert(grow_build_plan(&request, &plan) == GROW_OK);
    GrowResult result = grow_run(plan, &config, &outcome);
    if (result != GROW_OK) {
        fprintf(stderr, "%s, seed %lu: %s at %s\n", scenario->name, seed, grow_result_name(result),
                grow_stage_name(outcome.stuck_stage));
    }
    assert(result == GROW_OK && !outcome.stuck);
    grow_outcome_destroy(&outcome);
    plan_destroy(plan);
}

int main(void) {
    size_t runs = 0;
    for (size_t s = 0; s < sizeof(SCENARIOS) / sizeof(SCENARIOS[0]); s++) {
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            run(&SCENARIOS[s], seed);
            runs++;
        }
        printf("%s: %d of %d sentences ending HC grow\n", SCENARIOS[s].name, SEEDS, SEEDS);
    }
    printf("%zu runs\n", runs);
    return 0;
}
