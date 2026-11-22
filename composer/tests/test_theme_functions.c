#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <plan/plan_json.h>
#include <plan/plan_state.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/motive.h>

#include "api/api_grow.h"
#include "grow/grow_internal.h"
#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

#ifndef COMPOSER_FIXTURE_DIR
#error "COMPOSER_FIXTURE_DIR is not defined"
#endif

/*
 * Themes whose ideas follow their formal function (P67, R-710 to R-712). B6: harmony by function. With functions.harmony, sentences and
 * periods over s77-chords (C major), u51-chords (C minor) and the 3/4 motif of rates/two-bar-3-4 (A minor), seeds 1 to 8: every composed
 * theme has a predominant between the last tonic before its final dominant and that dominant (the response's chords), in 4/4 each
 * fragment bar has two chords, the fragments' chords are a `free_motive` target, and at least 6 of 8 seeds compose in each case (measured:
 * 8 of 8, 2026-11-22); every active progression type has a harmony shape. B5: the contrast target. Periods over s77-chords (C major) and
 * u51-chords (C minor), seeds 1 to 8 through api_grow_run: with functions.contrast every composed theme's contrasting ideas carry the
 * `contrast` constraint and the response's `contrasts` read rhythm and contour distances of at least 1/2; without it the plan equals the
 * plan with `functions` absent and the response has no `contrasts`; a plan with the constraint writes and reads back the same.
 */

#define SEEDS 8
#define MOTIVES 2 /* the fixtures of the contrast cases: s77-chords and u51-chords */
#define FIXTURES 3 /* and the 3/4 motif of the harmony cases */

/* "fast" runs seed 1 of every case (test_theme_functions_smoke); no argument runs seeds 1 to 8 (slow). */
static bool fast_run = false;

static unsigned long last_seed(void) {
    return fast_run ? 1 : SEEDS;
}

static Motive MOTIVE_OF[FIXTURES];
static const char *KEY_OF[FIXTURES] = {"C major", "C minor", "A minor"};
static const char *METER_OF[FIXTURES] = {"4/4", "4/4", "3/4"};

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

/* The 8-bar period of the motive and seed through api_grow_run with the functions settings (NULL for absent); false when no theme is made. */
static bool theme_of(size_t which, bool period, unsigned long seed, const GrowFunctions *functions, GrowBriefText *text,
                     GrowSetup *setup, GrowRun *run) {
    memset(text, 0, sizeof(*text));
    text->key = KEY_OF[which];
    text->meter = METER_OF[which];
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
    setup->motive = &MOTIVE_OF[which];
    setup->functions = functions;
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

static char *plan_text(const Plan *plan) {
    PlanJsonError error;
    char *text = plan_to_json(plan, test_plan_producer(), &error);
    assert(text);
    return text;
}

static double number_of(const cJSON *row, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(row, name);
    assert(cJSON_IsNumber(item));
    return item->valuedouble;
}

/* With contrast on: both contrasting ideas hold the constraint, and the measured rhythm and contour distances reach 1/2. */
static void test_contrast_target(void) {
    GrowFunctions on = {.contrast = true};
    size_t composed = 0;
    for (size_t which = 0; which < MOTIVES; which++) {
        for (unsigned long seed = 1; seed <= last_seed(); seed++) {
            GrowBriefText text;
            GrowSetup setup;
            GrowRun run;
            if (!theme_of(which, true, seed, &on, &text, &setup, &run)) {
                continue;
            }
            composed++;
            const Plan *plan = run.themes[0].plan;
            static const char *const IDEAS[2] = {"contrasting idea", "consequent contrasting idea"};
            static const char *const AGAINST[2] = {"basic idea", "consequent basic idea"};
            for (size_t i = 0; i < 2; i++) {
                NodeId node = test_find_node(plan, IDEAS[i]);
                assert(node != PLAN_NODE_NONE);
                const PlanConstraint *constraint = plan_node_find_constraint(plan, node, PLAN_CONSTRAINT_CONTRAST);
                assert(constraint && strcmp(constraint->params.contrast.against, AGAINST[i]) == 0);
                assert(constraint->params.contrast.aspect_count == 2);
            }
            cJSON *document = api_grow_result_json(&setup, &run, NULL);
            assert(document);
            const cJSON *theme = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0);
            const cJSON *contrasts = cJSON_GetObjectItemCaseSensitive(theme, "contrasts");
            assert(cJSON_IsArray(contrasts) && cJSON_GetArraySize(contrasts) == 2);
            for (int i = 0; i < 2; i++) {
                const cJSON *row = cJSON_GetArrayItem(contrasts, i);
                assert(strcmp(cJSON_GetObjectItemCaseSensitive(row, "idea")->valuestring, IDEAS[i]) == 0);
                assert(number_of(row, "rhythm") >= 0.5 && number_of(row, "contour") >= 0.5);
                assert(number_of(row, "register") >= 0 && number_of(row, "density") >= 0);
            }
            cJSON_Delete(document);

            /* The plan with the constraint writes and reads back the same. */
            PlanJsonError error;
            Plan *parsed = NULL;
            char *written = plan_text(plan);
            assert(plan_from_json(test_knowledge(), written, composer_plan_read_hooks(), &parsed, &error) == PLAN_OK);
            char *again = plan_text(parsed);
            assert(strcmp(written, again) == 0 && strstr(written, "\"kind\": \"contrast\"") != NULL);
            free(written);
            free(again);
            plan_destroy(parsed);
            api_grow_run_destroy(&run);
        }
    }
    assert(composed >= MOTIVES * last_seed() / 2);
    printf("test_theme_functions: contrast target, %zu of %lu periods composed\n", composed, MOTIVES * last_seed());
}

/* With every setting off the plan is the plan with `functions` absent, and the response has no `contrasts`. */
static void test_off_changes_nothing(void) {
    GrowFunctions off = {0};
    for (size_t which = 0; which < MOTIVES; which++) {
        for (int period = 0; period < 2; period++) {
            for (unsigned long seed = 1; seed <= last_seed(); seed++) {
                GrowBriefText text_a;
                GrowBriefText text_b;
                GrowSetup setup_a;
                GrowSetup setup_b;
                GrowRun run_a;
                GrowRun run_b;
                bool a = theme_of(which, period, seed, NULL, &text_a, &setup_a, &run_a);
                bool b = theme_of(which, period, seed, &off, &text_b, &setup_b, &run_b);
                assert(a == b);
                if (!a) {
                    continue;
                }
                char *first = plan_text(run_a.themes[0].plan);
                char *second = plan_text(run_b.themes[0].plan);
                assert(strcmp(first, second) == 0 && strstr(first, "\"kind\": \"contrast\"") == NULL);
                free(first);
                free(second);
                cJSON *document = api_grow_result_json(&setup_b, &run_b, NULL);
                assert(document);
                const cJSON *theme = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0);
                assert(cJSON_GetObjectItemCaseSensitive(theme, "contrasts") == NULL);
                cJSON_Delete(document);
                api_grow_run_destroy(&run_a);
                api_grow_run_destroy(&run_b);
            }
        }
    }
}

/* --- harmony by function (B6) -------------------------------------------------------------------------------- */

/* The chords of the theme as the response gives them: the function of each, in time order. */
static size_t chord_functions(const cJSON *theme, const char *names[64]) {
    const cJSON *chords = cJSON_GetObjectItemCaseSensitive(theme, "chords");
    size_t count = 0;
    const cJSON *chord;
    cJSON_ArrayForEach(chord, chords) {
        if (count < 64) {
            names[count++] = cJSON_GetObjectItemCaseSensitive(chord, "function")->valuestring;
        }
    }
    return count;
}

/* A predominant stands between the last tonic before the final dominant and that dominant. */
static bool predominant_before_final_dominant(const char *const *names, size_t count) {
    size_t dominant = count;
    for (size_t i = count; i-- > 0 && dominant == count;) {
        if (strcmp(names[i], "dominant") == 0) {
            dominant = i;
        }
    }
    for (size_t i = dominant; dominant < count && i-- > 0 && strcmp(names[i], "tonic") != 0;) {
        if (strcmp(names[i], "predominant") == 0) {
            return true;
        }
    }
    return false;
}

/* The harmony spans that start inside the node, and whether each holds a harmony shape with free_motive. */
static size_t chords_in(const Plan *plan, NodeId node, size_t *free_count) {
    PlanSpan span = plan_node_span(plan, node);
    LaneSpanId ids[16];
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, 16);
    size_t count = 0;
    for (size_t i = 0; i < found && i < 16; i++) {
        if (rational_cmp(plan_lane_span_span(plan, ids[i]).start, span.start) < 0) {
            continue;
        }
        count++;
        const PlanConstraint *shape = plan_lane_span_find_constraint(plan, ids[i], PLAN_CONSTRAINT_HARMONY_SHAPE);
        if (shape && shape->params.harmony_shape.free_motive && free_count) {
            (*free_count)++;
        }
    }
    return count;
}

static void test_harmony_by_function(void) {
    GrowFunctions on = {.harmony = true};
    for (size_t which = 0; which < FIXTURES; which++) {
        for (int period = 0; period < 2; period++) {
            size_t composed = 0;
            for (unsigned long seed = 1; seed <= last_seed(); seed++) {
                GrowBriefText text;
                GrowSetup setup;
                GrowRun run;
                if (!theme_of(which, period, seed, &on, &text, &setup, &run)) {
                    continue;
                }
                composed++;
                const Plan *plan = run.themes[0].plan;
                cJSON *document = api_grow_result_json(&setup, &run, NULL);
                assert(document);
                const cJSON *theme = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0);
                const char *names[64];
                size_t count = chord_functions(theme, names);
                assert(predominant_before_final_dominant(names, count));
                for (int fragment = 1; fragment <= 2; fragment++) {
                    char name[16];
                    snprintf(name, sizeof(name), "fragment %d", fragment);
                    NodeId node = test_find_node(plan, name);
                    if (node == PLAN_NODE_NONE) {
                        continue;
                    }
                    size_t freed = 0;
                    size_t chords = chords_in(plan, node, &freed);
                    assert(freed >= 1 && (which == 2 || chords == 2));
                }
                cJSON_Delete(document);
                api_grow_run_destroy(&run);
            }
            printf("test_theme_functions: harmony, %s %s: %zu of %lu composed\n", KEY_OF[which], period ? "period" : "sentence", composed,
                   last_seed());
            assert(composed >= (fast_run ? 1u : 6u));
        }
    }
    /* Every active progression type has a shape. */
    const KnowledgeSnapshot *snapshot = test_knowledge();
    size_t active = 0;
    for (size_t i = 0; i < knowledge_snapshot_entry_count(snapshot); i++) {
        const VocabularyEntry *entry = knowledge_snapshot_vocabulary(snapshot, knowledge_snapshot_entry_at(snapshot, i)->id);
        PlanHarmonyShapeKind kind;
        if (entry && entry->subtype == VOCABULARY_PROGRESSION_TYPE &&
            knowledge_snapshot_entry_at(snapshot, i)->status == KNOWLEDGE_STATUS_ACTIVE) {
            assert(grow_progression_shape(entry->code, &kind));
            active++;
        }
    }
    assert(active == 2);
}

int main(int argc, char **argv) {
    fast_run = argc > 1 && strcmp(argv[1], "fast") == 0;
    MOTIVE_OF[0] = load_motive(COMPOSER_FIXTURE_DIR "/continuation/s77-chords.json");
    MOTIVE_OF[1] = load_motive(COMPOSER_FIXTURE_DIR "/continuation/u51-chords.json");
    MOTIVE_OF[2] = load_motive(COMPOSER_FIXTURE_DIR "/rates/two-bar-3-4.json");
    test_harmony_by_function();
    test_contrast_target();
    test_off_changes_nothing();
    return 0;
}
