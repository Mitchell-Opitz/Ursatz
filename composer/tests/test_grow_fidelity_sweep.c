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
#include "util/plan_util.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Fidelity over the sweep motifs (B1 and B2 of P37, R-354, R-355): the four motifs as an 8-bar sentence and an 8-bar
 * period in their own key and meter, PAC, seeds 1 to 10, for each development setting. Prints, for each setting and role,
 * the lowest score, the share of ideas heard as intended, and the share the recognizer found a chain for. Asserts that every
 * buildable case composes (at least MIN_COMPOSED of the seeds, R-317), that every composed idea is at or above its role's
 * minimum for the setting, and that presentation and continuation ideas are heard as intended at least as often as the baselines below.
 * Slow: CI runs ctest -LE slow.
 */

#define SEEDS 10
#define MIN_COMPOSED 8
#define ROLE_COUNT 5
/*
 * The share of presentation and continuation ideas heard as intended, as measured over this sweep (R-355's goal is 90): the
 * melody rules bend a note that clashes with its chord (R-328), the recognizer then hears interval changes where the plan
 * asked for an exact or a transposed idea, and every kept candidate of a target bends alike, so ranking has little to choose
 * from. Presentation measured 20 to 26% and continuation 75%; the baselines are a little under. Raised only upward.
 */
#define AGREE_PERCENT_PRESENTATION 15
#define AGREE_PERCENT_CONTINUATION 70

typedef struct {
    const char *name;
    const char *file;
    const char *key;
    const char *meter;
} Motif;

static const Motif MOTIFS[] = {
    {"test-motif-6-8", COMPOSER_FIXTURE_DIR "/motifs/test-motif-6-8.json", "C major", "6/8"},
    {"motive-d-minor-arpeggio", COMPOSER_FIXTURE_DIR "/motifs/motive-d-minor-arpeggio.json", "D minor", "4/4"},
    {"one-bar-4-4", COMPOSER_FIXTURE_DIR "/rates/one-bar-4-4.json", "C major", "4/4"},
    {"two-bar-3-4", COMPOSER_FIXTURE_DIR "/rates/two-bar-3-4.json", "A minor", "3/4"}};

static const char *const ROLE_NAMES[ROLE_COUNT] = {"none", "presentation", "continuation", "cadential", "contrast"};

typedef struct {
    size_t ideas;
    size_t agree;
    size_t found;
    size_t under; /* below the role's minimum */
    bool has_lowest;
    MotiveScore lowest;
} RoleStats;

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

/* Whether a is below b. */
static bool score_below(MotiveScore a, MotiveScore b) {
    return (unsigned long long)a.numerator * b.denominator < (unsigned long long)b.numerator * a.denominator;
}

static void add_idea(RoleStats *stats, const MotiveFidelity *fidelity, MotiveScore minimum) {
    stats->ideas++;
    stats->under += score_below(fidelity->score, minimum) ? 1 : 0;
    stats->agree += fidelity->agrees ? 1 : 0;
    stats->found += fidelity->found ? 1 : 0;
    if (!stats->has_lowest || score_below(fidelity->score, stats->lowest)) {
        stats->lowest = fidelity->score;
        stats->has_lowest = true;
    }
}

/* Reads every idea with a role of a composed theme into the setting's stats. */
static void read_theme(const PiecePlan *plan, const Motive *basic, GrowDevelopment development, RoleStats stats[ROLE_COUNT]) {
    for (NodeId idea = 1; idea <= plan_node_count(plan); idea++) {
        LaneSpanId span;
        GrowIdeaRole role =
            plan_node_kind(plan, idea) == PLAN_NODE_IDEA ? grow_idea_role(plan_node_name(plan, idea)) : GROW_ROLE_NONE;
        MotiveFidelity fidelity;
        if (role == GROW_ROLE_NONE || !plan_melody_span_of(plan, idea, &span)) {
            continue;
        }
        memset(&fidelity, 0, sizeof(fidelity));
        Motive source;
        PlanMotiveRef ref;
        bool from_segment = false;
        assert(grow_idea_fidelity_source(plan, span, basic, &source, &ref, &from_segment));
        if (!grow_idea_fidelity(plan, span, basic, &fidelity)) {
            fprintf(stderr, "idea \"%s\" has no source or no realized notes\n", plan_node_name(plan, idea));
        }
        assert(grow_idea_fidelity(plan, span, basic, &fidelity));
        /* An idea whose ref cuts the motive down is ranked but never refused (R-355). */
        add_idea(&stats[role], &fidelity,
                 grow_ref_departs(&ref, from_segment) ? (MotiveScore){0, 1} : grow_fidelity_minimum(development, role));
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    int failures = 0;
    for (int development = 0; development < GROW_DEVELOP_COUNT; development++) {
        RoleStats stats[ROLE_COUNT];
        memset(stats, 0, sizeof(stats));
        for (size_t m = 0; m < sizeof(MOTIFS) / sizeof(MOTIFS[0]); m++) {
            Motive motive = load_motive(MOTIFS[m].file);
            for (int period = 0; period <= 1; period++) {
                GrowBriefText text;
                memset(&text, 0, sizeof(text));
                text.key = MOTIFS[m].key;
                text.meter = MOTIFS[m].meter;
                text.cadence = "PAC";
                GrowSetup setup;
                memset(&setup, 0, sizeof(setup));
                setup.text = &text;
                setup.into = period ? GROW_INTO_PERIOD : GROW_INTO_HYBRID;
                setup.pairing.beginning = period ? PLAN_THEME_BEGINNING_ANTECEDENT : PLAN_THEME_BEGINNING_PRESENTATION;
                setup.pairing.ending = period ? PLAN_THEME_ENDING_CONSEQUENT : PLAN_THEME_ENDING_CONTINUATION;
                setup.key = motive_key(&motive);
                setup.meter = motive_meter(&motive);
                setup.bars = 8;
                setup.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
                setup.first_cadence = CADENCE_TYPE_HALF;
                setup.motive = &motive;
                setup.suggest = 1;
                setup.development = (GrowDevelopment)development;
                char name[96];
                snprintf(name, sizeof(name), "%s %s (%s)", MOTIFS[m].name, period ? "period" : "sentence",
                         api_development_name(setup.development));
                /* Not buildable: the pairing cannot be built, or the plan refuses the motif before any seeding. */
                GrowRequest request;
                GrowRefusal refusal;
                PiecePlan *built = NULL;
                memset(&refusal, 0, sizeof(refusal));
                api_grow_request_for(&setup, 1, &request);
                bool buildable = grow_theme_buildable(&setup.meter, setup.bars, &setup.pairing, setup.cadence, setup.first_cadence) &&
                                 grow_build_plan_checked(&request, &built, &refusal) == GROW_OK;
                plan_destroy(built);
                if (!buildable) {
                    printf("%-56s not buildable%s%s\n", name, refusal.message[0] ? ": " : "", refusal.message);
                    continue;
                }
                int composed = 0;
                for (unsigned long seed = 1; seed <= SEEDS; seed++) {
                    GrowRun run;
                    ComposerError error;
                    memset(&error, 0, sizeof(error));
                    setup.seed = seed;
                    if (api_grow_run(&setup, &run, &error) == COMPOSER_OK && run.made == 1) {
                        composed++;
                        read_theme(run.themes[0].plan, &motive, (GrowDevelopment)development, stats);
                    }
                    api_grow_run_destroy(&run);
                }
                printf("%-56s composed %2d of %d\n", name, composed, SEEDS);
                if (composed < MIN_COMPOSED) {
                    fprintf(stderr, "%s: composed %d of %d\n", name, composed, SEEDS);
                    failures++;
                }
            }
        }
        printf("\ndevelopment %s\n%-14s %6s %14s %8s %8s\n", api_development_name((GrowDevelopment)development), "role", "ideas",
               "lowest score", "agrees", "found");
        for (int role = GROW_ROLE_PRESENTATION; role < ROLE_COUNT; role++) {
            const RoleStats *s = &stats[role];
            if (s->ideas == 0) {
                printf("%-14s %6s\n", ROLE_NAMES[role], "none");
                continue;
            }
            printf("%-14s %6zu %9lu/%-4lu %6zu%% %7zu%%\n", ROLE_NAMES[role], s->ideas, s->lowest.numerator, s->lowest.denominator,
                   s->agree * 100 / s->ideas, s->found * 100 / s->ideas);
            if (s->under > 0) {
                fprintf(stderr, "%s, %s: %zu of %zu ideas under the minimum\n", api_development_name((GrowDevelopment)development),
                        ROLE_NAMES[role], s->under, s->ideas);
                failures++;
            }
            int baseline = role == GROW_ROLE_PRESENTATION ? AGREE_PERCENT_PRESENTATION
                           : role == GROW_ROLE_CONTINUATION ? AGREE_PERCENT_CONTINUATION
                                                            : 0;
            if (s->agree * 100 < s->ideas * (size_t)baseline) {
                fprintf(stderr, "%s, %s: %zu of %zu ideas heard as intended, under %d%%\n",
                        api_development_name((GrowDevelopment)development), ROLE_NAMES[role], s->agree, s->ideas, baseline);
                failures++;
            }
        }
        printf("\n");
    }
    if (failures > 0) {
        fprintf(stderr, "%d failures: cases composed on fewer than %d of %d seeds, roles under their minimum or under their heard-as-intended baseline\n", failures, MIN_COMPOSED, SEEDS);
        return 1;
    }
    return 0;
}
