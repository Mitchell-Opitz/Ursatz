#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <cJSON.h>
#include <ursatz_composer/preferences.h>

/*
 * The preferences document (R-411): the defaults written and read back, the melody part of the document in the shape
 * docs/preferences.md gives, members read over the defaults, and every melody limit refused with its path.
 */

static const char *DEFAULT_MELODY =
    "{\"range\":{\"low\":60,\"high\":84},\"nct_types\":[\"passing\",\"neighbor\"],\"density\":0,\"climax_position\":667,"
    "\"climax_rise\":4,\"climax_slack\":150,\"second_nearest_chord_tone\":\"1/3\",\"attempts\":48,\"keep\":6,"
    "\"plain_shaping\":false,\"weights\":{\"melodic\":1,\"against_bass\":1,\"tendency\":1,\"climax\":1,\"contour\":1,"
    "\"ending_reduction\":1,\"waves\":1,\"central_range\":1,\"monotony\":1,\"seam\":1}}";

static bool read_text(const char *text, ComposerPreferences *out, char *path, char *message) {
    cJSON *object = cJSON_Parse(text);
    assert(object);
    bool ok = composer_preferences_read(object, out, path, COMPOSER_PREFERENCES_PATH_MAX, message,
                                        COMPOSER_PREFERENCES_MESSAGE_MAX);
    cJSON_Delete(object);
    return ok;
}

/* The text is refused at path, with a message. */
static void refused(const char *text, const char *path) {
    ComposerPreferences preferences;
    char found[COMPOSER_PREFERENCES_PATH_MAX];
    char message[COMPOSER_PREFERENCES_MESSAGE_MAX];
    if (read_text(text, &preferences, found, message) || strcmp(found, path) != 0 || message[0] == '\0') {
        fprintf(stderr, "%s: expected a refusal at %s, got path \"%s\" message \"%s\"\n", text, path, found, message);
        assert(false);
    }
    /* A refusal leaves the preferences usable: the defaults with whatever was read before it. */
    assert(preferences.melody.attempts >= 1);
}

static void defaults_round_trip(void) {
    ComposerPreferences defaults;
    ComposerPreferences read;
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    char message[COMPOSER_PREFERENCES_MESSAGE_MAX];
    composer_preferences_default(&defaults);
    assert(composer_preferences_is_default(&defaults));

    cJSON *document = composer_preferences_write(&defaults);
    assert(document);
    char *text = cJSON_PrintUnformatted(cJSON_GetObjectItemCaseSensitive(document, "melody"));
    assert(text && strcmp(text, DEFAULT_MELODY) == 0);
    cJSON_free(text);

    assert(composer_preferences_read(document, &read, path, sizeof(path), message, sizeof(message)));
    assert(composer_preferences_is_default(&read));
    assert(read.melody.range_low == defaults.melody.range_low && read.melody.keep == defaults.melody.keep &&
           read.melody.nct_types == defaults.melody.nct_types);
    cJSON *again = composer_preferences_write(&read);
    char *first = cJSON_PrintUnformatted(document);
    char *second = cJSON_PrintUnformatted(again);
    assert(first && second && strcmp(first, second) == 0);
    cJSON_free(first);
    cJSON_free(second);
    cJSON_Delete(again);
    cJSON_Delete(document);

    /* The defaults are the constants the stage used before the settings existed. */
    assert(defaults.melody.range_low == 60 && defaults.melody.range_high == 84 && defaults.melody.climax_position == 667);
    assert(defaults.melody.climax_rise == 4 && defaults.melody.climax_slack == 150);
    assert(defaults.melody.second_nearest.numerator == 1 && defaults.melody.second_nearest.denominator == 3);
    assert(defaults.melody.attempts == 48 && defaults.melody.keep == 6 && defaults.melody.weights.seam == 1);
}

static void members_read_over_defaults(void) {
    ComposerPreferences preferences;
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    char message[COMPOSER_PREFERENCES_MESSAGE_MAX];
    assert(read_text("{}", &preferences, path, message) && composer_preferences_is_default(&preferences));
    assert(read_text("{\"melody\":{}}", &preferences, path, message) && composer_preferences_is_default(&preferences));
    assert(read_text("{\"melody\":{\"keep\":3,\"attempts\":12,\"range\":{\"high\":90},\"density\":2,"
                     "\"nct_types\":[\"suspension\",\"passing\"],\"second_nearest_chord_tone\":\"1/2\","
                     "\"plain_shaping\":true,\"weights\":{\"waves\":0,\"central_range\":20}}}",
                     &preferences, path, message));
    assert(!composer_preferences_is_default(&preferences));
    assert(preferences.melody.keep == 3 && preferences.melody.attempts == 12 && preferences.melody.density == 2);
    assert(preferences.melody.range_low == 60 && preferences.melody.range_high == 90);
    assert(preferences.melody.nct_types == (MELODY_NCT_SUSPENSION | MELODY_NCT_PASSING));
    assert(preferences.melody.second_nearest.numerator == 1 && preferences.melody.second_nearest.denominator == 2);
    assert(preferences.melody.plain_shaping && preferences.melody.weights.waves == 0 &&
           preferences.melody.weights.central_range == 20 && preferences.melody.weights.melodic == 1);

    /* What is read is written back as read. */
    cJSON *document = composer_preferences_write(&preferences);
    ComposerPreferences again;
    assert(document && composer_preferences_read(document, &again, path, sizeof(path), message, sizeof(message)));
    assert(again.melody.nct_types == preferences.melody.nct_types && again.melody.keep == 3 &&
           again.melody.weights.waves == 0 && again.melody.range_high == 90);
    cJSON_Delete(document);

    /* A single setting equal to its default is still the defaults. */
    assert(read_text("{\"melody\":{\"keep\":6}}", &preferences, path, message) && composer_preferences_is_default(&preferences));
}

static void limits_are_refused(void) {
    refused("{\"melody\":{\"keep\":17}}", "/melody/keep");
    refused("{\"melody\":{\"keep\":0}}", "/melody/keep");
    refused("{\"melody\":{\"attempts\":4,\"keep\":5}}", "/melody/keep");
    refused("{\"melody\":{\"keep\":5,\"attempts\":4}}", "/melody/keep");
    refused("{\"melody\":{\"attempts\":257}}", "/melody/attempts");
    refused("{\"melody\":{\"keep\":2.5}}", "/melody/keep");
    refused("{\"melody\":{\"keep\":\"6\"}}", "/melody/keep");
    refused("{\"melody\":{\"range\":{\"high\":71}}}", "/melody/range/high");
    refused("{\"melody\":{\"range\":{\"low\":80}}}", "/melody/range/high");
    refused("{\"melody\":{\"range\":{\"low\":35}}}", "/melody/range/low");
    refused("{\"melody\":{\"range\":{\"low\":97,\"high\":108}}}", "/melody/range/low");
    refused("{\"melody\":{\"range\":{\"high\":109}}}", "/melody/range/high");
    refused("{\"melody\":{\"range\":{\"mid\":70}}}", "/melody/range/mid");
    refused("{\"melody\":{\"nct_types\":[\"passing\",\"escape\"]}}", "/melody/nct_types");
    refused("{\"melody\":{\"nct_types\":[\"passing\",\"passing\"]}}", "/melody/nct_types");
    refused("{\"melody\":{\"nct_types\":\"passing\"}}", "/melody/nct_types");
    refused("{\"melody\":{\"second_nearest_chord_tone\":\"4/3\"}}", "/melody/second_nearest_chord_tone");
    refused("{\"melody\":{\"second_nearest_chord_tone\":\"1/17\"}}", "/melody/second_nearest_chord_tone");
    refused("{\"melody\":{\"second_nearest_chord_tone\":\"1/0\"}}", "/melody/second_nearest_chord_tone");
    refused("{\"melody\":{\"second_nearest_chord_tone\":\"one third\"}}", "/melody/second_nearest_chord_tone");
    refused("{\"melody\":{\"density\":5}}", "/melody/density");
    refused("{\"melody\":{\"climax_position\":1001}}", "/melody/climax_position");
    refused("{\"melody\":{\"climax_slack\":1001}}", "/melody/climax_slack");
    refused("{\"melody\":{\"climax_rise\":0}}", "/melody/climax_rise");
    refused("{\"melody\":{\"climax_rise\":13}}", "/melody/climax_rise");
    refused("{\"melody\":{\"plain_shaping\":1}}", "/melody/plain_shaping");
    refused("{\"melody\":{\"weights\":{\"waves\":21}}}", "/melody/weights/waves");
    refused("{\"melody\":{\"weights\":{\"waves\":-1}}}", "/melody/weights/waves");
    refused("{\"melody\":{\"weights\":{\"bogus\":1}}}", "/melody/weights/bogus");
    refused("{\"melody\":{\"bogus\":1}}", "/melody/bogus");
    refused("{\"melody\":5}", "/melody");
    refused("{\"bogus\":{}}", "/bogus");

    ComposerPreferences preferences;
    cJSON *list = cJSON_CreateArray();
    char path[COMPOSER_PREFERENCES_PATH_MAX];
    assert(!composer_preferences_read(list, &preferences, path, sizeof(path), NULL, 0) && path[0] == '\0');
    assert(!composer_preferences_read(NULL, &preferences, NULL, 0, NULL, 0));
    cJSON_Delete(list);
}

int main(void) {
    defaults_round_trip();
    members_read_over_defaults();
    limits_are_refused();
    printf("test_preferences: ok\n");
    return 0;
}
