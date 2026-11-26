#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/preferences.h>

#include "support/test_composer.h"
#include "support/test_knowledge.h"
#include "support/test_support.h"
#include "ursatz_test.h"

/*
 * The presets (P69c B5, R-754): ff7 and hollow-knight each read over the defaults, in the order and with the labels the vocabulary gives; a
 * request's own settings apply over a preset's and keep the rest of it; a theme request with preset ff7, the u51 motif and development
 * blueprint composes on seed 1 with the preset's accompaniment; a preset that is no preset is refused at /preset.
 */

static void test_presets_read(void) {
    assert(composer_preset_count() == 2);
    assert(strcmp(composer_preset_name(0), "ff7") == 0 && strcmp(composer_preset_label(0), "Final Fantasy VII") == 0);
    assert(strcmp(composer_preset_name(1), "hollow-knight") == 0 && strcmp(composer_preset_label(1), "Hollow Knight") == 0);
    assert(composer_preset_name(2) == NULL && composer_preset_source(0) && composer_preset_source(1));
    ComposerPreferences defaults;
    composer_preferences_default(&defaults);
    for (size_t i = 0; i < 2; i++) {
        ComposerPreferences read;
        char pattern[64];
        char message[256];
        composer_preferences_default(&read);
        assert(composer_preset_apply(composer_preset_name(i), &read, pattern, sizeof pattern, message, sizeof message));
        assert(!test_preferences_are_default(&read) && read.melody.cadence_long);
        assert(strncmp(pattern, "michelle.pattern.arpeggio-", 25) == 0);
    }
    ComposerPreferences ff7;
    ComposerPreferences hollow;
    char pattern[64];
    char message[256];
    composer_preferences_default(&ff7);
    composer_preferences_default(&hollow);
    assert(composer_preset_apply("ff7", &ff7, pattern, sizeof pattern, message, sizeof message) && strcmp(pattern, "michelle.pattern.arpeggio-eight") == 0);
    assert(composer_preset_apply("hollow-knight", &hollow, pattern, sizeof pattern, message, sizeof message) &&
           strcmp(pattern, "michelle.pattern.arpeggio-wave") == 0);
    assert(ff7.blueprint.sounds[BLUEPRINT_SOUND_NINTH_DOWNBEAT] == 6 && ff7.blueprint.climax[0] == 0 && ff7.blueprint.climax[1] == 1);
    assert(hollow.blueprint.sounds[BLUEPRINT_SOUND_LEADING_TONE] == 0 && hollow.blueprint.sounds[BLUEPRINT_SOUND_MAJOR_SEVENTH] == 6);
    assert(!composer_preset_apply("nothing", &ff7, pattern, sizeof pattern, message, sizeof message) && pattern[0] == '\0' &&
           strstr(message, "is not a preset"));
}

/* The preset with {"blueprint": {"climax": {"m6": 2}}} over it keeps the rest of the preset and takes m6. */
static void test_layering(void) {
    ComposerPreferences preferences;
    char pattern[64];
    char message[256];
    char path[128];
    composer_preferences_default(&preferences);
    assert(composer_preset_apply("ff7", &preferences, pattern, sizeof pattern, message, sizeof message));
    cJSON *own = cJSON_Parse("{\"blueprint\": {\"climax\": {\"m6\": 2}}}");
    assert(own && composer_preferences_apply(own, &preferences, path, sizeof path, message, sizeof message));
    cJSON_Delete(own);
    assert(preferences.blueprint.climax[0] == 2 && preferences.blueprint.climax[1] == 1);
    assert(preferences.blueprint.sounds[BLUEPRINT_SOUND_NINTH_DOWNBEAT] == 6 && preferences.blueprint.sounds[BLUEPRINT_SOUND_LONG_WEAK] == 2);
    assert(preferences.melody.cadence_long && preferences.motif.avoid_repeats);
}

/* --- through the API ----------------------------------------------------------------------------- */

static const char *const U51 =
    "{\"schema\": \"ursatz-theme-request\", \"schema_version\": 2, \"into\": \"sentence\", \"key\": \"C\", \"material\": \"minor\","
    " \"meter\": \"4/4\", \"bars\": 8, \"development\": \"blueprint\", \"seed\": 1, %s"
    " \"motives\": [{\"id\": \"M1\", \"form\": \"full\", \"degrees\": \"5:q. 2:e 1:h 4:q. 3:e 2:h\", \"pickup\": \"0\","
    " \"harmony\": \"I IV@4 V@6\", \"features\": [], \"role\": \"melodic\"}], \"slots\": {\"basic\": \"M1\"}}";

static void run(const char *extra, UrsatzStatus expected, ComposerResponse *response) {
    char request[1500];
    snprintf(request, sizeof request, U51, extra);
    assert(composer_api_theme(test_context(), request, NULL, response) == expected);
}

static void test_theme_request(void) {
    ComposerResponse response;
    run("\"preset\": \"ff7\",", URSATZ_OK, &response);
    /* Written for test_api_schema: the request with `preset` and its response with the brief's `preset`. */
    char request[1500];
    snprintf(request, sizeof request, U51, "\"preset\": \"ff7\",");
    test_write_text("api_theme_request_preset.json", request);
    test_write_text("api_theme_response_preset.json", response.json);
    cJSON *document = cJSON_Parse(response.json);
    assert(document);
    const cJSON *brief = cJSON_GetObjectItemCaseSensitive(document, "brief");
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(brief, "preset")->valuestring, "ff7") == 0);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(brief, "development")->valuestring, "blueprint") == 0);
    assert(response.plan_json && strstr(response.plan_json, "michelle.pattern.arpeggio-eight"));
    cJSON_Delete(document);
    composer_response_destroy(&response);

    /* The request's own accompaniment wins over the preset's, and its preferences apply over the preset's. */
    run("\"preset\": \"ff7\", \"accompaniment\": {\"pattern\": \"michelle.pattern.arpeggio-wave\"},"
        " \"preferences\": {\"blueprint\": {\"climax\": {\"m6\": 2}}},",
        URSATZ_OK, &response);
    assert(response.plan_json && strstr(response.plan_json, "michelle.pattern.arpeggio-wave") && !strstr(response.plan_json, "arpeggio-eight"));
    composer_response_destroy(&response);

    run("\"preset\": \"nothing\",", URSATZ_ERR_REQUEST, &response);
    assert(strcmp(response.error.path, "/preset") == 0 && strstr(response.error.message, "'nothing' is not a preset; see the vocabulary's presets"));
    composer_response_destroy(&response);
    run("\"preset\": 3,", URSATZ_ERR_REQUEST, &response);
    assert(strcmp(response.error.path, "/preset") == 0);
    composer_response_destroy(&response);
}

int main(void) {
    test_presets_read();
    test_layering();
    test_theme_request();
    printf("test_presets: ok\n");
    return 0;
}
