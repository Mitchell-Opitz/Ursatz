#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/fill_rng.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/melody_fill.h>
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
 * Ranking and refusing ideas by fidelity (B2 of P37, R-355): the Test Motif as an 8-bar sentence and the two-bar 3/4 motif as an
 * 8-bar period, seeds 1 to 3, balanced. Every idea with a role is at or above its minimum; for every motive-backed target
 * the melody record chose the best candidate that was not rejected; and a recorded kind of candidate checked with the
 * minimum raised to 1 is refused with the fidelity reason.
 */

#define SEEDS 3
#define MELODY_OPERATOR "melody-fill"

typedef struct {
    const char *name;
    const char *selector; /* the argument that runs only this case */
    const char *file;
    const char *key;
    const char *meter;
    PlanThemeBeginning beginning;
    PlanThemeEnding ending;
} Case;

static const Case CASES[] = {
    {"test-motif sentence", "sentence", COMPOSER_FIXTURE_DIR "/motifs/test-motif-6-8.json", "C major", "6/8", PLAN_THEME_BEGINNING_PRESENTATION,
     PLAN_THEME_ENDING_CONTINUATION},
    {"two-bar-3-4 period", "period", COMPOSER_FIXTURE_DIR "/rates/two-bar-3-4.json", "A minor", "3/4", PLAN_THEME_BEGINNING_ANTECEDENT,
     PLAN_THEME_ENDING_CONSEQUENT}};

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

/* Whether a/b is below c/d. */
static bool below(unsigned long a, unsigned long b, unsigned long c, unsigned long d) {
    return (unsigned long long)a * d < (unsigned long long)c * b;
}

/* Every idea with a role is at or above its minimum, its fidelity read from the written plan (grow_idea_fidelity's score). Returns how many were checked. */
static size_t check_minimums(const PiecePlan *plan, const Motive *basic, GrowDevelopment development, const char *label) {
    size_t checked = 0;
    for (NodeId idea = 1; idea <= plan_node_count(plan); idea++) {
        LaneSpanId span;
        GrowIdeaRole role =
            plan_node_kind(plan, idea) == PLAN_NODE_IDEA ? grow_idea_role(plan_node_name(plan, idea)) : GROW_ROLE_NONE;
        if (role == GROW_ROLE_NONE || !plan_melody_span_of(plan, idea, &span)) {
            continue;
        }
        /* The score only: the heard chain is searched to two steps, half a second a miss, and decides nothing here (R-307). */
        Motive source;
        Motive realized;
        PlanMotiveRef ref;
        bool from_segment = false;
        MotiveRecognizability recognizability;
        assert(grow_idea_fidelity_source(plan, span, basic, &source, &ref, &from_segment));
        assert(grow_idea_realized(plan, span, &realized));
        assert(motive_recognizability(&source, &realized, NULL, &recognizability));
        MotiveFidelity fidelity;
        memset(&fidelity, 0, sizeof(fidelity));
        fidelity.score = recognizability.total;
        MotiveScore minimum = grow_fidelity_minimum(development, role);
        if (below(fidelity.score.numerator, fidelity.score.denominator, minimum.numerator, minimum.denominator)) {
            fprintf(stderr, "%s: idea \"%s\" has fidelity %lu/%lu under its minimum %lu/%lu\n", label, plan_node_name(plan, idea),
                    fidelity.score.numerator, fidelity.score.denominator, minimum.numerator, minimum.denominator);
        }
        assert(!below(fidelity.score.numerator, fidelity.score.denominator, minimum.numerator, minimum.denominator));
        checked++;
    }
    return checked;
}

/* Every melody-fill step that chose a candidate chose one no better candidate that was not rejected beats. Returns the steps read. */
static size_t check_choices(const PiecePlan *plan, const FillRecord *record, const char *label) {
    size_t read = 0;
    assert(record);
    for (size_t s = 0; s < record->step_count; s++) {
        const FillStep *step = &record->steps[s];
        if (step->outcome != FILL_STEP_APPLIED || step->chosen < 0 || (size_t)step->chosen >= step->considered_count ||
            step->target.kind != FILL_TARGET_LANE_SPAN || !plan_has_lane_span(plan, (LaneSpanId)step->target.id) ||
            !plan_lane_span_find_constraint(plan, (LaneSpanId)step->target.id, PLAN_CONSTRAINT_MOTIVE_REF)) {
            continue;
        }
        const FillConsidered *chosen = &step->considered[step->chosen];
        if (strcmp(chosen->operator_name, MELODY_OPERATOR) != 0) {
            continue;
        }
        assert(!chosen->rejected);
        for (size_t c = 0; c < step->considered_count; c++) {
            const FillConsidered *other = &step->considered[c];
            if (other->rejected || strcmp(other->operator_name, MELODY_OPERATOR) != 0) {
                continue;
            }
            if (below(chosen->score_numerator, chosen->score_denominator, other->score_numerator, other->score_denominator)) {
                fprintf(stderr, "%s: target %zu chose %lu/%lu over %lu/%lu\n", label, step->target.id, chosen->score_numerator,
                        chosen->score_denominator, other->score_numerator, other->score_denominator);
            }
            assert(!below(chosen->score_numerator, chosen->score_denominator, other->score_numerator, other->score_denominator));
        }
        read++;
    }
    return read;
}

/*
 * The idea's melody span put back to open, its candidates made again, each checked with the table's minimum and with the
 * minimum raised to 1: the raised one refuses what is not exact, with the fidelity reason, and a candidate that stands is
 * scored as R-355 says. Returns how many candidates the raised minimum refused.
 */
static size_t check_refusal(PiecePlan *plan, const Motive *basic, LaneSpanId span, const char *label) {
    assert(plan_lane_span_set_state(plan, span, PLAN_STATE_CONSTRAINED) == PLAN_OK);
    FillOperatorRegistry *registry = fill_registry_create();
    assert(registry && melody_fill_register(registry, NULL) == FILL_OK);
    const FillOperator *operator_ = fill_registry_at(registry, 0);
    FillTarget target = {FILL_TARGET_LANE_SPAN, (size_t)span};
    FillRng rng;
    FillCandidateList list;
    memset(&list, 0, sizeof(list));
    fill_rng_seed(&rng, 1);
    assert(operator_->applies(plan, target, operator_->context));
    assert(operator_->generate(plan, target, &rng, operator_->context, &list));
    if (list.count == 0) {
        fprintf(stderr, "%s: no candidate for the idea\n", label);
    }
    assert(list.count > 0);

    MotiveScore one = {1, 1};
    GrowFidelityContext table = {GROW_DEVELOP_BALANCED, basic, melody_fill_checker, NULL, NULL, NULL};
    GrowFidelityContext raised = table;
    raised.minimum = &one;
    size_t refused = 0;
    for (size_t c = 0; c < list.count; c++) {
        FillCheck plain, ranked, strict;
        memset(&plain, 0, sizeof(plain));
        memset(&ranked, 0, sizeof(ranked));
        memset(&strict, 0, sizeof(strict));
        assert(melody_fill_checker(plan, target, operator_, &list.items[c], NULL, &plain));
        assert(grow_fidelity_checker(plan, target, operator_, &list.items[c], &table, &ranked));
        assert(grow_fidelity_checker(plan, target, operator_, &list.items[c], &raised, &strict));
        assert(!plain.hard_violation); /* the generator proposes only what the melody rules pass */
        if (!ranked.hard_violation) {
            /* Agreement, then fidelity, then the melody rules' share: over 201 (total + 1). */
            assert(ranked.score_denominator == 201ul * (plain.score_denominator + 1));
            assert(ranked.score_numerator < ranked.score_denominator);
        } else {
            assert(strncmp(ranked.reason, "motive fidelity ", 16) == 0);
        }
        if (strict.hard_violation) {
            assert(strncmp(strict.reason, "motive fidelity ", 16) == 0);
            assert(strstr(strict.reason, "under 1/1") != NULL);
            refused++;
        }
    }
    fill_candidates_clear(&list);
    fill_registry_destroy(registry);
    return refused;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    size_t checked = 0;
    size_t chosen_steps = 0;
    size_t refused = 0;
    size_t run_cases = 0;
    for (size_t c = 0; c < sizeof(CASES) / sizeof(CASES[0]); c++) {
        if (argc > 1 && strcmp(argv[1], CASES[c].selector) != 0) {
            continue;
        }
        run_cases++;
        Motive motive = load_motive(CASES[c].file);
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            GrowBriefText text;
            memset(&text, 0, sizeof(text));
            text.key = CASES[c].key;
            text.meter = CASES[c].meter;
            text.cadence = "PAC";
            GrowSetup setup;
            memset(&setup, 0, sizeof(setup));
            setup.text = &text;
            setup.into = CASES[c].beginning == PLAN_THEME_BEGINNING_ANTECEDENT ? GROW_INTO_PERIOD : GROW_INTO_HYBRID;
            setup.pairing.beginning = CASES[c].beginning;
            setup.pairing.ending = CASES[c].ending;
            setup.key = motive_key(&motive);
            setup.meter = motive_meter(&motive);
            setup.bars = 8;
            setup.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
            setup.first_cadence = CADENCE_TYPE_HALF;
            setup.motive = &motive;
            setup.suggest = 1;
            setup.development = GROW_DEVELOP_BALANCED;
            setup.seed = seed;
            GrowRun run;
            ComposerError error;
            char label[64];
            memset(&error, 0, sizeof(error));
            snprintf(label, sizeof(label), "%s seed %lu", CASES[c].name, seed);
            assert(api_grow_run(&setup, &run, &error) == COMPOSER_OK && run.made == 1);
            PiecePlan *plan = run.themes[0].plan;
            checked += check_minimums(plan, &motive, GROW_DEVELOP_BALANCED, label);
            chosen_steps += check_choices(plan, run.themes[0].outcome.records[GROW_STAGE_MELODY], label);

            /* The refusal on the basic idea (always there) of the first seed; the plan is reopened, so it is read last. */
            if (seed == 1) {
                for (NodeId idea = 1; idea <= plan_node_count(plan); idea++) {
                    LaneSpanId span;
                    if (plan_node_kind(plan, idea) == PLAN_NODE_IDEA && strcmp(plan_node_name(plan, idea), "basic idea") == 0 &&
                        plan_melody_span_of(plan, idea, &span)) {
                        refused += check_refusal(plan, &motive, span, label);
                        break;
                    }
                }
            }
            api_grow_run_destroy(&run);
        }
    }
    printf("ideas at or above their minimum: %zu; melody choices read: %zu; refused at a minimum of 1: %zu\n", checked,
           chosen_steps, refused);
    assert(run_cases > 0 && checked >= 4 * SEEDS * run_cases);
    assert(chosen_steps >= 4 * SEEDS * run_cases);
    assert(refused > 0 || (argc > 1 && strcmp(argv[1], "sentence") != 0 && strcmp(argv[1], "period") != 0));
    return 0;
}
