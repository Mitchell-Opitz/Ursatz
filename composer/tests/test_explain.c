#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include <ursatz_composer/compose.h>
#include <ursatz_composer/explain.h>
#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/style.h>
#include "support/test_support.h"
#include "ursatz_test.h"
#include "support/test_knowledge.h"

#ifndef COMPOSER_CLI_PATH
#error "COMPOSER_CLI_PATH is not defined"
#endif
#ifndef COMPOSER_STYLE_DIR
#error "COMPOSER_STYLE_DIR is not defined"
#endif

#define CHORALES COMPOSER_STYLE_DIR "/chorales.style.json"

/*
 * The explanation of a run (explain.h, docs/explain.md).
 *
 *   choices      rounded binary, seed 3, with the check and the chorales style: every target of the explanation is a
 *                step of the stage's record, its chosen candidate is the record's chosen entry, its runners-up are the
 *                best of the others by score, its rejected candidates are the record's, with the checker's reasons;
 *                the style term is in the score; each section carries its analyzer check
 *   rejections   a period in G major 6/8 composed with the check: a run the analysis contradicted is in
 *                ComposeOutcome.rejections and in the explanation, with the items that disagreed
 *   same         the same seed gives the same JSON and the same text
 *   cli          fill and grow --explain write JSON for a name ending in .json and text otherwise; a fill record without a
 *                style is byte for byte what main wrote (a hash of it was taken on main); with a style it carries the term
 */

/*
 * The fill record of rounded binary, seed 3, 24 bars in C major 4/4, as main writes it with `fill --seed 3 --record`:
 * FNV-1a 64, taken after the motive-forms (P14 B8) changed the ideas' motives, and again after the shaping preferences
 * (P16 B4) changed the candidates chosen, and again when the numerals were written in the library grammar (P25 B3), and again when the
 * presentation drew its repetition by weight (P34 B1), and again when the continuation took segments (P34 B2).
 */
#define MAIN_RECORD_HASH 0x77b59ca4f5d1ac76ULL /* re-recorded at P37 B2 (ideas ranked by fidelity, R-355); P36 B3 before */

static Plan *rounded_binary(void) {
    Plan *plan = NULL;
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.form = "rounded-binary";
    request.key = test_key(0, "major");
    request.knowledge = test_knowledge();
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    request.bars = 24;
    assert(piece_build_plan(&request, &plan) == PIECE_OK);
    return plan;
}

/* A period of 8 bars in G major 6/8 grown from a motive, a plan the Analyzer disagrees with in 7 of 10 seeds. It was A minor 3/4 until
   P61 B6 (the Composer hears on harmonic spans, R-618): the Analyzer then agreed with every seed of it (and with 300 seeds). */
static Plan *period_g_major(Motive *motive) {
    Plan *plan = NULL;
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.knowledge = test_knowledge();
    options.has_meter = true;
    options.meter.beats_per_bar = 6;
    options.meter.beat_unit = 8;
    assert(motive_from_notes_text("B4:q A4:e G4:e F#4:e D4:e", &options, motive, NULL) == MOTIVE_OK);
    GrowRequest request;
    memset(&request, 0, sizeof(request));
    request.motive = motive;
    request.key = test_key(7, "major");
    request.meter = options.meter;
    request.bars = 8;
    request.pairing.beginning = SECTION_BEGINNING_ANTECEDENT;
    request.pairing.ending = SECTION_ENDING_CONSEQUENT;
    request.cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    assert(grow_build_plan_checked(&request, &plan, NULL) == GROW_OK);
    return plan;
}

static ComposerStyle *chorales(void) {
    ComposerStyle *style = NULL;
    assert(composer_style_load_without(CHORALES, NULL, 0, &style) == COMPOSER_STYLE_OK);
    return style;
}

static ComposeStage stage_named(const char *name) {
    for (int stage = 0; stage < COMPOSE_STAGE_COUNT; stage++) {
        if (strcmp(compose_stage_name((ComposeStage)stage), name) == 0) {
            return (ComposeStage)stage;
        }
    }
    assert(!"a stage name");
    return COMPOSE_STAGE_COUNT;
}

static bool same_candidate(const ExplainCandidate *candidate, const FillConsidered *considered) {
    return strcmp(candidate->operator_name, considered->operator_name) == 0 &&
           strcmp(candidate->label, considered->label) == 0 && candidate->score_numerator == considered->score_numerator &&
           candidate->score_denominator == considered->score_denominator && candidate->styled == considered->styled &&
           candidate->style_numerator == considered->style_numerator &&
           candidate->style_denominator == considered->style_denominator && candidate->rejected == considered->rejected &&
           strcmp(candidate->reason, considered->reason) == 0;
}

__extension__ typedef unsigned __int128 Wide;

static bool scores_higher(unsigned long an, unsigned long ad, unsigned long bn, unsigned long bd) {
    return (Wide)an * bd > (Wide)bn * ad;
}

/* ------------------------------------------------------------------ choices */

static int test_choices(void) {
    Plan *plan = rounded_binary();
    ComposerStyle *style = chorales();
    ComposeConfig config;
    memset(&config, 0, sizeof(config));
    config.knowledge = test_knowledge();
    config.seed = 3;
    config.analyzer_check = true;
    config.style = style;
    ComposeOutcome outcome;
    assert(compose_run(plan, &config, &outcome) == PIECE_OK);
    Explanation explanation;
    assert(explain_build(plan, &outcome, &explanation) == EXPLAIN_OK);

    /* Every step that weighed candidates is in the explanation once, in the order of the record. */
    size_t expected = 0;
    for (int stage = 0; stage < COMPOSE_STAGE_COUNT; stage++) {
        const FillRecord *record = outcome.records[stage];
        for (size_t s = 0; record && s < record->step_count; s++) {
            expected += record->steps[s].outcome != FILL_STEP_NO_OPERATOR;
        }
    }
    assert(expected > 0 && explanation.target_count == expected);

    size_t styled_choices = 0;
    size_t runners_seen = 0;
    size_t rejected_seen = 0;
    size_t last_step[COMPOSE_STAGE_COUNT];
    bool seen_stage[COMPOSE_STAGE_COUNT] = {false};
    for (size_t t = 0; t < explanation.target_count; t++) {
        const ExplainTarget *target = &explanation.targets[t];
        ComposeStage stage = stage_named(target->stage);
        const FillRecord *record = outcome.records[stage];
        assert(record && target->step < record->step_count);
        assert(!seen_stage[stage] || target->step > last_step[stage]);
        seen_stage[stage] = true;
        last_step[stage] = target->step;
        const FillStep *step = &record->steps[target->step];
        assert(step->outcome != FILL_STEP_NO_OPERATOR);
        assert(target->kind == step->target.kind && target->id == step->target.id);
        assert(target->outcome == step->outcome && target->considered == step->considered_count);
        /* Motive material (m0 and the motives made from it) lies after the piece's last bar. */
        assert(target->first_bar >= 1 && target->last_bar >= target->first_bar);

        /* The chosen candidate is the record's chosen entry. */
        assert(target->has_chosen == (step->chosen >= 0));
        if (target->has_chosen) {
            assert(same_candidate(&target->chosen, &step->considered[step->chosen]));
            assert(!target->chosen.rejected);
            if (target->chosen.styled) {
                styled_choices++;
                /* The style term is part of the score. */
                assert(!scores_higher(target->chosen.style_numerator, target->chosen.style_denominator,
                                      target->chosen.score_numerator, target->chosen.score_denominator));
            }
        }

        /* The runners-up are the best of the others, by score, and none is rejected or chosen. */
        size_t eligible = 0;
        for (size_t i = 0; i < step->considered_count; i++) {
            eligible += !step->considered[i].rejected && (long)i != step->chosen;
        }
        assert(target->runner_up_count == (eligible < EXPLAIN_RUNNERS_UP ? eligible : EXPLAIN_RUNNERS_UP));
        for (size_t r = 0; r < target->runner_up_count; r++) {
            const ExplainCandidate *runner = &target->runners_up[r];
            bool found = false;
            size_t better_outside = 0;
            for (size_t i = 0; i < step->considered_count; i++) {
                const FillConsidered *entry = &step->considered[i];
                found = found || (!entry->rejected && (long)i != step->chosen && same_candidate(runner, entry));
            }
            assert(found);
            /* No candidate left out of the runners-up scores above this one. */
            for (size_t i = 0; i < step->considered_count; i++) {
                const FillConsidered *entry = &step->considered[i];
                bool listed = false;
                for (size_t other = 0; other < target->runner_up_count; other++) {
                    listed = listed || same_candidate(&target->runners_up[other], entry);
                }
                if (!entry->rejected && (long)i != step->chosen && !listed &&
                    scores_higher(entry->score_numerator, entry->score_denominator, runner->score_numerator,
                                  runner->score_denominator)) {
                    better_outside++;
                }
            }
            assert(better_outside == 0);
            if (r > 0) {
                const ExplainCandidate *before = &target->runners_up[r - 1];
                assert(!scores_higher(runner->score_numerator, runner->score_denominator, before->score_numerator,
                                      before->score_denominator));
            }
            if (target->has_chosen) {
                /* Behind the chosen one by exactly the difference of the scores. */
                Wide left = (Wide)target->chosen.score_numerator * runner->score_denominator;
                Wide right = (Wide)runner->score_numerator * target->chosen.score_denominator;
                Wide gap = left > right ? left - right : 0;
                assert(gap * runner->behind_denominator ==
                       (Wide)runner->behind_numerator * target->chosen.score_denominator * runner->score_denominator);
            }
            runners_seen++;
        }

        /* The rejected candidates are the record's, in order, with the checkers' reasons. */
        size_t next = 0;
        for (size_t i = 0; i < step->considered_count; i++) {
            if (step->considered[i].rejected) {
                assert(next < target->rejected_count && same_candidate(&target->rejected[next], &step->considered[i]));
                assert(target->rejected[next].reason[0] != '\0');
                next++;
            }
        }
        assert(next == target->rejected_count);
        rejected_seen += next;
    }
    assert(styled_choices > 0 && runners_seen > 0 && rejected_seen > 0);

    /* Each section has its analyzer check and what it was heard as. */
    assert(explanation.section_count == outcome.section_count && explanation.section_count > 0);
    for (size_t s = 0; s < explanation.section_count; s++) {
        const ExplainSection *section = &explanation.sections[s];
        assert(section->checked && section->check.planned > 0 && section->check.item_count == outcome.analyzer_checks[s].item_count);
        assert(section->check.agreed == outcome.analyzer_checks[s].agreed);
        assert(section->heard && section->heard_form[0] != '\0');
    }
    assert(explanation.rejection_count == outcome.rejection_count);

    explain_destroy(&explanation);
    compose_outcome_destroy(&outcome);
    composer_style_destroy(style);
    plan_destroy(plan);
    printf("every choice in the explanation matches the fill records\n");
    return 0;
}

/* --------------------------------------------------------------- rejections */

/* The section of the plan a rejection names: the first child of the form. */
static NodeId first_section(const Plan *plan) {
    return plan_node_child_at(plan, plan_root(plan), 0);
}

/*
 * explain_analyzer_rejection, explain_build_grow and compose_rejections_free on their own: one item of three disagreeing is the one
 * rejection item, naming the section; all three agreeing is a rejection with no item; a grow with one extra rejection explains one
 * section (the theme) and that rejection.
 */
static void test_rejection_helpers(void) {
    Motive motive;
    Plan *plan = period_g_major(&motive);
    NodeId section = first_section(plan);
    const char *name = plan_node_name(plan, section);
    AnalyzerCheckItem items[3];
    memset(items, 0, sizeof(items));
    for (size_t i = 0; i < 3; i++) {
        items[i].kind = ANALYZER_CHECK_CADENCE;
        items[i].bar = i * 4;
        snprintf(items[i].planned, sizeof(items[i].planned), "perfect-authentic");
        snprintf(items[i].heard, sizeof(items[i].heard), "perfect-authentic");
        items[i].agreed = true;
    }
    items[1].agreed = false;
    snprintf(items[1].heard, sizeof(items[1].heard), "half");
    AnalyzerCheck check = {true, 3, 2, items, 3};
    ComposeRejection rejection;
    assert(name && explain_analyzer_rejection(plan, section, &check, 2, 77, &rejection) == EXPLAIN_OK);
    assert(rejection.round == 2 && rejection.seed == 77 && strcmp(rejection.stage, "check") == 0 && strcmp(rejection.reason, "analyzer") == 0);
    assert(rejection.item_count == 1 && strcmp(rejection.items[0].section, name) == 0);
    assert(rejection.items[0].item.bar == 4 && !rejection.items[0].item.agreed && strcmp(rejection.items[0].item.heard, "half") == 0);
    /* Freed with the array that holds it; NULL is safe. */
    ComposeRejection *held = calloc(1, sizeof(*held));
    assert(held);
    *held = rejection;
    compose_rejections_free(held, 1);
    compose_rejections_free(NULL, 0);
    /* All three agreeing: a rejection of the check with nothing to show. */
    items[1].agreed = true;
    assert(explain_analyzer_rejection(plan, section, &check, 0, 5, &rejection) == EXPLAIN_OK);
    assert(rejection.item_count == 0 && rejection.items == NULL && strcmp(rejection.reason, "analyzer") == 0);
    assert(explain_analyzer_rejection(NULL, section, &check, 0, 5, &rejection) == EXPLAIN_ERR_NULL);
    assert(explain_analyzer_rejection(plan, section, NULL, 0, 5, &rejection) == EXPLAIN_ERR_NULL);
    assert(explain_analyzer_rejection(plan, section, &check, 0, 5, NULL) == EXPLAIN_ERR_NULL);

    /* A grow that kept no record, with one rejection of its own to add. */
    GrowOutcome outcome;
    ComposeRejection extra;
    Explanation explanation;
    memset(&outcome, 0, sizeof(outcome));
    memset(&extra, 0, sizeof(extra));
    extra.round = 2;
    extra.seed = 9;
    snprintf(extra.stage, sizeof(extra.stage), "harmony");
    snprintf(extra.reason, sizeof(extra.reason), "stuck");
    assert(explain_build_grow(plan, section, &outcome, NULL, &extra, 1, &explanation) == EXPLAIN_OK);
    assert(explanation.section_count == 1 && explanation.sections[0].section == section && !explanation.sections[0].checked);
    assert(explanation.rejection_count == 1 && explanation.rejections[0].round == 2 && strcmp(explanation.rejections[0].reason, "stuck") == 0);
    explain_destroy(&explanation);
    /* No theme node: no section. */
    assert(explain_build_grow(plan, PLAN_NODE_NONE, &outcome, NULL, NULL, 0, &explanation) == EXPLAIN_OK);
    assert(explanation.section_count == 0 && explanation.rejection_count == 0);
    explain_destroy(&explanation);
    /* NULL arguments, and an extra count with no extra. */
    assert(explain_build_grow(plan, section, NULL, NULL, NULL, 0, &explanation) == EXPLAIN_ERR_NULL);
    assert(explain_build_grow(plan, section, &outcome, NULL, NULL, 1, &explanation) == EXPLAIN_ERR_NULL);
    assert(explain_build_grow(plan, section, &outcome, NULL, NULL, 0, NULL) == EXPLAIN_ERR_NULL);
    plan_destroy(plan);
}

static int test_rejections(void) {
    test_rejection_helpers();
    bool found = false;
    for (unsigned long seed = 1; seed <= 10 && !found; seed++) {
        Motive motive;
        Plan *plan = period_g_major(&motive);
        ComposeConfig config;
        memset(&config, 0, sizeof(config));
        config.knowledge = test_knowledge();
        config.seed = seed;
        config.analyzer_check = true;
        ComposeOutcome outcome;
        PieceResult ran = compose_run(plan, &config, &outcome);
        assert(ran == PIECE_OK || ran == PIECE_ERR_STUCK);
        /* A run that ended stuck threw away every round (twelve of them); one that completed, fewer. */
        assert(ran == PIECE_OK ? outcome.rejection_count < 12 : outcome.rejection_count == 12);
        for (size_t r = 0; r < outcome.rejection_count; r++) {
            const ComposeRejection *rejection = &outcome.rejections[r];
            assert(rejection->round == r && rejection->seed == seed + 100000UL * r);
            assert(rejection->stage[0] != '\0' || strcmp(rejection->reason, "stuck") == 0);
            if (strcmp(rejection->reason, "analyzer") == 0) {
                assert(rejection->item_count > 0);
                for (size_t i = 0; i < rejection->item_count; i++) {
                    assert(!rejection->items[i].item.agreed && rejection->items[i].section[0] != '\0');
                }
                found = true;
            }
        }
        if (found) {
            Explanation explanation;
            assert(explain_build(plan, &outcome, &explanation) == EXPLAIN_OK);
            assert(explanation.rejection_count == outcome.rejection_count);
            for (size_t r = 0; r < outcome.rejection_count; r++) {
                const ComposeRejection *from = &outcome.rejections[r];
                const ComposeRejection *to = &explanation.rejections[r];
                assert(to->round == from->round && to->seed == from->seed && strcmp(to->reason, from->reason) == 0 &&
                       to->item_count == from->item_count);
            }
            char *text = explain_to_text(&explanation);
            char *json = explain_to_json(&explanation);
            assert(text && strstr(text, "rejected runs") && strstr(text, "analyzer"));
            assert(json && strstr(json, "\"reason\": \"analyzer\""));
            free(text);
            free(json);
            explain_destroy(&explanation);
        }
        compose_outcome_destroy(&outcome);
        plan_destroy(plan);
    }
    assert(found);
    printf("a run the analysis contradicted is in the rejections\n");
    return 0;
}

/* --------------------------------------------------------------------- same */

static void explained(unsigned long seed, char **text, char **json) {
    Plan *plan = rounded_binary();
    ComposerStyle *style = chorales();
    ComposeConfig config;
    memset(&config, 0, sizeof(config));
    config.knowledge = test_knowledge();
    config.seed = seed;
    config.analyzer_check = true;
    config.style = style;
    ComposeOutcome outcome;
    assert(compose_run(plan, &config, &outcome) == PIECE_OK);
    Explanation explanation;
    assert(explain_build(plan, &outcome, &explanation) == EXPLAIN_OK);
    *text = explain_to_text(&explanation);
    *json = explain_to_json(&explanation);
    assert(*text && *json);
    explain_destroy(&explanation);
    compose_outcome_destroy(&outcome);
    composer_style_destroy(style);
    plan_destroy(plan);
}

static int test_same(void) {
    char *text_a, *json_a, *text_b, *json_b;
    explained(3, &text_a, &json_a);
    explained(3, &text_b, &json_b);
    assert(strcmp(text_a, text_b) == 0 && strcmp(json_a, json_b) == 0);
    assert(strstr(json_a, "\"schema\": \"explanation\"") && strstr(json_a, "\"version\": 1"));
    assert(strstr(json_a, "\"style\"") && strstr(text_a, "(style "));
    free(text_a);
    free(json_a);
    free(text_b);
    free(json_b);
    printf("the same seed gives the same explanation\n");
    return 0;
}

/* ---------------------------------------------------------------------- cli */

static int run(const char *arguments) {
    char command[4096];
    snprintf(command, sizeof(command), "\"%s\" %s >explain_test_out.txt 2>explain_test_err.txt", COMPOSER_CLI_PATH, arguments);
    int status = system(command);
    assert(WIFEXITED(status));
    return WEXITSTATUS(status);
}

/* The record's hash without its generator_version lines, so a version bump never moves it. */
static uint64_t fnv1a_record(const char *text) {
    uint64_t hash = 1469598103934665603ULL;
    while (*text) {
        const char *end = strchr(text, '\n');
        size_t length = end ? (size_t)(end - text) + 1 : strlen(text);
        bool version = false;
        for (size_t i = 0; i + 19 <= length && !version; i++) {
            version = strncmp(text + i, "\"generator_version\"", 19) == 0;
        }
        if (!version) {
            for (size_t i = 0; i < length; i++) {
                hash ^= (unsigned char)text[i];
                hash *= 1099511628211ULL;
            }
        }
        text += length;
    }
    return hash;
}

static int test_cli(void) {
    char arguments[2048];
    assert(run("plan --form rounded-binary --key C --meter 4/4 --bars 24 --out explain_test_plan.json") == 0);

    /* A record without a style is what main wrote. */
    assert(run("fill explain_test_plan.json --seed 3 --record explain_test_record.json") == 0);
    char *record = test_must_read_file("explain_test_record.json", NULL);
    assert(strstr(record, "\"style\"") == NULL);
    assert(fnv1a_record(record) == MAIN_RECORD_HASH);
    free(record);

    /* --explain: JSON for a name ending in .json, text otherwise; neither changes the record. */
    assert(run("fill explain_test_plan.json --seed 3 --record explain_test_record2.json --explain explain_test.json") == 0);
    assert(run("fill explain_test_plan.json --seed 3 --explain explain_test.txt") == 0);
    char *a = test_must_read_file("explain_test_record.json", NULL);
    char *b = test_must_read_file("explain_test_record2.json", NULL);
    assert(strcmp(a, b) == 0);
    free(a);
    free(b);
    char *json = test_must_read_file("explain_test.json", NULL);
    char *text = test_must_read_file("explain_test.txt", NULL);
    assert(json[0] == '{' && strstr(json, "\"schema\": \"explanation\""));
    assert(strncmp(text, "section ", 8) == 0 && strstr(text, "chosen:") && strstr(text, "[harmony]"));
    assert(strstr(text, "analyzer check: not made"));
    free(json);
    free(text);

    /* With the check and a style, together; the record carries the style term. */
    snprintf(arguments, sizeof(arguments),
             "fill explain_test_plan.json --seed 3 --check --style %s --record explain_test_record3.json --explain explain_test3.json",
             CHORALES);
    assert(run(arguments) == 0);
    record = test_must_read_file("explain_test_record3.json", NULL);
    assert(strstr(record, "\"style\": {"));
    free(record);
    json = test_must_read_file("explain_test3.json", NULL);
    assert(strstr(json, "\"style\": {") && strstr(json, "\"check\": {") && strstr(json, "\"heard\": \"perfect-authentic\""));
    free(json);
    assert(run("fill explain_test_plan.json --seed 3 --check --explain explain_test4.txt") == 0);
    text = test_must_read_file("explain_test4.txt", NULL);
    assert(strstr(text, "planned agreed") && !strstr(text, "analyzer check: not made"));
    free(text);

    /* grow */
    FILE *motive = fopen("explain_test_motive.txt", "wb");
    assert(motive && fputs("E4:q G4:e F4:e D4:q C4:q\n", motive) >= 0);
    fclose(motive);
    assert(run("grow explain_test_motive.txt --into period --key \"C major\" --meter 4/4 --seed 1 --check "
               "--explain explain_test_grow.json") == 0);
    json = test_must_read_file("explain_test_grow.json", NULL);
    assert(strstr(json, "\"schema\": \"explanation\"") && strstr(json, "\"stage\": \"harmony\"") &&
           strstr(json, "\"rejections\": ["));
    free(json);
    snprintf(arguments, sizeof(arguments),
             "grow explain_test_motive.txt --into sentence --key \"C major\" --meter 4/4 --seed 1 --style %s "
             "--explain explain_test_grow.txt",
             CHORALES);
    assert(run(arguments) == 0);
    text = test_must_read_file("explain_test_grow.txt", NULL);
    assert(strncmp(text, "section theme", 13) == 0 && strstr(text, "(style "));
    free(text);
    printf("fill and grow --explain write the explanation\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: test_explain choices|rejections|same|cli\n");
        return 2;
    }
    if (strcmp(argv[1], "choices") == 0) {
        return test_choices();
    }
    if (strcmp(argv[1], "rejections") == 0) {
        return test_rejections();
    }
    if (strcmp(argv[1], "same") == 0) {
        return test_same();
    }
    if (strcmp(argv[1], "cli") == 0) {
        return test_cli();
    }
    fprintf(stderr, "unknown mode '%s'\n", argv[1]);
    return 2;
}
