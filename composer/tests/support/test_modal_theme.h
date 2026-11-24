#ifndef URSATZ_COMPOSER_TESTS_SUPPORT_TEST_MODAL_THEME_H
#define URSATZ_COMPOSER_TESTS_SUPPORT_TEST_MODAL_THEME_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <plan/plan.h>
#include <plan/plan_json.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/piece_plan_keys.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

/*
 * Themes in a mode as `listen` makes them (R-725): the random motive of a seed, analyzed, as the basic idea of a sentence of 8 bars in
 * C, grown from the same seed (or a period: compose_theme_in). Shared by test_modal_compose and test_modal_color; the including test defines COMPOSER_FIXTURE_DIR.
 */

static cJSON *fixture_json(const char *name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", COMPOSER_FIXTURE_DIR, name);
    char *text = test_must_read_file(path, NULL);
    cJSON *json = cJSON_Parse(text);
    free(text);
    assert(json);
    return json;
}

/* The response of one entry call as a document; the test stops for a refusal. */
static inline cJSON *call_json(UrsatzStatus (*entry)(const ComposerContext *, const char *, const UrsatzJob *, ComposerResponse *), const char *request,
                        ComposerResponse *response) {
    memset(response, 0, sizeof(*response));
    UrsatzStatus status = entry(test_context(), request, NULL, response);
    if (status != URSATZ_OK || !response->json) {
        composer_response_destroy(response);
        return NULL;
    }
    return cJSON_Parse(response->json);
}

/*
 * Composes a theme in C <mode> as `listen` does: the random motive of the seed, analyzed, as the basic idea of a sentence of 8
 * bars, grown from the same seed. False when no theme could be grown.
 */
static inline bool compose_theme_in(const char *word, const char *into, unsigned seed, const char *preferences_json, Plan **plan) {
    char text[512];
    ComposerResponse response;
    snprintf(text, sizeof(text), "{\"material\": \"%s\", \"meter\": \"4/4\", \"strategy\": \"segments\", \"count\": 1, \"seed\": \"%u\"}", word, seed);
    cJSON *draw_request = cJSON_Parse(text);
    if (preferences_json) {
        cJSON_AddItemToObject(draw_request, "preferences", cJSON_Parse(preferences_json));
    }
    char *draw_text = cJSON_PrintUnformatted(draw_request);
    cJSON_Delete(draw_request);
    cJSON *drawn = call_json(composer_api_random_motive, draw_text, &response);
    free(draw_text);
    assert(drawn);
    composer_response_destroy(&response);
    cJSON *motive = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(drawn, "motives"), 0), "motive");
    assert(motive);
    cJSON *analyze = cJSON_CreateObject();
    cJSON *input = cJSON_AddObjectToObject(analyze, "input");
    char *motive_text = cJSON_PrintUnformatted(motive);
    assert(cJSON_AddStringToObject(input, "format", "json") && cJSON_AddStringToObject(input, "text", motive_text));
    cJSON_free(motive_text);
    char *analyze_text = cJSON_PrintUnformatted(analyze);
    cJSON *document = call_json(composer_api_motive, analyze_text, &response);
    free(analyze_text);
    cJSON_Delete(analyze);
    assert(document);
    composer_response_destroy(&response);
    const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(document, "analysis");
    const cJSON *entry_motive = cJSON_GetObjectItemCaseSensitive(document, "motive");

    cJSON *request = fixture_json("theme/one-motive.json");
    cJSON_ReplaceItemInObject(request, "key", cJSON_CreateString("C"));
    cJSON_ReplaceItemInObject(request, "into", cJSON_CreateString(into));
    cJSON_DeleteItemFromObject(request, "mode");
    cJSON_AddStringToObject(request, "material", word);
    cJSON_AddStringToObject(request, "development", "balanced");
    char number[16];
    snprintf(number, sizeof(number), "%u", seed);
    cJSON_AddStringToObject(request, "seed", number);
    cJSON_AddStringToObject(request, "suggest", "1");
    if (preferences_json) {
        cJSON_AddItemToObject(request, "preferences", cJSON_Parse(preferences_json));
    }
    cJSON *entry = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(request, "motives"), 0);
    cJSON_ReplaceItemInObject(entry, "degrees", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(analysis, "degrees_text"), true));
    cJSON_ReplaceItemInObject(entry, "pickup", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(analysis, "pickup"), true));
    const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(entry_motive, "harmony");
    cJSON_ReplaceItemInObject(entry, "harmony", cJSON_IsString(harmony) && harmony->valuestring[0] ? cJSON_Duplicate(harmony, true) : cJSON_CreateNull());
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(entry_motive, "features");
    cJSON_ReplaceItemInObject(entry, "features", cJSON_IsArray(features) ? cJSON_Duplicate(features, true) : cJSON_CreateArray());
    cJSON_Delete(document);
    cJSON_Delete(drawn);

    char *theme_text = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    memset(&response, 0, sizeof(response));
    UrsatzStatus status = composer_api_theme(test_context(), theme_text, NULL, &response);
    free(theme_text);
    bool composed = status == URSATZ_OK && response.plan_json != NULL;
    if (composed) {
        PlanJsonError error;
        assert(plan_from_json(test_knowledge(), response.plan_json, composer_plan_read_hooks(), plan, &error) == PLAN_OK);
    }
    composer_response_destroy(&response);
    return composed;
}


static inline bool compose_theme_with(const char *word, unsigned seed, const char *preferences_json, Plan **plan) {
    return compose_theme_in(word, "sentence", seed, preferences_json, plan);
}

static inline bool compose_theme(const char *word, unsigned seed, Plan **plan) {
    return compose_theme_with(word, seed, NULL, plan);
}

#endif
