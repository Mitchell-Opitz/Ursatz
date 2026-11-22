#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <plan/plan_state.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive.h>

#include "api/api_grow.h"
#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * The critic's ranking over the theme quality sweep (P68, R-722): the eight cases of test_theme_functions_sweep (the four harmony
 * fixtures as 8-bar sentences and periods) with every P67 setting (functions harmony, surface and contrast, melody.cadence_long) and
 * critic.rank on (candidates 4), seeds 1 to 5. With rank on at least as many seeds compose as with rank off, and in every run the kept
 * theme's score is at least every rejected one's. "fast" runs seed 1 of every case (test_critic_rank_sweep_smoke);
 * no argument runs seeds 1 to 5 (slow).
 */

#define SEEDS 5
#define FIXTURES 4

static bool fast_run = false;
static Motive MOTIVE_OF[FIXTURES];
static const char *const FILES[FIXTURES] = {"/continuation/s77-chords.json", "/continuation/u51-chords.json", "/rates/two-bar-3-4.json",
                                            "/motifs/motive-d-minor-arpeggio.json"};

static Motive load_motive(const char *path) {
    char *text = test_must_read_file(path, NULL);
    Motive motive;
    MotiveError error;
    memset(&error, 0, sizeof(error));
    MotiveResult result = motive_from_json(text, test_motive_options(), &motive, &error);
    if (result != MOTIVE_OK) {
        fprintf(stderr, "%s: cannot read the motif at %s: %s\n", path, error.location, error.message);
    }
    assert(result == MOTIVE_OK);
    free(text);
    return motive;
}

/* The theme of the fixture and seed with every setting on, ranked by the critic or not; false when none is made. */
static bool theme_of(size_t which, bool period, unsigned long seed, bool rank, GrowBriefText *text, GrowSetup *setup, GrowRun *run) {
    static const GrowFunctions ALL = {.harmony = true, .surface = true, .contrast = true};
    static MelodyFillParams melody;
    static CriticSettings critic;
    melody = melody_fill_params_default();
    melody.cadence_long = true;
    critic_settings_default(&critic);
    critic.rank = rank;
    memset(text, 0, sizeof(*text));
    text->key = "key";
    text->meter = "meter";
    text->cadence = "PAC";
    text->into = period ? "period" : "sentence";
    memset(setup, 0, sizeof(*setup));
    setup->knowledge = test_knowledge();
    setup->text = text;
    setup->into = period ? GROW_INTO_PERIOD : GROW_INTO_SENTENCE;
    setup->pairing.beginning = period ? SECTION_BEGINNING_ANTECEDENT : SECTION_BEGINNING_PRESENTATION;
    setup->pairing.ending = period ? SECTION_ENDING_CONSEQUENT : SECTION_ENDING_CONTINUATION;
    setup->key = motive_key(&MOTIVE_OF[which]);
    setup->meter = motive_meter(&MOTIVE_OF[which]);
    setup->bars = 8;
    setup->cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    setup->first_cadence = CADENCE_TYPE_HALF;
    setup->development = GROW_DEVELOP_BALANCED;
    setup->motive = &MOTIVE_OF[which];
    setup->functions = &ALL;
    setup->melody = &melody;
    setup->critic = &critic;
    setup->suggest = 1;
    setup->seed = seed;
    setup->tempo_from = "none";
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    if (api_grow_run(setup, run, &error) != URSATZ_OK || run->made != 1) {
        api_grow_run_destroy(run);
        return false;
    }
    return true;
}

int main(int argc, char **argv) {
    fast_run = argc > 1 && strcmp(argv[1], "fast") == 0;
    for (size_t which = 0; which < FIXTURES; which++) {
        char path[512];
        snprintf(path, sizeof(path), "%s%s", COMPOSER_FIXTURE_DIR, FILES[which]);
        MOTIVE_OF[which] = load_motive(path);
    }
    unsigned long last_seed = fast_run ? 1 : SEEDS;
    for (size_t which = 0; which < FIXTURES; which++) {
        for (int period = 0; period < 2; period++) {
            size_t off = 0;
            size_t on = 0;
            for (unsigned long seed = 1; seed <= last_seed; seed++) {
                GrowBriefText text;
                GrowSetup setup;
                GrowRun run;
                if (theme_of(which, period, seed, false, &text, &setup, &run)) {
                    off++;
                    api_grow_run_destroy(&run);
                }
                if (!theme_of(which, period, seed, true, &text, &setup, &run)) {
                    continue;
                }
                on++;
                assert(run.ranked && run.themes[0].judged);
                for (size_t i = 0; i < run.rejected_count; i++) {
                    assert(rational_cmp(run.themes[0].judgment.score, run.rejected[i].score) >= 0);
                }
                api_grow_run_destroy(&run);
            }
            printf("test_critic_rank_sweep: %s %zu: %zu of %lu compose with rank off, %zu with rank on\n", period ? "period" : "sentence", which, off,
                   last_seed, on);
            fflush(stdout);
            assert(on >= off);
        }
    }
    return 0;
}
