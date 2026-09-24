#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/constraint.h>
#include <ursatz_composer/form_template.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/return_hear.h>

/*
 * Hearing the returns of a composed piece (return_hear.h): rounded binary, ternary and variations (3), C major 4/4 with
 * an invented motive, seeds 1 to 10. Each section that returns another is planned to resemble it; it is heard as
 * planned when the kernel's thematic return detection finds that pair.
 *
 *   smoke   rounded binary 24 bars, seed 1: the return is heard, and a piece with no return has none heard (quick)
 *   heard   the sweep: for every completed run, return_hear finds the pairs the run reports; the share of planned
 *           returns heard is printed with the share of sections a theme is heard in, and is not below its baseline
 *
 * The share is below 100% where no theme is heard in a section: the kernel's detection compares the exact contour and
 * rhythm of the themes standard-form finds, and it finds none in a period (docs/piece.md).
 */

#define SEEDS 10

typedef struct {
    const char *name;
    FormTemplateKind kind;
    size_t bars;
    size_t count;
    size_t baseline; /* returns heard, of those planned over the completed runs */
} Scenario;

static PieceRequest request_of(const Scenario *scenario) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = scenario->kind;
    request.bars = scenario->bars;
    request.count = scenario->count;
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    return request;
}

/* The section a section returns, else PLAN_NODE_NONE. */
static NodeId returned(const PiecePlan *plan, NodeId section) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, section, i);
        if (constraint->kind == PLAN_CONSTRAINT_RETURN) {
            return constraint->params.section_return.section;
        }
    }
    return PLAN_NODE_NONE;
}

/* Runs one seed; the planned and the heard returns of a completed run are added, and true is returned. */
static bool run(const Scenario *scenario, unsigned long seed, size_t *planned, size_t *heard) {
    PieceRequest request = request_of(scenario);
    PiecePlan *plan = NULL;
    ComposeOutcome outcome;
    ComposeConfig config;
    assert(piece_build_plan(&request, &plan) == PIECE_OK);
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    bool completed = compose_run(plan, &config, &outcome) == PIECE_OK;
    if (completed) {
        ReturnHeard direct;
        size_t found = 0;
        assert(return_hear(plan, &direct) == RETURN_HEAR_OK);
        NodeId form = plan_root(plan);
        for (size_t i = 0; i + 1 < plan_node_child_count(plan, form); i++) {
            NodeId section = plan_node_child_at(plan, form, i);
            NodeId source = returned(plan, section);
            if (source != PLAN_NODE_NONE) {
                const ReturnHeardPair *pair = return_heard_find(&direct, source, section);
                *planned += 1;
                found += pair != NULL;
                assert(!pair || (pair->similarity >= 0.8 && pair->similarity <= 1.0 && pair->earlier == source &&
                                 pair->later == section));
            }
        }
        /* The outcome reports what the call finds (it hears only a piece that plans a return), and every pair is a later
           section resembling an earlier one. */
        assert((outcome.returns_planned == 0) == (scenario->kind == FORM_TEMPLATE_BINARY));
        assert(outcome.returns_heard == found && (outcome.returns_planned == 0 || outcome.returns.count == direct.count));
        assert(outcome.returns_heard_as_planned == (outcome.returns_heard == outcome.returns_planned));
        for (size_t i = 0; i < direct.count; i++) {
            assert(direct.pairs[i].earlier < direct.pairs[i].later);
        }
        *heard += found;
        return_heard_destroy(&direct);
    }
    compose_outcome_destroy(&outcome);
    plan_destroy(plan);
    return completed;
}

static int test_smoke(void) {
    Scenario rounded = {"rounded binary 24 bars", FORM_TEMPLATE_ROUNDED_BINARY, 24, 0, 0};
    Scenario binary = {"binary 16 bars", FORM_TEMPLATE_BINARY, 16, 0, 0};
    size_t planned = 0;
    size_t heard = 0;
    assert(run(&rounded, 1, &planned, &heard));
    assert(planned == 1 && heard == 1);
    planned = 0;
    heard = 0;
    assert(run(&binary, 1, &planned, &heard));
    assert(planned == 0 && heard == 0);
    printf("rounded binary: the return of A is heard; binary has none to hear\n");
    return 0;
}

static int test_heard(void) {
    static const Scenario SCENARIOS[] = {
        {"rounded binary 24 bars", FORM_TEMPLATE_ROUNDED_BINARY, 24, 0, 10},
        {"ternary 24 bars", FORM_TEMPLATE_TERNARY, 24, 0, 0},
        {"variations (3)", FORM_TEMPLATE_VARIATIONS, 0, 3, 3},
    };
    int status = 0;
    size_t all_planned = 0;
    size_t all_heard = 0;
    for (size_t s = 0; s < sizeof(SCENARIOS) / sizeof(SCENARIOS[0]); s++) {
        size_t planned = 0;
        size_t heard = 0;
        size_t completed = 0;
        for (unsigned long seed = 1; seed <= SEEDS; seed++) {
            completed += run(&SCENARIOS[s], seed, &planned, &heard);
        }
        printf("%s: %zu of %zu runs complete, %zu of %zu returns heard (%.0f%%; baseline %zu)\n", SCENARIOS[s].name,
               completed, (size_t)SEEDS, heard, planned, planned ? 100.0 * (double)heard / (double)planned : 0.0,
               SCENARIOS[s].baseline);
        assert(completed > 0);
        all_planned += planned;
        all_heard += heard;
        status |= heard < SCENARIOS[s].baseline;
    }
    printf("all: %zu of %zu returns heard (%.0f%%)\n", all_heard, all_planned,
           all_planned ? 100.0 * (double)all_heard / (double)all_planned : 0.0);
    return status;
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "";
    if (strcmp(mode, "smoke") == 0) {
        return test_smoke();
    }
    if (strcmp(mode, "heard") == 0) {
        return test_heard();
    }
    fprintf(stderr, "usage: test_return_heard smoke|heard\n");
    return 2;
}
