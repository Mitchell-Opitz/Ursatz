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
 * The theme quality sweep (P67, gate 2 of R-543): the four harmony fixtures (s77-chords, u51-chords, two-bar-3-4 and the D minor
 * arpeggio) as 8-bar sentences and periods, development balanced, seeds 1 to 10 through api_grow_run with all of `functions`
 * (harmony, surface, contrast) and melody.cadence_long on. At least 8 of 10 seeds compose in each case (R-317); every composed theme
 * has a predominant between the last tonic before its final dominant and that dominant, and its last melody note starts on the
 * theme's last downbeat and ends at its end; every period's two contrasting ideas are at least 1/2 unlike the basic idea in rhythm and
 * contour (the response's contrasts); every 4/4 sentence has two chords in each fragment bar. "fast" runs seed 1 of every case
 * (test_theme_functions_sweep_smoke); no argument runs seeds 1 to 10 (slow).
 */

#define SEEDS 10
#define FIXTURES 4

static bool fast_run = false;
static Motive MOTIVE_OF[FIXTURES];
static const char *const FILES[FIXTURES] = {"/continuation/s77-chords.json", "/continuation/u51-chords.json", "/rates/two-bar-3-4.json",
                                            "/motifs/motive-d-minor-arpeggio.json"};

static unsigned long last_seed(void) {
    return fast_run ? 1 : SEEDS;
}

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

/* The theme of the fixture and seed with every setting on (false when none is made). */
static bool theme_of(size_t which, bool period, unsigned long seed, GrowBriefText *text, GrowSetup *setup, GrowRun *run) {
    static const GrowFunctions ALL = {.harmony = true, .surface = true, .contrast = true};
    static MelodyFillParams melody;
    melody = melody_fill_params_default();
    melody.cadence_long = true;
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

/* A predominant stands between the last tonic before the final dominant and that dominant. */
static bool predominant_before_final_dominant(const cJSON *theme) {
    const char *names[64];
    size_t count = 0;
    const cJSON *chord;
    cJSON_ArrayForEach(chord, cJSON_GetObjectItemCaseSensitive(theme, "chords")) {
        if (count < 64) {
            names[count++] = cJSON_GetObjectItemCaseSensitive(chord, "function")->valuestring;
        }
    }
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

/* The last melody note of the node starts on its last downbeat and ends at its end. */
static bool last_note_held(const Plan *plan, NodeId node, Rational bar) {
    PlanSpan span = plan_node_span(plan, node);
    LaneSpanId ids[16];
    size_t found = plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, 16);
    Rational onset = {0, 1};
    Rational end = {0, 1};
    bool any = false;
    for (size_t i = 0; i < found && i < 16; i++) {
        Rational start = plan_lane_span_span(plan, ids[i]).start;
        const PlanNotes *notes = &plan_lane_span_payload(plan, ids[i])->notes;
        for (size_t n = 0; n < notes->note_count; n++) {
            Rational at;
            assert(rational_add(start, notes->notes[n].offset, &at) == RATIONAL_OK);
            if (!any || rational_cmp(at, onset) > 0) {
                onset = at;
                assert(rational_add(at, notes->notes[n].duration, &end) == RATIONAL_OK);
                any = true;
            }
        }
    }
    Rational node_end;
    Rational downbeat;
    assert(rational_add(span.start, span.length, &node_end) == RATIONAL_OK && rational_sub(node_end, bar, &downbeat) == RATIONAL_OK);
    return any && rational_cmp(onset, downbeat) == 0 && rational_cmp(end, node_end) == 0;
}

/* The harmony spans that start inside the node. */
static size_t chords_in(const Plan *plan, NodeId node) {
    PlanSpan span = plan_node_span(plan, node);
    LaneSpanId ids[16];
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, 16);
    size_t count = 0;
    for (size_t i = 0; i < found && i < 16; i++) {
        count += rational_cmp(plan_lane_span_span(plan, ids[i]).start, span.start) >= 0 ? 1 : 0;
    }
    return count;
}

static void check_theme(bool period, const GrowSetup *setup, GrowRun *run) {
    const Plan *plan = run->themes[0].plan;
    Rational bar;
    assert(plan_meter_bar_length(&setup->meter, &bar) == PLAN_OK);
    cJSON *document = api_grow_result_json(setup, run, NULL);
    assert(document);
    const cJSON *theme = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0);
    assert(predominant_before_final_dominant(theme));
    assert(last_note_held(plan, test_find_node(plan, "theme"), bar));
    if (period) {
        assert(last_note_held(plan, test_find_node(plan, "contrasting idea"), bar));
        const cJSON *contrasts = cJSON_GetObjectItemCaseSensitive(theme, "contrasts");
        assert(cJSON_IsArray(contrasts) && cJSON_GetArraySize(contrasts) == 2);
        const cJSON *row;
        cJSON_ArrayForEach(row, contrasts) {
            assert(cJSON_GetObjectItemCaseSensitive(row, "rhythm")->valuedouble >= 0.5);
            assert(cJSON_GetObjectItemCaseSensitive(row, "contour")->valuedouble >= 0.5);
        }
    } else if (setup->meter.beats_per_bar == 4 && setup->meter.beat_unit == 4) {
        for (int fragment = 1; fragment <= 2; fragment++) {
            char name[16];
            snprintf(name, sizeof(name), "fragment %d", fragment);
            assert(chords_in(plan, test_find_node(plan, name)) == 2);
        }
    }
    cJSON_Delete(document);
}

int main(int argc, char **argv) {
    fast_run = argc > 1 && strcmp(argv[1], "fast") == 0;
    for (size_t which = 0; which < FIXTURES; which++) {
        char path[512];
        snprintf(path, sizeof(path), "%s%s", COMPOSER_FIXTURE_DIR, FILES[which]);
        MOTIVE_OF[which] = load_motive(path);
    }
    for (size_t which = 0; which < FIXTURES; which++) {
        for (int period = 0; period < 2; period++) {
            size_t composed = 0;
            for (unsigned long seed = 1; seed <= last_seed(); seed++) {
                GrowBriefText text;
                GrowSetup setup;
                GrowRun run;
                if (!theme_of(which, period, seed, &text, &setup, &run)) {
                    continue;
                }
                composed++;
                check_theme(period, &setup, &run);
                api_grow_run_destroy(&run);
            }
            printf("test_theme_functions_sweep: %s %zu: %zu of %lu composed\n", period ? "period" : "sentence", which, composed, last_seed());
            fflush(stdout);
            assert(composed * 10 >= 8 * last_seed() || fast_run);
        }
    }
    return 0;
}
