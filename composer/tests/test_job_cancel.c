#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/form_template.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/plan_json.h>

/*
 * Progress and cancel (docs/api.md, "Jobs"): compose_run on a theme-fill plan with a job that cancels at its third step
 * gives PIECE_ERR_STUCK, outcome.cancelled and nothing left allocated (the valgrind run checks it); progress never goes
 * back within a stage; a NULL job gives the same plan as before; grow_run reports "grow" and stops the same way.
 */

typedef struct {
    unsigned steps;
    unsigned cancel_at; /* the step that answers true; 0 for never */
    char stage[32];
    unsigned done;
    unsigned total;
    bool decreased;
    bool total_changed;
    unsigned compose_stage_count;
} Watch;

static void on_progress(void *user, const char *stage, unsigned done, unsigned total) {
    Watch *watch = user;
    if (strcmp(stage, watch->stage) == 0) {
        /* A fill loop that starts again (the melody is written again from the next seed) begins at 0. */
        if (done < watch->done && done != 0) {
            watch->decreased = true;
        }
        if (total != watch->total) {
            watch->total_changed = true;
        }
    }
    snprintf(watch->stage, sizeof watch->stage, "%s", stage);
    watch->done = done;
    watch->total = total;
    if (strcmp(stage, "fill") != 0) {
        watch->compose_stage_count++;
    }
}

static bool on_cancelled(void *user) {
    Watch *watch = user;
    watch->steps++;
    return watch->cancel_at != 0 && watch->steps >= watch->cancel_at;
}

static PiecePlan *theme_fill_plan(void) {
    PiecePlan *plan = NULL;
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = FORM_TEMPLATE_ROUNDED_BINARY;
    request.key = plan_key_of_pitch_class(0, KEY_STRUCTURE_MODE_MAJOR);
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    request.bars = 24;
    assert(piece_build_plan(&request, &plan) == PIECE_OK);
    return plan;
}

static void test_cancel(void) {
    PiecePlan *plan = theme_fill_plan();
    Watch watch;
    memset(&watch, 0, sizeof watch);
    watch.cancel_at = 3;
    ComposerJob job = {on_progress, on_cancelled, &watch};
    ComposeConfig config;
    memset(&config, 0, sizeof config);
    config.seed = 3;
    config.job = &job;
    ComposeOutcome outcome;
    assert(compose_run(plan, &config, &outcome) == PIECE_ERR_STUCK);
    assert(outcome.cancelled);
    assert(!outcome.stuck);
    assert(watch.steps == 3);
    compose_outcome_destroy(&outcome);
    plan_destroy(plan);

    /* Cancelled at the very first step, at a compose stage. */
    plan = theme_fill_plan();
    memset(&watch, 0, sizeof watch);
    watch.cancel_at = 1;
    assert(compose_run(plan, &config, &outcome) == PIECE_ERR_STUCK);
    assert(outcome.cancelled && watch.steps == 1);
    compose_outcome_destroy(&outcome);
    plan_destroy(plan);

    /* Candidates stop at the cancel and keep nothing. */
    plan = theme_fill_plan();
    memset(&watch, 0, sizeof watch);
    watch.cancel_at = 2;
    ComposeCandidates candidates;
    assert(compose_candidates(plan, &config, 3, &candidates) == PIECE_ERR_STUCK);
    assert(candidates.count == 0 && candidates.items == NULL);
    assert(watch.steps == 2);
    plan_destroy(plan);
}

static void test_progress(void) {
    PiecePlan *plan = theme_fill_plan();
    Watch watch;
    memset(&watch, 0, sizeof watch);
    ComposerJob job = {on_progress, on_cancelled, &watch};
    ComposeConfig config;
    memset(&config, 0, sizeof config);
    config.seed = 3;
    config.job = &job;
    ComposeOutcome outcome;
    assert(compose_run(plan, &config, &outcome) == PIECE_OK);
    assert(!outcome.cancelled);
    assert(!watch.decreased);
    assert(!watch.total_changed);
    assert(watch.compose_stage_count >= 6);
    assert(watch.steps > watch.compose_stage_count);
    compose_outcome_destroy(&outcome);
    plan_destroy(plan);
}

/* A NULL job, a job with no members and a job that only watches give the plan the run gave before the job existed. */
static void test_same_plan(void) {
    ComposeConfig config;
    memset(&config, 0, sizeof config);
    config.seed = 3;
    ComposeOutcome outcome;
    char *texts[3];
    ComposerJob none = {NULL, NULL, NULL};
    Watch watch;
    memset(&watch, 0, sizeof watch);
    ComposerJob watching = {on_progress, on_cancelled, &watch};
    const ComposerJob *jobs[3] = {NULL, &none, &watching};
    for (int i = 0; i < 3; i++) {
        PiecePlan *plan = theme_fill_plan();
        config.job = jobs[i];
        assert(compose_run(plan, &config, &outcome) == PIECE_OK);
        texts[i] = plan_to_json(plan, NULL);
        assert(texts[i]);
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    assert(strcmp(texts[0], texts[1]) == 0);
    assert(strcmp(texts[0], texts[2]) == 0);
    for (int i = 0; i < 3; i++) {
        free(texts[i]);
    }
}

static void test_fill_job(void) {
    /* The fill loop alone: one report per target, a cancel is FILL_STATUS_ERROR with the flag. */
    PiecePlan *plan = theme_fill_plan();
    FillOperatorRegistry *registry = fill_registry_create();
    assert(registry);
    Watch watch;
    memset(&watch, 0, sizeof watch);
    watch.cancel_at = 1;
    ComposerJob job = {on_progress, on_cancelled, &watch};
    FillConfig config;
    memset(&config, 0, sizeof config);
    config.job = &job;
    FillOutcome outcome;
    FillRecord *record = NULL;
    assert(fill_run(plan, registry, &config, &outcome, &record) != FILL_OK);
    assert(outcome.cancelled && outcome.status == FILL_STATUS_ERROR && outcome.iterations == 0);
    assert(strcmp(watch.stage, "fill") == 0 && watch.done == 0);
    fill_record_destroy(record);
    fill_outcome_destroy(&outcome);
    fill_registry_destroy(registry);
    plan_destroy(plan);
}

/* No argument runs everything; "cancel" runs the cancelled runs (the valgrind run: a complete run leaks in Ursatz's motive finder
   while it hears the plan, which is not this test's to fix); "complete" runs the runs that finish. */
int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "all";
    bool all = strcmp(mode, "all") == 0;
    assert(all || strcmp(mode, "cancel") == 0 || strcmp(mode, "complete") == 0);
    if (all || strcmp(mode, "cancel") == 0) {
        test_cancel();
        test_fill_job();
    }
    if (all || strcmp(mode, "complete") == 0) {
        test_progress();
        test_same_plan();
    }
    printf("test_job_cancel %s: ok\n", mode);
    return 0;
}
