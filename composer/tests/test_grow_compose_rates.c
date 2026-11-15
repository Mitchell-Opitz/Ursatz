#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/plan_state.h>

#include "api/api_grow.h"
#include "grow/grow_internal.h"
#include "support/test_support.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Compose rates (B4 of P33, R-317): for each of the 9 pairings, 4 and 8 bars and the two reference motifs
 * (tests/fixtures/rates), with no chords given, no check and suggest 1, how many of the start seeds 1 to 10 compose
 * (api_grow_run returns a theme within GROW_MAX_SEED_TRIES of the start seed). Cases the pairing cannot be built in are
 * printed as "not buildable". A table of case, composed of 10 and the stages the failures were stuck in is printed. Each
 * buildable case composes at least 8 of 10. With the argument "fast" only start seed 1 is run and it must compose.
 * Each case is run for the three development settings (B3 of P34), and each must hold.
 * Composing, not being heard as requested: docs/grow-hearing.md.
 */

#define MIN_COMPOSED 8
#define START_SEEDS 10

typedef struct {
    const char *name;
    const char *file;
    const char *key;
    const char *meter;
} Reference;

static const Reference REFERENCES[] = {{"two-bar-3-4", COMPOSER_FIXTURE_DIR "/rates/two-bar-3-4.json", "A minor", "3/4"},
                                       {"one-bar-4-4", COMPOSER_FIXTURE_DIR "/rates/one-bar-4-4.json", "C major", "4/4"}};

static const char *const BEGINNINGS[] = {"presentation", "antecedent", "compound"};
static const char *const ENDINGS[] = {"continuation", "consequent", "cadential"};

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

static GrowStage stage_of_name(const char *name) {
    for (int stage = 0; stage < GROW_STAGE_COUNT; stage++) {
        if (strcmp(grow_stage_name((GrowStage)stage), name) == 0) {
            return (GrowStage)stage;
        }
    }
    return GROW_STAGE_COUNT;
}

/* Runs one case from one start seed: whether it composed, and the stage of the last seed passed over. */
static bool compose_from(GrowSetup *setup, unsigned long seed, int stuck[GROW_STAGE_COUNT + 1]) {
    GrowRun run;
    ComposerError error;
    memset(&error, 0, sizeof(error));
    setup->seed = seed;
    ComposerStatus status = api_grow_run(setup, &run, &error);
    bool composed = status == COMPOSER_OK && run.made == 1;
    if (!composed) {
        GrowStage stage = run.passed_count > 0 ? stage_of_name(run.passed[run.passed_count - 1].stage) : GROW_STAGE_COUNT;
        stuck[stage]++;
    }
    api_grow_run_destroy(&run);
    return composed;
}

int main(int argc, char **argv) {
    bool fast = argc > 1 && strcmp(argv[1], "fast") == 0;
    int failures = 0;
    if (!fast) {
        printf("%-62s %s\n", "case", "composed of 10 (stuck stages of the failures)");
    }
    for (int development = 0; development < GROW_DEVELOP_COUNT; development++) {
    for (size_t r = 0; r < sizeof(REFERENCES) / sizeof(REFERENCES[0]); r++) {
        Motive motive = load_motive(REFERENCES[r].file);
        for (size_t b = 0; b < 3; b++) {
            for (size_t e = 0; e < 3; e++) {
                for (size_t bars = 4; bars <= 8; bars += 4) {
                    GrowBriefText text;
                    memset(&text, 0, sizeof(text));
                    text.key = REFERENCES[r].key;
                    text.meter = REFERENCES[r].meter;
                    text.cadence = "PAC";
                    GrowSetup setup;
                    memset(&setup, 0, sizeof(setup));
                    setup.text = &text;
                    setup.into = b == 1 ? GROW_INTO_PERIOD : GROW_INTO_HYBRID;
                    setup.pairing.beginning = (PlanThemeBeginning)b;
                    setup.pairing.ending = (PlanThemeEnding)e;
                    setup.key = motive_key(&motive);
                    setup.meter = motive_meter(&motive);
                    setup.bars = bars;
                    setup.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
                    setup.first_cadence = CADENCE_TYPE_HALF;
                    setup.motive = &motive;
                    setup.explain = true;
                    setup.suggest = 1;
                    setup.development = (GrowDevelopment)development;
                    char name[112];
                    snprintf(name, sizeof(name), "%s %s+%s %zu bars (%s)", REFERENCES[r].name, BEGINNINGS[b], ENDINGS[e], bars,
                             api_development_name(setup.development));
                    /* Not buildable: the pairing cannot be built, or the plan refuses the motif before any seeding. */
                    GrowRequest request;
                    GrowRefusal refusal;
                    PiecePlan *plan = NULL;
                    memset(&refusal, 0, sizeof(refusal));
                    api_grow_request_for(&setup, 1, &request);
                    bool buildable = grow_theme_buildable(&setup.meter, bars, &setup.pairing, setup.cadence, setup.first_cadence) &&
                                     grow_build_plan_checked(&request, &plan, &refusal) == GROW_OK;
                    plan_destroy(plan);
                    if (!buildable) {
                        if (!fast) {
                            printf("%-62s not buildable%s%s\n", name, refusal.message[0] ? ": " : "", refusal.message);
                        }
                        continue;
                    }
                    int stuck[GROW_STAGE_COUNT + 1];
                    memset(stuck, 0, sizeof(stuck));
                    int composed = 0;
                    unsigned starts = fast ? 1 : START_SEEDS;
                    for (unsigned long seed = 1; seed <= starts; seed++) {
                        composed += compose_from(&setup, seed, stuck) ? 1 : 0;
                    }
                    if (!fast) {
                        printf("%-62s %2d of %d", name, composed, START_SEEDS);
                        for (int stage = 0; stage <= GROW_STAGE_COUNT; stage++) {
                            if (stuck[stage] > 0) {
                                printf("  %s x%d", stage == GROW_STAGE_COUNT ? "refused" : grow_stage_name((GrowStage)stage),
                                       stuck[stage]);
                            }
                        }
                        printf("\n");
                    }
                    bool good = fast ? composed == 1 : composed >= MIN_COMPOSED;
                    if (!good) {
                        fprintf(stderr, "%s: composed %d of %u\n", name, composed, starts);
                        failures++;
                    }
                }
            }
        }
    }
    }
    if (failures > 0) {
        fprintf(stderr, "%d cases below the rate\n", failures);
        return 1;
    }
    return 0;
}
