#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>
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
 * The critic choosing among theme candidates (R-722). With critic.rank off a run is what it was: the response of api_grow_run with the
 * critic settings equals the one without them and has no `critic` and no `rejected`. With rank on (candidates 4) a run collects four
 * themes for the one it keeps: the kept theme has the critic's judgment and a score at least every rejected one's, `rejected` holds
 * at most keep_rejected (2 when set to 2, through the API), the kept theme's seed is among the seeds tried and the first seed that composes with rank
 * off is among the candidates. The response of a theme request with rank on is written for the schema test (api_theme_*_rank.json).
 */

static Motive motive;

static Motive load_motive(const char *path) {
    char *text = test_must_read_file(path, NULL);
    Motive loaded;
    MotiveError error;
    memset(&error, 0, sizeof(error));
    assert(motive_from_json(text, test_motive_options(), &loaded, &error) == MOTIVE_OK);
    free(text);
    return loaded;
}

static bool theme_of(bool period, unsigned long seed, const CriticSettings *critic, GrowBriefText *text, GrowSetup *setup, GrowRun *run) {
    memset(text, 0, sizeof(*text));
    text->key = "C major";
    text->meter = "4/4";
    text->cadence = "PAC";
    text->into = period ? "period" : "sentence";
    memset(setup, 0, sizeof(*setup));
    setup->knowledge = test_knowledge();
    setup->text = text;
    setup->into = period ? GROW_INTO_PERIOD : GROW_INTO_SENTENCE;
    setup->pairing.beginning = period ? SECTION_BEGINNING_ANTECEDENT : SECTION_BEGINNING_PRESENTATION;
    setup->pairing.ending = period ? SECTION_ENDING_CONSEQUENT : SECTION_ENDING_CONTINUATION;
    setup->key = motive_key(&motive);
    setup->meter = motive_meter(&motive);
    setup->bars = 8;
    setup->cadence = CADENCE_TYPE_PERFECT_AUTHENTIC;
    setup->first_cadence = CADENCE_TYPE_HALF;
    setup->motive = &motive;
    setup->critic = critic;
    setup->suggest = 1;
    setup->seed = seed;
    setup->tempo_from = "none";
    UrsatzError error;
    memset(&error, 0, sizeof(error));
    if (api_grow_run(setup, run, &error) != URSATZ_OK || run->made < 1) {
        api_grow_run_destroy(run);
        return false;
    }
    return true;
}

static double ratio(const cJSON *object) {
    return cJSON_GetObjectItemCaseSensitive(object, "numerator")->valuedouble / cJSON_GetObjectItemCaseSensitive(object, "denominator")->valuedouble;
}

static void test_rank_off(void) {
    CriticSettings off;
    critic_settings_default(&off);
    off.candidates = 8;
    for (unsigned long seed = 1; seed <= 1; seed++) {
        GrowBriefText text;
        GrowSetup plain;
        GrowRun run;
        if (!theme_of(true, seed, NULL, &text, &plain, &run)) {
            continue;
        }
        char *without = cJSON_PrintUnformatted(api_grow_result_json(&plain, &run, NULL));
        api_grow_run_destroy(&run);
        GrowBriefText text_off;
        GrowSetup setup;
        assert(theme_of(true, seed, &off, &text_off, &setup, &run));
        cJSON *document = api_grow_result_json(&setup, &run, NULL);
        char *with = cJSON_PrintUnformatted(document);
        assert(without && with && strcmp(without, with) == 0);
        assert(cJSON_GetObjectItemCaseSensitive(document, "rejected") == NULL);
        assert(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0), "critic") == NULL);
        assert(!run.ranked && run.rejected_count == 0 && !run.themes[0].judged);
        cJSON_Delete(document);
        cJSON_free(with);
        cJSON_free(without);
        api_grow_run_destroy(&run);
    }
}

static void test_rank_on(void) {
    CriticSettings on;
    critic_settings_default(&on);
    on.rank = true;
    on.candidates = 4;
    size_t ranked = 0;
    for (unsigned long seed = 1; seed <= 1; seed++) {
        GrowBriefText text;
        GrowSetup plain;
        GrowRun run;
        if (!theme_of(true, seed, NULL, &text, &plain, &run)) {
            continue;
        }
        unsigned long first_off = run.themes[0].seed;
        api_grow_run_destroy(&run);
        GrowBriefText text_on;
        GrowSetup setup;
        assert(theme_of(true, seed, &on, &text_on, &setup, &run));
        ranked++;
        assert(run.ranked && run.made == 1 && run.themes[0].judged);
        cJSON *document = api_grow_result_json(&setup, &run, NULL);
        const cJSON *kept = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0);
        const cJSON *judgment = cJSON_GetObjectItemCaseSensitive(kept, "critic");
        assert(cJSON_IsObject(judgment) && cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(judgment, "evaluators")) == 8);
        const cJSON *rejected = cJSON_GetObjectItemCaseSensitive(document, "rejected");
        assert(cJSON_IsArray(rejected));
        double best = ratio(cJSON_GetObjectItemCaseSensitive(judgment, "score"));
        unsigned long kept_seed = (unsigned long)cJSON_GetObjectItemCaseSensitive(kept, "seed")->valuedouble;
        bool first_among = kept_seed == first_off;
        /* the candidates are drawn from the seeds seed, seed + 1, ...: the kept one and the rejected are among the first 16 x 4 */
        assert(kept_seed >= seed && kept_seed < seed + 64);
        const cJSON *candidate;
        cJSON_ArrayForEach(candidate, rejected) {
            unsigned long rejected_seed = (unsigned long)cJSON_GetObjectItemCaseSensitive(candidate, "seed")->valuedouble;
            assert(best >= ratio(cJSON_GetObjectItemCaseSensitive(candidate, "score")));
            assert(rejected_seed >= seed && rejected_seed < seed + 64 && rejected_seed != kept_seed);
            assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(candidate, "evaluators")) == 8);
            first_among = first_among || rejected_seed == first_off;
        }
        assert(cJSON_GetArraySize(rejected) == 3 && first_among);
        /* the response is the same as the run's: judged and rejected in memory and in the document */
        assert(run.rejected_count == 3);
        cJSON_Delete(document);
        api_grow_run_destroy(&run);
    }
    assert(ranked == 1);
}

/* A theme request with critic.rank on through the API: the judgment and the rejected list are in the response; written for the schema test. */
static void test_api_response(void) {
    char *text = test_must_read_file(COMPOSER_FIXTURE_DIR "/theme/period-contrast.json", NULL);
    cJSON *request = cJSON_Parse(text);
    free(text);
    assert(request && cJSON_AddNumberToObject(request, "seed", 1));
    cJSON *preferences = cJSON_Parse("{\"critic\":{\"rank\":true,\"candidates\":3,\"keep_rejected\":2}}");
    assert(preferences && cJSON_AddItemToObject(request, "preferences", preferences));
    char *printed = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    ComposerResponse response;
    assert(composer_api_theme(test_context(), printed, NULL, &response) == URSATZ_OK);
    FILE *file = fopen("api_theme_request_rank.json", "wb");
    assert(file);
    fputs(printed, file);
    fclose(file);
    file = fopen("api_theme_response_rank.json", "wb");
    assert(file);
    fputs(response.json, file);
    fclose(file);
    free(printed);
    cJSON *document = cJSON_Parse(response.json);
    composer_response_destroy(&response);
    assert(document);
    const cJSON *theme = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "themes"), 0);
    assert(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(theme, "critic")));
    assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(document, "rejected")) == 2); /* keep_rejected 2 of the two not kept */
    assert(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "preferences"), "critic") != NULL);
    cJSON_Delete(document);
}

int main(void) {
    motive = load_motive(COMPOSER_FIXTURE_DIR "/continuation/s77-chords.json");
    test_rank_off();
    test_rank_on();
    test_api_response();
    printf("test_critic_rank: ok\n");
    return 0;
}
